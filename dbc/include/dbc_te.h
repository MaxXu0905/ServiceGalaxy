#if !defined(__DBC_TE_H__)
#define __DBC_TE_H__

#include "dbc_struct.h"
#include "dbc_stat.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

int const TE_FIND_STAT = 0;
int const TE_FIND_ROWID_STAT = 1;
int const TE_INSERT_STAT = 2;
int const TE_UPDATE_STAT = 3;
int const TE_UPDATE_ROWID_STAT = 4;
int const TE_ERASE_STAT = 5;
int const TE_ERASE_ROWID_STAT = 6;
int const TE_TOTAL_STAT = 7;

extern const char *TE_STAT_NAMES[TE_TOTAL_STAT];

// 最大允许的变长字节数为MAX_VSLOTS * sizeof(long)
int const MAX_VSLOTS = 8192;

// 表定义
struct dbc_te_t
{
	int64_t user_id;// 用户ID
	int table_id;	// 表ID
	int version;	// 表版本，当dbc_server更新时调整
	int flags;		// 内部标识
	int max_index;	// 当前最大索引
	long cur_count;	// 当前记录数
	/*
	 * 表读写锁
	 * 当需要修改定义时(包括删除表、增加索引、删除索引等)，需要加写锁
	 * 对表中数据的查询、更改操作，需要加读锁
	 * 对内存区域做调整时需要加锁
	 */
	bi::interprocess_upgradable_mutex rwlock;	// 读写锁
	dbc_data_t te_meta;						// 元数据定义
	long mem_used;							// 使用的内存块首地址
	long row_used;							// 使用节点偏移量，相对于数据区开始位置
	bi::interprocess_mutex mutex_free[MAX_VSLOTS];	// 空闲节点锁
	bi::interprocess_mutex mutex_ref;				// 引用节点锁
	long row_free[MAX_VSLOTS];				// 空闲节点偏移量，相对于数据区开始位置
	long row_ref;							// 引用节点偏移量，相对于数据开始位置
	ihash_t index_hash[MAX_INDEXES];		// 索引定义数组结构
	dbc_stat_t stat_array[TE_TOTAL_STAT];	// 统计数组

	dbc_te_t();
	dbc_te_t(const dbc_te_t& rhs);
	dbc_te_t& operator=(const dbc_te_t& rhs);

	void set_flags(int flags_);
	void clear_flags(int flags_);
	bool in_flags(int flags_) const;
	bool in_persist() const;
	bool in_use() const;
	bool in_active() const;
};

class dbc_te : public dbc_manager
{
public:
	static dbc_te& instance(dbct_ctx *DBCT);

	bool init(dbc_te_t *te, const dbc_te_t& data, const vector<dbc_ie_t>& index_vector);
	// 初始化数据区
	void init_rows(long offset, int row_size, int slot, int extra_size);
	// 切换操作表上下文
	void set_ctx(dbc_te_t *te_);
	// 获取当前的TE结构
	dbc_te_t * get_te();
	// 根据表名，获取其TE结构
	dbc_te_t * find_te(const string& table_name);
	// 从数据区获取一个空闲节点，加入数据链表中
	long create_row(int extra_size);
	// 从数据区获取一个空闲节点，把数据填充后，加入数据链表中
	long create_row(const void *data);
	// 释放一个节点到数据区中
	bool erase_row(long rowid);
	// 遍历数据链表，查找符合条件的记录
	bool find_row(const void *data);
	// 根据rowid，获取符合条件的记录
	bool find_row(long rowid);
	// 对已存在的节点增加引用计数
	void ref_row(long rowid);
	// 对已存在的节点减少引用计数
	void unref_row(long rowid);
	// 对已经存在的节点加锁
	bool lock_row(long rowid);
	bool lock_row(row_link_t *link);
	// 对已经存在的节点解锁
	void unlock_row(long rowid);
	void unlock_row(row_link_t *link);
	// 创建rowid对应数据记录的所有索引
	bool create_row_indexes(long rowid);
	// 删除rowid对应数据记录的所有索引
	void erase_row_indexes(long rowid);
	dbc_ie_t * get_index(int index_id);
	void mark_ref();
	void revoke_ref();
	void free_mem();
	bool check_rowid(long rowid);
	int get_vsize(const void *data);
	const char * marshal(const void *src);
	void marshal(void *dst, const void *src);
	void unmarshal(void *dst, const void *src);
	int primary_key();

private:
	dbc_te();
	virtual ~dbc_te();

	row_link_t * get_row_link(int extra_size, int flags);
	void put_row_link(row_link_t *link, long& head, bi::interprocess_mutex& mutex);
	dbc_ie_t * get_index_internal(int index_id);

	dbc_te_t *te;	// 指向共享内存中的表定义节点

	friend class dbct_ctx;
};

}
}

#endif

