#include "dbc_internal.h"

namespace ai
{
namespace sg
{

const char *TE_STAT_NAMES[TE_TOTAL_STAT] = { "S", "SR", "I", "U", "UR", "D", "DR" };

dbc_te_t::dbc_te_t()
{
}

dbc_te_t::dbc_te_t(const dbc_te_t& rhs)
{
	memcpy(this, &rhs, sizeof(dbc_te_t));
}

dbc_te_t& dbc_te_t::operator=(const dbc_te_t& rhs)
{
	memcpy(this, &rhs, sizeof(dbc_te_t));
	return *this;
}

void dbc_te_t::set_flags(int flags_)
{
	flags |= flags_;
}

void dbc_te_t::clear_flags(int flags_)
{
	flags &= ~flags_;
}

bool dbc_te_t::in_flags(int flags_) const
{
	return (flags & flags_);
}

bool dbc_te_t::in_persist() const
{
	return ((flags & TE_IN_USE) && !(flags & (TE_IN_TEMPORARY | TE_IN_MEM)));
}

bool dbc_te_t::in_use() const
{
	return ((flags & TE_IN_USE) && !(flags & TE_IN_TEMPORARY));
}

bool dbc_te_t::in_active() const
{
	return ((flags & TE_IN_USE) && !(flags & (TE_IN_TEMPORARY | TE_MARK_REMOVED)));
}

dbc_te& dbc_te::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<dbc_te *>(DBCT->_DBCT_mgr_array[DBC_TE_MANAGER]);
}

bool dbc_te::init(dbc_te_t *te, const dbc_te_t& data, const vector<dbc_ie_t>& index_vector)
{
	int i;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("table_id={1}") % data.table_id).str(SGLOCALE), &retval);
#endif

 	if (index_vector.size() > MAX_INDEXES) {
		DBCT->_DBCT_error_code = DBCEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: index count exceeds MAX_INDEXES")).str(SGLOCALE));
		return retval;
 	}

	bool has_primary = false;
	BOOST_FOREACH(const dbc_ie_t& ie, index_vector) {
		if (ie.ie_meta.index_type & INDEX_TYPE_PRIMARY) {
			has_primary = true;
			break;
		}
	}

	te->table_id = data.table_id;
	te->version = 0;
	te->flags = TE_IN_USE;

	if (te->table_id >= TE_MIN_RESERVED)
		te->set_flags(TE_IN_MEM | TE_SYS_RESERVED);

	if (te->table_id == TE_DUAL) {
		te->set_flags(TE_READONLY);
	} else if (!has_primary) {
		GPP->write_log(__FILE__, __LINE__, (_("WARN: Table {1} is set readonly since primary key is not defined") % data.te_meta.table_name).str(SGLOCALE));
		te->set_flags(TE_READONLY);
	}

	if (data.te_meta.refresh_type & REFRESH_TYPE_IN_MEM)
		te->set_flags(TE_IN_MEM);

	for (i = 0; i < data.te_meta.field_size; i++) {
		if (data.te_meta.fields[i].field_type == SQLTYPE_VSTRING) {
			te->set_flags(TE_VARIABLE);
			break;
		}
	}

	te->max_index = data.max_index;
	te->cur_count = data.cur_count;
	(void)new (&te->rwlock) bi::interprocess_upgradable_mutex();
	te->te_meta = data.te_meta;
	te->mem_used = -1;
	te->row_used = -1;

	for (i = 0; i < MAX_VSLOTS; i++) {
		(void)new (&te->mutex_free[i]) bi::interprocess_mutex();
		te->row_free[i] = -1;
	}

	(void)new (&te->mutex_ref) bi::interprocess_mutex();
	te->row_ref = -1;

	for (i = 0; i < MAX_INDEXES; i++)
		te->index_hash[i] = -1;

	for (i = 0; i < TE_TOTAL_STAT; i++)
		te->stat_array[i].init();

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	int index_id = 0;
	dbc_data_t& te_meta = te->te_meta;

	for (vector<dbc_ie_t>::const_iterator iter = index_vector.begin(); iter != index_vector.end(); ++iter) {
		long offset = bbp->alloc(sizeof(dbc_ie_t) + iter->ie_meta.buckets * sizeof(row_bucket_t));
		if (offset == -1) {
			DBCT->_DBCT_error_code = DBCEOS;
			DBCT->_DBCT_native_code = USBRK;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't allocate rows for table {1}") % te_meta.table_name).str(SGLOCALE));
			return retval;
		}

		dbc_ie_t *ie = reinterpret_cast<dbc_ie_t *>(reinterpret_cast<char *>(bbp) + offset);
		*ie = *iter;
		te->index_hash[index_id++] = offset;
	}

	// 参考SDC的配置来调整partitions，取两者中大的
	sdc_config& config_mgr = sdc_config::instance(DBCT);
	int partitions = config_mgr.get_partitions(te->table_id);
	if (te_meta.partitions < partitions)
		te_meta.partitions = partitions;

	// 如果需要分区，则需要检查是否提供分区字段
	if (te_meta.partitions > 1) {
		bool found = false;

		for (i = 0; i < te_meta.field_size; i++) {
			const dbc_data_field_t& field = te_meta.fields[i];

			if (field.is_partition) {
				found = true;
				break;
			}
		}

		if (!found) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Missing partition field for table {1}") % te_meta.table_name).str(SGLOCALE));
			return retval;
		}
	}

	retval = true;
	return retval;
}

void dbc_te::init_rows(long offset, int row_size, int slot, int extra_size)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	row_link_t *row_link;
	dbc_data_t& te_meta = te->te_meta;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("offset={1}") % offset).str(SGLOCALE), NULL);
#endif

	// Add to mem_used list
	long block_offset = offset - sizeof(dbc_mem_block_t);
	dbc_mem_block_t *mem_block = bbp->long2block(block_offset);
	mem_block->prev = -1;
	mem_block->next = te->mem_used;
	if (te->mem_used != -1) {
		dbc_mem_block_t *root_block = bbp->long2block(te->mem_used);
		root_block->prev = block_offset;
	}

	long rowid = offset;
	for (long i = 0; i < te_meta.segment_rows; i++) {
		row_link = DBCT->rowid2link(rowid);
		row_link->flags = ROW_IN_USE;
		row_link->ref_count = 0;
		row_link->slot = slot;
		row_link->extra_size = extra_size;
		row_link->accword = 0;
		row_link->lock_level = 0;
		row_link->wait_level = 0;
		row_link->rte_id = -1;
		row_link->prev = rowid - row_size;
		rowid += row_size;
		row_link->next = rowid;
	}

	row_link = DBCT->rowid2link(offset);
	row_link->prev = -1;
	row_link = DBCT->rowid2link(offset + row_size * (te_meta.segment_rows - 1));
	row_link->next = -1;

	te->mem_used = block_offset;
	te->row_free[slot] = offset;
}

void dbc_te::set_ctx(dbc_te_t * te_)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("table_id={1}") % te_->table_id).str(SGLOCALE), NULL);
#endif

	te = te_;
	DBCT->_DBCT_dbc_switch = DBCP->_DBCP_once_switch.get_switch(te->table_id);
}

dbc_te_t * dbc_te::get_te()
{
	return te;
}

dbc_te_t * dbc_te::find_te(const string& table_name)
{
	dbc_te_t *retval = NULL;
#if defined(DEBUG)
	scoped_debug<dbc_te_t *> debug(50, __PRETTY_FUNCTION__, (_("table_name={1}") % table_name).str(SGLOCALE), &retval);
#endif
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	dbc_te_t *te;
	for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
		if (!te->in_active())
			continue;

		// 不是系统表，而且不是当前用户的表
		if ((te->user_id & UE_ID_MASK) && te->user_id != DBCT->_DBCT_user_id)
			continue;

		dbc_data_t& te_meta = te->te_meta;
		if (strcasecmp(te_meta.table_name, table_name.c_str()) == 0) {
			retval = te;
			return retval;
		}
	}

	return retval;
}

long dbc_te::create_row(int extra_size)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(50, __PRETTY_FUNCTION__, "", &retval);
#endif

	int flags;
	if (!te->in_flags(TE_VARIABLE))
		flags = 0;
	else
		flags = ROW_VARIABLE;

	// 从空闲列表中获取记录，并填充数据
	row_link_t *link = get_row_link(extra_size, flags);
	if (link == NULL)
		return retval;

	long rowid = DBCT->link2rowid(link);

	// 把记录加入到数据链表头
	bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);
	te->cur_count++;
	link->next = te->row_used;
	if (te->row_used != -1) {
		row_link_t *first = DBCT->rowid2link(te->row_used);
		first->prev = rowid;
	}
	te->row_used = rowid;

	retval = rowid;
	return retval;
}

// 创建一行记录，data为外部格式
long dbc_te::create_row(const void *data)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(50, __PRETTY_FUNCTION__, (_("data={1}") % data).str(SGLOCALE), &retval);
#endif
	dbc_data_t& te_meta = te->te_meta;

	int flags = ROW_TRAN_INSERT;
	int extra_size;
	if (!te->in_flags(TE_VARIABLE)) {
		extra_size = 0;
	} else if (DBCT->_DBCT_skip_marshal) {
		flags |= ROW_VARIABLE;
		extra_size = DBCT->_DBCT_insert_size - te_meta.internal_size;
	} else {
		flags |= ROW_VARIABLE;
		extra_size = get_vsize(data);
	}

	// 从空闲列表中获取记录，并填充数据
	row_link_t *link = get_row_link(extra_size, flags);
	if (link == NULL)
		return retval;

	long rowid = DBCT->link2rowid(link);
	char *row_data = link->data();
	if (!te->in_flags(TE_VARIABLE))
		memcpy(row_data, data, te_meta.struct_size);
	else if (DBCT->_DBCT_skip_marshal)
		memcpy(row_data, data, DBCT->_DBCT_insert_size);
	else
		marshal(row_data, data);

	// 把记录加入到数据链表头
	bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);
	te->cur_count++;
	link->next = te->row_used;
	if (te->row_used != -1) {
		row_link_t *first = DBCT->rowid2link(te->row_used);
		first->prev = rowid;
	}
	te->row_used = rowid;

	retval = rowid;
	return retval;
}

bool dbc_te::erase_row(long rowid)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("rowid={1}") % rowid).str(SGLOCALE), &retval);
#endif

	row_link_t *link = DBCT->rowid2link(rowid);

	{
		// 把记录从数据链表中删除
		bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);
		te->cur_count--;

		if (link->prev != -1) {
			row_link_t *prev = DBCT->rowid2link(link->prev);
			prev->next = link->next;
		} else {
			// 链表头记录
			te->row_used = link->next;
		}

		if (link->next != -1) {
			row_link_t *next = DBCT->rowid2link(link->next);
			next->prev = link->prev;
		}
	}

	// 释放节点到未使用链表或引用链表中
	// 在删除节点之前，需要保证不会有会话增加引用计数
	if (link->dec_ref() == 0)
		put_row_link(link, te->row_free[link->slot], te->mutex_free[link->slot]);
	else
		put_row_link(link, te->row_ref, te->mutex_ref);

	link->unlock(DBCT);

	retval = true;
	return retval;
}

bool dbc_te::find_row(const void *data)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("data={1}") % data).str(SGLOCALE), &retval);
#endif
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	const void *data_buf;

	if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal || data == NULL)
		data_buf = data;
	else
		data_buf = te_mgr.marshal(data);

	bi::sharable_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

	if (te->row_used == -1) {
		retval = true;
		return retval;
	}

	dbc_switch_t *dbc_switch = DBCT->_DBCT_dbc_switch;
	row_link_t *link = DBCT->rowid2link(te->row_used);

	while (1) {
		char *row_data = link->data();
		int equal_flag;
		if (DBCT->_DBCT_where_func) {
			char *global[3];
			global[0] = row_data;
			global[1] = DBCT->_DBCT_data_start;
			global[2] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);
			equal_flag = DBCT->execute_where_func(global);
		} else {
			equal_flag = (*dbc_switch->equal)(data_buf, row_data);
		}

		if (equal_flag == 0) {
			long rowid = DBCT->data2rowid(row_data);
			if (DBCT->row_visible(link)) {
				if (DBCT->rows_enough())
					break;
				link->inc_ref();
				DBCT->_DBCT_rowids->push_back(rowid);
			}
		}

		if (link->next == -1) {
			retval = true;
			return retval;
		}

		link = DBCT->rowid2link(link->next);
	}

	retval = true;
	return retval;
}

bool dbc_te::find_row(long rowid)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("rowid={1}") % rowid).str(SGLOCALE), &retval);
#endif
	bi::sharable_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

	if (!check_rowid(rowid)) {
		DBCT->_DBCT_error_code = DBCEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: staled rowid {1} given.") % rowid).str(SGLOCALE));
		return retval;
	}

	row_link_t *link = DBCT->rowid2link(rowid);
	char *row_data = link->data();
	int equal_flag;
	if (DBCT->_DBCT_where_func) {
		char *global[3];
		global[0] = row_data;
		global[1] = DBCT->_DBCT_data_start;
		global[2] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);
		equal_flag = DBCT->execute_where_func(global);
	} else {
		equal_flag = 0;
	}

	if (equal_flag == 0) {
		if (DBCT->row_visible(link)) {
			if (!DBCT->rows_enough()) {
				link->inc_ref();
				DBCT->_DBCT_rowids->push_back(rowid);
			}
		}
	}

	retval = true;
	return retval;
}

void dbc_te::ref_row(long rowid)
{
	row_link_t *link = DBCT->rowid2link(rowid);
	link->inc_ref();
}

void dbc_te::unref_row(long rowid)
{
	row_link_t *link = DBCT->rowid2link(rowid);
	link->dec_ref();
}

bool dbc_te::lock_row(long rowid)
{
	row_link_t *link = DBCT->rowid2link(rowid);
	return lock_row(link);
}

bool dbc_te::lock_row(row_link_t *link)
{
	scoped_usignal sig;
	while (!link->lock(DBCT)) {
		if (DBCT->_DBCT_error_code != DBCGOTSIG)
			return false;

		// 检查事务是否超时
		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		if (rte_mgr.in_flags(RTE_ABORT_ONLY)) {
			DBCT->_DBCT_error_code = DBCETIME;
			return false;
		}
	}

	return true;
}

void dbc_te::unlock_row(long rowid)
{
	row_link_t *link = DBCT->rowid2link(rowid);
	unlock_row(link);
}

void dbc_te::unlock_row(row_link_t *link)
{
	link->unlock(DBCT);
}

bool dbc_te::create_row_indexes(long rowid)
{
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("rowid={1}") % rowid).str(SGLOCALE), &retval);
#endif

	for (int i = 0; i <= te->max_index; i++) {
		if (te->index_hash[i] == -1)
			continue;

		ie_mgr.set_ctx(get_index_internal(i));
		if (!ie_mgr.create_row(rowid)) {
			for (int j = i - 1; j >= 0; j--) {
				if (te->index_hash[j] == -1)
					continue;

				ie_mgr.set_ctx(get_index_internal(j));
				ie_mgr.erase_row(rowid);
			}

			return retval;
		}
	}

	retval = true;
	return retval;
}

void dbc_te::erase_row_indexes(long rowid)
{
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("rowid={1}") % rowid).str(SGLOCALE), NULL);
#endif

	for (int i = 0; i <= te->max_index; i++) {
		if (te->index_hash[i] == -1)
			continue;

		ie_mgr.set_ctx(get_index_internal(i));
		ie_mgr.erase_row(rowid);
	}
}

// 该函数需要在set_data()之后才能调用
dbc_ie_t * dbc_te::get_index(int index_id)
{
	if (index_id >= MAX_INDEXES || te->index_hash[index_id] == -1)
		return NULL;

	return get_index_internal(index_id);
}

void dbc_te::mark_ref()
{
#if defined(DEBUG)
		scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif

	bi::scoped_lock<bi::interprocess_mutex> lock(te->mutex_ref);

	row_link_t *link = DBCT->rowid2link(te->row_ref);
	while (1) {
		link->flags |= ROW_MARK_REMOVED;

		if (link->next == -1)
			break;

		link = DBCT->rowid2link(link->next);
	}
}

void dbc_te::revoke_ref()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif

	bi::scoped_lock<bi::interprocess_mutex> lock(te->mutex_ref);

	row_link_t *link = DBCT->rowid2link(te->row_ref);
	while (1) {
		long link_next = link->next;

		if (link->flags & ROW_MARK_REMOVED) {
			long link_offset = DBCT->link2rowid(link);
			if (link_offset == te->row_ref)
				te->row_ref = link->next;

			if (link->prev != -1) {
				row_link_t *prev = DBCT->rowid2link(link->prev);
				prev->next = -1;
			}

			if (link->next != -1) {
				row_link_t *next = DBCT->rowid2link(link->next);
				next->prev = -1;
			}

			link->prev = -1;

			// 在获取另一个锁时，需要先释放当前锁，防止死锁
			lock.unlock();

			{
				bi::scoped_lock<bi::interprocess_mutex> free_lock(te->mutex_free[link->slot]);

				long& row_free = te->row_free[link->slot];
				link->next = row_free;
				if (row_free == -1) {
					row_link_t *first = DBCT->rowid2link(row_free);
					first->prev = -1;
				}
			}

			// 重新获取锁后，列表可能有变化，需要重新开始
			lock.lock();
			link = DBCT->rowid2link(te->row_ref);
		}

		if (link_next == -1)
			break;

		link = DBCT->rowid2link(link_next);
	}
}

void dbc_te::free_mem()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;

	long mem_used = te->mem_used;
	while (mem_used != -1) {
		long mem_prev = mem_used;
		dbc_mem_block_t *mem_block = bbp->long2block(mem_used);
		mem_used = mem_block->next;
		bbp->free(mem_prev);
	}
	te->mem_used = -1;
}

bool dbc_te::check_rowid(long rowid)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("rowid={1}") % rowid).str(SGLOCALE), &retval);
#endif
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	bi::sharable_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

	long mem_used = te->mem_used;
	while (mem_used != -1) {
		dbc_mem_block_t *mem_block = bbp->long2block(mem_used);
		if (mem_used + sizeof(dbc_mem_block_t) <= rowid && mem_used + sizeof(dbc_mem_block_t) + mem_block->size > rowid) {
			retval = true;
			return retval;
		}
		mem_used = mem_block->next;
	}

	return retval;
}

// 获取记录的变长部分长度
int dbc_te::get_vsize(const void *data)
{
	const dbc_data_t& te_meta = te->te_meta;
	int vsize = 0;

	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];

		if (field.field_type != SQLTYPE_VSTRING)
			continue;

		vsize += strlen(reinterpret_cast<const char *>(data) + field.field_offset) + 1;
	}

	return vsize;
}

// 外部格式数据转换成内部格式数据
const char * dbc_te::marshal(const void *src)
{
	const char *retval = NULL;
#if defined(DEBUG)
	scoped_debug<const char *> debug(50, __PRETTY_FUNCTION__, (_("src={1}") % src).str(SGLOCALE), &retval);
#endif
	const dbc_data_t& te_meta = te->te_meta;
	int max_size = te_meta.internal_size + MAX_VSLOTS * sizeof(long);
	if (DBCT->_DBCT_data_buf_size < max_size) {
		DBCT->_DBCT_data_buf_size = max_size;
		delete []DBCT->_DBCT_data_buf;
		DBCT->_DBCT_data_buf = new char[DBCT->_DBCT_data_buf_size];
	}

	char *tail = reinterpret_cast<char *>(DBCT->_DBCT_data_buf) + te_meta.internal_size;
	int offset = 0;

	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];
		const char *src_field = reinterpret_cast<const char *>(src) + field.field_offset;
		char *dst_field = reinterpret_cast<char *>(DBCT->_DBCT_data_buf) + field.internal_offset;

		if (field.field_type != SQLTYPE_VSTRING) {
			memcpy(dst_field, src_field, field.field_size);
		} else {
			int size = strlen(src_field) + 1;

			*reinterpret_cast<int *>(dst_field) = offset;
			memcpy(tail, src_field, size);
			offset += size;
			tail += size;
		}
	}

	retval = DBCT->_DBCT_data_buf;
	return retval;
}

// 外部格式数据转换成内部格式数据
void dbc_te::marshal(void *dst, const void *src)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("dst={1}, src={2}") % dst % src).str(SGLOCALE), NULL);
#endif
	const dbc_data_t& te_meta = te->te_meta;
	char *tail = reinterpret_cast<char *>(dst) + te_meta.internal_size;
	int offset = 0;

	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];
		const char *src_field = reinterpret_cast<const char *>(src) + field.field_offset;
		char *dst_field = reinterpret_cast<char *>(dst) + field.internal_offset;

		if (field.field_type != SQLTYPE_VSTRING) {
			memcpy(dst_field, src_field, field.field_size);
		} else {
			int size = strlen(src_field) + 1;

			*reinterpret_cast<int *>(dst_field) = offset;
			memcpy(tail, src_field, size);
			offset += size;
			tail += size;
		}
	}
}


// 内部格式数据转换成外部格式数据
void dbc_te::unmarshal(void *dst, const void *src)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("dst={1}, src={2}") % dst % src).str(SGLOCALE), NULL);
#endif
	const dbc_data_t& te_meta = te->te_meta;
	const char *tail = reinterpret_cast<const char *>(src) + te_meta.internal_size;

	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];
		const char *src_field = reinterpret_cast<const char *>(src) + field.internal_offset;
		char *dst_field = reinterpret_cast<char *>(dst) + field.field_offset;

		if (field.field_type != SQLTYPE_VSTRING) {
			memcpy(dst_field, src_field, field.field_size);
		} else {
			const int& offset = *reinterpret_cast<const int *>(src_field);
			strcpy(dst_field, tail + offset);
		}
	}
}

int dbc_te::primary_key()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, "", &retval);
#endif

	for (int i = 0; i <= te->max_index; i++) {
		dbc_ie_t *ie = get_index(i);
		if (ie != NULL && (ie->ie_meta.index_type & INDEX_TYPE_PRIMARY)) {
			retval = i;
			return retval;
		}
	}

	return retval;
}

dbc_te::dbc_te()
{
}

dbc_te::~dbc_te()
{
}

// 从链表头获取一个节点
row_link_t * dbc_te::get_row_link(int extra_size, int flags)
{
	dbc_data_t& te_meta = te->te_meta;
	int data_size = common::round_up(te_meta.internal_size + extra_size, STRUCT_ALIGN);
	int row_size = data_size + SYS_ROW_SIZE;
	int slot = (data_size - te_meta.internal_size) / STRUCT_ALIGN;
	row_link_t *link;

	{
		bi::scoped_lock<bi::interprocess_mutex> lock(te->mutex_free[slot]);
		long& row_free = te->row_free[slot];

		if (row_free == -1) { // No free node left
			// alloc from mem block
			dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
			long offset = bbp->alloc(te_meta.segment_rows * row_size);
			if (offset == -1) {
				DBCT->_DBCT_error_code = DBCEOS;
				DBCT->_DBCT_native_code = USBRK;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't allocate rows for table {1}") % te_meta.table_name).str(SGLOCALE));
				return NULL;
			}

			init_rows(offset, row_size, slot, extra_size);
		}

		link = DBCT->rowid2link(row_free);
		row_free = link->next;
		if (row_free != -1) { // 链表非空
			row_link_t *first_link = DBCT->rowid2link(row_free);
			first_link->prev = -1;
		}
	}

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();

	link->flags = ROW_IN_USE | flags;
	link->ref_count = 1;
	link->accword = 0;
	link->lock_level = 1;
	link->wait_level = 0;
	link->rte_id = rte->rte_id;
	link->prev = -1;
	return link;
}

// 把当前节点插入到链表头
void dbc_te::put_row_link(row_link_t *link, long& head, bi::interprocess_mutex& mutex)
{
	long link_offset = DBCT->link2rowid(link);

	link->flags = 0;
	link->prev = -1;

	bi::scoped_lock<bi::interprocess_mutex> lock(mutex);

	link->next = head;
	if (head != -1) { // 列表非空
		row_link_t *first_link = DBCT->rowid2link(head);
		first_link->prev = link_offset;
	}
	head = link_offset;
}

dbc_ie_t * dbc_te::get_index_internal(int index_id)
{
	return reinterpret_cast<dbc_ie_t *>(reinterpret_cast<char *>(DBCT->_DBCT_bbp) + te->index_hash[index_id]);
}

}
}

