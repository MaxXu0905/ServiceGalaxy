#include "dbc_internal.h"

namespace ai
{
namespace sg
{

const char *IE_STAT_NAMES[IE_TOTAL_STAT] = { "S", "SH", "I", "U", "UH", "D", "DH" };

dbc_ie_t::dbc_ie_t()
{
}

dbc_ie_t::dbc_ie_t(const dbc_ie_t& rhs)
{
	memcpy(this, &rhs, sizeof(dbc_ie_t));
}

dbc_ie_t& dbc_ie_t::operator=(const dbc_ie_t& rhs)
{
	memcpy(this, &rhs, sizeof(dbc_ie_t));
	return *this;
}

dbc_ie& dbc_ie::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<dbc_ie *>(DBCT->_DBCT_mgr_array[DBC_IE_MANAGER]);
}

void dbc_ie::init()
{
	long i;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif

	// 必须先初始化以下变量
	(void)new (&ie->lock) bi::interprocess_upgradable_mutex();
	ie->node_accword = 0;
	ie->mem_used = -1;
	ie->node_free = -1;

	row_bucket_t *bucket_array = begin_bucket();
	dbc_index_t& ie_meta = ie->ie_meta;
	for (i = 0; i < ie_meta.buckets; i++)
		(void)new (&bucket_array[i]) row_bucket_t();

	for (i = 0; i < IE_TOTAL_STAT; i++)
		ie->stat_array[i].init();
}

void dbc_ie::init_inode(long offset)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("offset={1}") % offset).str(SGLOCALE), NULL);
#endif

	// Add to mem_used list
	dbc_mem_block_t *mem_block = bbp->long2block(offset - sizeof(dbc_mem_block_t));
	mem_block->prev = -1;
	mem_block->next = ie->mem_used;
	if (ie->mem_used != -1) {
		dbc_mem_block_t *root_block = bbp->long2block(ie->mem_used);
		root_block->prev = offset - sizeof(dbc_mem_block_t);
	}
	ie->mem_used = offset - sizeof(dbc_mem_block_t);

	// Initialize inodes
	dbc_inode_t *first_node = DBCT->long2inode(offset);
	dbc_index_t& ie_meta = ie->ie_meta;
	long rowid = offset;
	for (long i = 0; i < ie_meta.segment_rows; i ++) {
		first_node[i].left = rowid - sizeof(dbc_inode_t);
		rowid += sizeof(dbc_inode_t);
		first_node[i].right = rowid;
	}
	first_node[0].left = -1;
	first_node[ie_meta.segment_rows - 1].right = -1;

	ie->node_free = offset;
}

void dbc_ie::init_snode(long offset)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("offset={1}") % offset).str(SGLOCALE), NULL);
#endif

	// Add to mem_used list
	dbc_mem_block_t *mem_block = bbp->long2block(offset - sizeof(dbc_mem_block_t));
	mem_block->prev = -1;
	mem_block->next = ie->mem_used;
	if (ie->mem_used != -1) {
		dbc_mem_block_t *root_block = bbp->long2block(ie->mem_used);
		root_block->prev = offset - sizeof(dbc_mem_block_t);
	}
	ie->mem_used = offset - sizeof(dbc_mem_block_t);

	// Initialize inodes
	dbc_snode_t *first_node = DBCT->long2snode(offset);
	dbc_index_t& ie_meta = ie->ie_meta;
	long rowid = offset;
	for (long i = 0; i < ie_meta.segment_rows; i ++) {
		first_node[i].prev = rowid - sizeof(dbc_snode_t);
		rowid += sizeof(dbc_snode_t);
		first_node[i].next = rowid;
	}
	first_node[0].prev = -1;
	first_node[ie_meta.segment_rows - 1].next = -1;

	ie->node_free = offset;
}


void dbc_ie::set_ctx(dbc_ie_t *ie_)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("ie={1}") % ie_).str(SGLOCALE), NULL);
#endif

	ie = ie_;
}

bool dbc_ie::create_row(long rowid)
{
	char *data = DBCT->rowid2data(rowid);
	const dbc_index_switch_t& index_switch = DBCT->get_index_switches()[ie->index_id];
	dbc_index_t& ie_meta = ie->ie_meta;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("rowid={1}") % rowid).str(SGLOCALE), &retval);
#endif

	long hash_value;
	row_bucket_t *bucket_array = begin_bucket();
	if (!(ie_meta.method_type & METHOD_TYPE_HASH)) {
		hash_value = 0;
	} else {
		char key[MAX_HASH_SIZE + 1];
		(*index_switch.get_hash)(key, data);
		hash_value = get_row_hash(key);
	}
	row_bucket_t& bucket_node = bucket_array[hash_value];

	// 对hash插槽节点加锁，结束时自动解锁
	bi::scoped_lock<dbc_upgradable_mutex_t> lock(bucket_node.mutex);

	if (ie_meta.method_type & METHOD_TYPE_SEQ) {
		dbc_snode_t snode;
		snode.rowid = rowid;

		inode_list& inode_list_mgr = inode_list::instance(DBCT);
		if (ie_meta.index_type & INDEX_TYPE_RANGE)
			retval = (inode_list_mgr.insert(snode, bucket_node.root, index_switch.compare, bucket_node.mutex) != -1);
		else
			retval = (inode_list_mgr.insert(snode, bucket_node.root, DBCT->_DBCT_dbc_switch->equal, bucket_node.mutex) != -1);
	} else {
		dbc_inode_t inode;
		inode.rowid = rowid;

		inode_rbtree& inode_tree_mgr = inode_rbtree::instance(DBCT);
		retval = (inode_tree_mgr.insert(inode, bucket_node.root, index_switch.compare, bucket_node.mutex) != -1);
	}

	return retval;
}

void dbc_ie::erase_row(long rowid)
{
	char *data = DBCT->rowid2data(rowid);
	const dbc_index_switch_t& index_switch = DBCT->get_index_switches()[ie->index_id];
	dbc_index_t& ie_meta = ie->ie_meta;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("rowid={1}") % rowid).str(SGLOCALE), NULL);
#endif

	long hash_value;
	row_bucket_t *bucket_array = begin_bucket();
	if (!(ie_meta.method_type & METHOD_TYPE_HASH)) {
		hash_value = 0;
	} else {
		char key[MAX_HASH_SIZE + 1];
		(*index_switch.get_hash)(key, data);
		hash_value = get_row_hash(key);
	}
	row_bucket_t& bucket_node = bucket_array[hash_value];

	// 对hash插槽节点加锁，结束时自动解锁
	bi::scoped_lock<dbc_upgradable_mutex_t> lock(bucket_node.mutex);

	if (ie_meta.method_type & METHOD_TYPE_SEQ) {
		dbc_snode_t snode;
		snode.rowid = rowid;

		inode_list& inode_list_mgr = inode_list::instance(DBCT);
		inode_list_mgr.erase(snode, bucket_node.root);
	} else {
		dbc_inode_t inode;
		inode.rowid = rowid;

		inode_rbtree& inode_tree_mgr = inode_rbtree::instance(DBCT);
		inode_tree_mgr.erase(inode, bucket_node.root, index_switch.compare);
	}
}

void dbc_ie::find_row(const void *data)
{
	const dbc_index_switch_t& index_switch = DBCT->get_index_switches()[ie->index_id];
	dbc_index_t& ie_meta = ie->ie_meta;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("data={1}") % data).str(SGLOCALE), NULL);
#endif

	long hash_value;
	row_bucket_t *bucket_array = begin_bucket();
	if (!(ie_meta.method_type & METHOD_TYPE_HASH)) {
		hash_value = 0;
	} else {
		char key[MAX_HASH_SIZE + 1];
		(*index_switch.get_hash)(key, data);
		hash_value = get_row_hash(key);
	}
	row_bucket_t& bucket_node = bucket_array[hash_value];

	long rowid = DBCT->data2rowid(data);

	// 对hash插槽节点加锁，结束时自动解锁
	bi::sharable_lock<dbc_upgradable_mutex_t> lock(bucket_node.mutex);

	if (ie_meta.method_type & METHOD_TYPE_SEQ) {
		inode_list& inode_list_mgr = inode_list::instance(DBCT);
		if (DBCT->_DBCT_where_func == NULL) {
			dbc_snode_t snode;
			snode.rowid = rowid;

			inode_list_mgr.search(snode, bucket_node.root, index_switch.compare);
		} else {
			inode_list_mgr.search(bucket_node.root);
		}
	} else {
		dbc_inode_t inode;
		inode.rowid = rowid;

		inode_rbtree& inode_tree_mgr = inode_rbtree::instance(DBCT);
		if (DBCT->_DBCT_where_func == NULL)
			inode_tree_mgr.search(inode, bucket_node.root, index_switch.compare);
		else
			inode_tree_mgr.search(bucket_node.root);
	}
}

dbc_ie_t * dbc_ie::get_ie()
{
	return ie;
}

dbc_ie::dbc_ie()
{
}

dbc_ie::~dbc_ie()
{
}

row_bucket_t * dbc_ie::begin_bucket()
{
	return reinterpret_cast<row_bucket_t *>(reinterpret_cast<char *>(ie + 1));
}

dbc_inode_t * dbc_ie::get_inode()
{
	dbc_index_t& ie_meta = ie->ie_meta;
#if defined(DEBUG)
	scoped_debug<bool> debug(10, __PRETTY_FUNCTION__, "", NULL);
#endif
	ipc_sem::tas(&ie->node_accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&ie)) {
		ipc_sem::semclear(&ie->node_accword);
	} BOOST_SCOPE_EXIT_END

	if (ie->node_free == -1) { // No free node left
		// alloc from mem block
		dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
		long offset = bbp->alloc(ie_meta.segment_rows * sizeof(dbc_inode_t));
		if (offset == -1) {
			DBCT->_DBCT_error_code = DBCEOS;
			DBCT->_DBCT_native_code = USBRK;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't allocate rows for index {1}") % ie_meta.index_name).str(SGLOCALE));
			return NULL;
		}

		init_inode(offset);
	}

	dbc_inode_t *node = DBCT->long2inode(ie->node_free);
	ie->node_free = node->right;
	if (ie->node_free  != -1) { // 链表非空
		dbc_inode_t *first_node = DBCT->long2inode(ie->node_free);
		first_node->left = -1;
	}

	return node;
}

void dbc_ie::put_inode(dbc_inode_t *node)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(10, __PRETTY_FUNCTION__, (_("node={1}") % node).str(SGLOCALE), NULL);
#endif
	long node_offset = DBCT->inode2long(node);

	ipc_sem::tas(&ie->node_accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&ie)) {
		ipc_sem::semclear(&ie->node_accword);
	} BOOST_SCOPE_EXIT_END

	node->left = -1;
	node->right = ie->node_free;
	if (ie->node_free != -1) { // 空闲列表非空
		dbc_inode_t *first_node = DBCT->long2inode(ie->node_free);
		first_node->left = node_offset;
	}
	ie->node_free = node_offset;
}

dbc_snode_t * dbc_ie::get_snode()
{
	dbc_index_t& ie_meta = ie->ie_meta;
#if defined(DEBUG)
	scoped_debug<bool> debug(10, __PRETTY_FUNCTION__, "", NULL);
#endif
	ipc_sem::tas(&ie->node_accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&ie)) {
		ipc_sem::semclear(&ie->node_accword);
	} BOOST_SCOPE_EXIT_END

	if (ie->node_free == -1) { // No free node left
		// alloc from mem block
		dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
		long offset = bbp->alloc(ie_meta.segment_rows * sizeof(dbc_snode_t));
		if (offset == -1) {
			DBCT->_DBCT_error_code = DBCEOS;
			DBCT->_DBCT_native_code = USBRK;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't allocate rows for index {1}") % ie_meta.index_name).str(SGLOCALE));
			return NULL;
		}

		init_snode(offset);
	}

	dbc_snode_t *node = DBCT->long2snode(ie->node_free);
	ie->node_free = node->next;
	if (ie->node_free  != -1) { // 链表非空
		dbc_snode_t *first_node = DBCT->long2snode(ie->node_free);
		first_node->prev = -1;
	}

	return node;
}

void dbc_ie::put_snode(dbc_snode_t *node)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(10, __PRETTY_FUNCTION__, (_("node={1}") % node).str(SGLOCALE), NULL);
#endif
	long node_offset = DBCT->snode2long(node);

	ipc_sem::tas(&ie->node_accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&ie)) {
		ipc_sem::semclear(&ie->node_accword);
	} BOOST_SCOPE_EXIT_END

	node->prev = -1;
	node->next = ie->node_free;
	if (ie->node_free != -1) { // 空闲列表非空
		dbc_snode_t *first_node = DBCT->long2snode(ie->node_free);
		first_node->prev = node_offset;
	}
	ie->node_free = node_offset;
}

size_t dbc_ie::get_row_hash(const char *key)
{
	dbc_index_t& ie_meta = ie->ie_meta;
	long buckets = ie_meta.buckets;
	size_t retval = 0;
#if defined(DEBUG)
	scoped_debug<size_t> debug(50, __PRETTY_FUNCTION__, (_("key={1}") % key).str(SGLOCALE), &retval);
#endif

	if (ie_meta.method_type & METHOD_TYPE_NUMBER) {
		retval = static_cast<size_t>(::atol(key)) % buckets;
	} else {
		long len = strlen(key);
		retval = boost::hash_range(key, key + len) % buckets;
	}

	return retval;
}

void dbc_ie::free_mem()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif

	long mem_used = ie->mem_used;
	while (mem_used != -1) {
		long mem_prev = mem_used;
		dbc_mem_block_t *mem_block = bbp->long2block(mem_used);
		mem_used = mem_block->next;
		bbp->free(mem_prev);
	}
	ie->mem_used = -1;
	ie->node_free = -1;
}

}
}

