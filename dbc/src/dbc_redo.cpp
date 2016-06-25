#include "dbc_internal.h"

namespace ai
{
namespace sg
{

long redo_row_t::compare(const redo_row_t & rhs) const
{
	long ret = user_idx - rhs.user_idx;
	if (ret)
		return ret;

	ret = table_id - rhs.table_id;
	if (ret)
		return ret;

	ret = rowid - rhs.rowid;
	if (ret)
		return ret;

	return sequence - rhs.sequence;
}

long redo_row_t::compare_s(const redo_row_t & rhs) const
{
	long ret = user_idx - rhs.user_idx;
	if (ret)
		return ret;

	ret = table_id - rhs.table_id;
	if (ret)
		return ret;

	return rowid - rhs.rowid;
}

redo_row_t& redo_row_t::operator=(const redo_row_t& rhs)
{
	user_idx = rhs.user_idx;
	table_id = rhs.table_id;
	flags = rhs.flags;
	row_size = rhs.row_size;
	old_row_size = rhs.old_row_size;
	rowid = rhs.rowid;
	old_rowid = rhs.old_rowid;
	sequence = rhs.sequence;

	return *this;
}

bool redo_prow_t::operator<(const redo_prow_t& rhs) const
{
	return redo_row->sequence < rhs.redo_row->sequence;
}

dbc_redo& dbc_redo::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<dbc_redo *>(DBCT->_DBCT_mgr_array[DBC_REDO_MANAGER]);
}

void dbc_redo::init()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_redo_t *redo = DBCT->_DBCT_redo;

	redo->accword = 0;
	redo->per_buckets = (bbparms.segment_redos + REDO_ROWS_PER_BUCKET - 1) / REDO_ROWS_PER_BUCKET;
	redo->mem_used = -1;
	redo->redo_free = -1;
}

redo_bucket_t * dbc_redo::get_bucket()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_redo_t *redo = DBCT->_DBCT_redo;
	redo_bucket_t *retval = NULL;
#if defined(DEBUG)
	scoped_debug<redo_bucket_t *> debug(50, __PRETTY_FUNCTION__, "", &retval);
#endif

	ipc_sem::tas(&redo->accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&redo)) {
		ipc_sem::semclear(&redo->accword);
	} BOOST_SCOPE_EXIT_END

	if (redo->redo_free == -1) { // No free node left
		long offset = bbp->alloc(sizeof(redo_bucket_t) * redo->per_buckets);
		if (offset == -1) {
			DBCT->_DBCT_error_code = DBCEOS;
			DBCT->_DBCT_native_code = USBRK;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't allocate rows for redo.")).str(SGLOCALE));
			return retval;
		}

		init_rows(offset);
	}

	redo_bucket_t *redo_bucket = DBCT->long2redobucket(redo->redo_free);
	redo->redo_free = redo_bucket->next;
	if (redo->redo_free  != -1) { // 链表非空
		redo_bucket_t *free_head = DBCT->long2redobucket(redo->redo_free);
		free_head->prev = -1;
	}

	redo_bucket->used = 0;
	redo_bucket->flags = REDO_IN_USE;
	redo_bucket->pid = DBCP->_DBCP_current_pid;

	retval = redo_bucket;
	return retval;
}

void dbc_redo::put_bucket(redo_bucket_t *redo_bucket)
{
	dbc_redo_t *redo = DBCT->_DBCT_redo;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("redo_bucket={1}") % redo_bucket).str(SGLOCALE), NULL);
#endif

	ipc_sem::tas(&redo->accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&redo)) {
		ipc_sem::semclear(&redo->accword);
	} BOOST_SCOPE_EXIT_END

	long bucket_offset = DBCT->redobucket2long(redo_bucket);

	redo_bucket->prev = -1;
	redo_bucket->next = redo->redo_free;
	if (redo->redo_free != -1) { // 列表非空
		redo_bucket_t *free_head = DBCT->long2redobucket(redo->redo_free);
		free_head->prev = bucket_offset;
	}
	redo->redo_free = bucket_offset;
}

void dbc_redo::free_buckets(long& head_offset)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("head_offset={1}") % head_offset).str(SGLOCALE), NULL);
#endif

	if (head_offset == -1)
		return;

	dbc_redo_t *redo = DBCT->_DBCT_redo;

	redo_bucket_t *tail_bucket = DBCT->long2redobucket(head_offset);
	while (tail_bucket->next != -1)
		tail_bucket = DBCT->long2redobucket(tail_bucket->next);

	ipc_sem::tas(&redo->accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&redo)) {
		ipc_sem::semclear(&redo->accword);
	} BOOST_SCOPE_EXIT_END

	tail_bucket->next = redo->redo_free;
	if (redo->redo_free != -1) { // 列表非空
		redo_bucket_t *free_head = DBCT->long2redobucket(redo->redo_free);
		free_head->prev = DBCT->redobucket2long(tail_bucket);
	}
	redo->redo_free = head_offset;
	head_offset = -1;
}

redo_bucket_t * dbc_redo::row2bucket(redo_row_t *redo_row)
{
	int row_index = (redo_row->flags & REDO_INDEX_MASK);
	return reinterpret_cast<redo_bucket_t *>(redo_row - row_index);
}

void dbc_redo::free_mem()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_redo_t *redo = DBCT->_DBCT_redo;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif

	long mem_used = redo->mem_used;
	while (mem_used != -1) {
		long mem_prev = mem_used;
		dbc_mem_block_t *mem_block = bbp->long2block(mem_used);
		mem_used = mem_block->next;
		bbp->free(mem_prev);
	}
	redo->mem_used = -1;
}

dbc_redo::dbc_redo()
{
}

dbc_redo::~dbc_redo()
{
}

void dbc_redo::init_rows(long offset)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_redo_t *redo = DBCT->_DBCT_redo;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("offset={1}") % offset).str(SGLOCALE), NULL);
#endif

	// Add to mem_used list
	dbc_mem_block_t *mem_block = bbp->long2block(offset - sizeof(dbc_mem_block_t));
	mem_block->prev = -1;
	mem_block->next = redo->mem_used;
	if (redo->mem_used != -1) {
		dbc_mem_block_t *root_block = bbp->long2block(redo->mem_used);
		root_block->prev = offset - sizeof(dbc_mem_block_t);
	}
	redo->mem_used = offset - sizeof(dbc_mem_block_t);

	redo_bucket_t *redo_buckets = DBCT->long2redobucket(offset);
	redo_bucket_t *redo_bucket = redo_buckets;
	long bucket_offset = offset;
	for (long i = 0; i < redo->per_buckets; i ++) {
		redo_bucket->prev = bucket_offset - sizeof(redo_bucket_t);
		bucket_offset += sizeof(redo_bucket_t);
		redo_bucket->next = bucket_offset;

		for (int j = 0; j < REDO_ROWS_PER_BUCKET; j++)
			redo_bucket->redo_rows[j].flags = j;

		redo_bucket++;
	}

	redo_buckets[0].prev = -1;
	redo_buckets[redo->per_buckets - 1].next = -1;

	redo->redo_free = offset;
}

}
}

