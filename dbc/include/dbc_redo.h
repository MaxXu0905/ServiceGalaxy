#if !defined(__DBC_REDO_H__)
#define __DBC_REDO_H__

#include "dbc_struct.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

int const REDO_ROWS_PER_BUCKET = 1000;
int const REDO_INDEX_MASK = 0x0000FFFF;
int const REDO_FLAG_MASK = 0xFFFF0000;
int const REDO_IN_USE = 0x10000;
int const REDO_MARK_REMOVED = 0x20000;

struct redo_row_t
{
	long left;
	long right;
	long parent;
	color_t color;

	// 数据区
	int user_idx;
	int table_id;
	int flags;
	int row_size;
	int old_row_size;
	long rowid;
	long old_rowid;
	long sequence;

	long compare(const redo_row_t& rhs) const;
	long compare_s(const redo_row_t& rhs) const;
	redo_row_t& operator=(const redo_row_t& rhs);
};

struct redo_prow_t
{
	// 数据区
	const redo_row_t *redo_row;

	bool operator<(const redo_prow_t& rhs) const;
};

// 注意:第一个字段必须为redo_rows
struct redo_bucket_t
{
	redo_row_t redo_rows[REDO_ROWS_PER_BUCKET];
	int freed;
	int used;
	int flags;
	pid_t pid;
	long prev;
	long next;
};

struct dbc_redo_t
{
	int accword;
	long per_buckets;	// buckets per allocation
	long mem_used;	// allocated memory list head
	long redo_free;	// redo free list head
};

class dbc_redo : public dbc_manager
{
public:
	static dbc_redo& instance(dbct_ctx *DBCT);

	void init();
	redo_bucket_t * get_bucket();
	void put_bucket(redo_bucket_t *redo_bucket);
	void free_buckets(long& head_offset);
	redo_bucket_t * row2bucket(redo_row_t *redo_row);
	void free_mem();

private:
	dbc_redo();
	virtual ~dbc_redo();

	void init_rows(long offset);

	friend class dbct_ctx;
};

}
}

#endif

