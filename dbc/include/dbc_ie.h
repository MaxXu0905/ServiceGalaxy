#if !defined(__DBC_IE_H__)
#define __DBC_IE_H__

#include "dbc_struct.h"
#include "dbc_te.h"
#include "dbc_stat.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

int const IE_FIND_STAT = 0;
int const IE_FIND_HASH_STAT = 1;
int const IE_INSERT_STAT = 2;
int const IE_UPDATE_STAT = 3;
int const IE_UPDATE_HASH_STAT = 4;
int const IE_ERASE_STAT = 5;
int const IE_ERASE_HASH_STAT = 6;
int const IE_TOTAL_STAT = 7;

extern const char *IE_STAT_NAMES[IE_TOTAL_STAT];

// 索引定义
struct dbc_ie_t
{
	int index_id;	// 索引ID
	bi::interprocess_upgradable_mutex lock;	// 索引读写锁
	int node_accword;	// 修改索引的自旋锁
	dbc_index_t ie_meta;	// 元数据定义
	long mem_used;	// 使用的内存块首地址
	long node_free;	// 未使用索引节点偏移量
	dbc_stat_t stat_array[IE_TOTAL_STAT];

	dbc_ie_t();
	dbc_ie_t(const dbc_ie_t& rhs);
	dbc_ie_t& operator=(const dbc_ie_t& rhs);
};

class dbc_ie : public dbc_manager
{
public:
	static dbc_ie& instance(dbct_ctx *DBCT);

	void init();
	// 初始化数据区
	void init_inode(long offset);
	void init_snode(long offset);
	void set_ctx(dbc_ie_t *ie_);
	// 根据给定的记录rowid，创建该记录的索引
	bool create_row(long rowid);
	// 根据给定的记录rowid，删除该记录的索引
	void erase_row(long rowid);
	// 根据索引快速查找符合条件的记录
	void find_row(const void *data);
	dbc_ie_t * get_ie();

	dbc_inode_t * get_inode();
	void put_inode(dbc_inode_t *inode);
	dbc_snode_t * get_snode();
	void put_snode(dbc_snode_t *inode);
	void free_mem();

private:
	dbc_ie();
	virtual ~dbc_ie();

	row_bucket_t * begin_bucket();
	size_t get_row_hash(const char *key);

	dbc_ie_t *ie;

	friend class dbct_ctx;
};

}
}

#endif

