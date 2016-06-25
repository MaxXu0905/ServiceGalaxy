#if !defined(__DBC_RTE_H__)
#define __DBC_RTE_H__

#include "dbc_struct.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

// 注册节点
struct dbc_rte_t
{
	short next;			// 下一节点
	int accword;		// 用来保护flags
	int flags;			// 使用情况标识
	int rte_id;			// 注册节点ID
	bool autocommit;	// 是否自动提交
 	bi::interprocess_mutex mutex;	// 当资源被占用时，等待的信号量
	bi::interprocess_condition cond;	// 当资源被占用时，等待的条件
	pid_t pid;			// 该节点归属的进程pid
	long timeout;
	long redo_full;		// 没有空闲节点的列表
	long redo_free;		// 有空闲节点的列表
	long redo_root;		// 重做树根
	int64_t tran_id;	// 事务ID
 	time_t rtimestamp;	// 事务开始时间点

	void reset_tran();
	void set_flags(int flags_);
	void clear_flags(int flags_);
	bool in_flags(int flags_) const;
};

class dbc_rte : public dbc_manager
{
public:
	static dbc_rte& instance(dbct_ctx *DBCT);

	void init();
	void reinit();
	void set_ctx(dbc_rte_t *rte_);
	dbc_rte_t * get_rte();
	dbc_rte_t * base();
	bool get_slot();
	void put_slot();
	bool set_tran();
	void reset_tran();
	void set_flags(int flags);
	void clear_flags(int flags);
	bool in_flags(int flags) const;
	void set_autocommit(bool autocommit);
	bool get_autocommit() const;
	void set_timeout(long timeout);
	long get_timeout() const;

private:
	dbc_rte();
	virtual ~dbc_rte();

	dbc_rte_t *rte;

	friend class dbct_ctx;
};

}
}

#endif

