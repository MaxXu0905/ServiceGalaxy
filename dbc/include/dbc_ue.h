#if !defined(__DBC_UE_H__)
#define __DBC_UE_H__

#include "dbc_struct.h"
#include "dbc_stat.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

class dbc_user_t;

// 用户定义
struct dbc_ue_t
{
	char user_name[MAX_DBC_USER_NAME + 1];
	char password[MAX_DBC_PASSWORD + 1];
	char dbname[MAX_DBNAME + 1];		// 数据库名称
	char openinfo[MAX_OPENINFO + 1];	// 数据库连接参数
	int64_t user_id;// 用户ID
	int perm;		// 用户权限
	int flags;		// 内部标识
	time_t ctime;	// 创建时间
	time_t mtime;	// 更改时间
	long thash[MAX_DBC_ENTITIES];
	long shash[MAX_DBC_ENTITIES];

	void set_flags(int flags_);
	void clear_flags(int flags_);
	bool in_flags(int flags_) const;
	bool in_use() const;
	int get_user_idx() const;
};

class dbc_ue : public dbc_manager
{
public:
	static dbc_ue& instance(dbct_ctx *DBCT);

	// 获取当前的UE结构
	dbc_ue_t * get_ue();
	// 切换用户上下文
	void set_ctx(int user_idx);
	void set_ctx(dbc_ue_t *ue_);
	// 登录用户
	bool connect(const std::string& user_name, const std::string& password);
	// 注销用户
	void disconnect();
	// 获取连接状态
	bool connected() const;
	// 获取当前的TE结构
	dbc_te_t * get_table(int table_id);
	// 获取当前的SE结构
	dbc_se_t * get_seq(const std::string& seq_name);
	// 创建用户，成功则返回用户ID，失败返回-1
	int64_t create_user(const dbc_user_t& user);

private:
	dbc_ue();
	virtual ~dbc_ue();

	dbc_ue_t *ue;	// 指向共享内存中的用户定义节点

	friend class dbct_ctx;
};

}
}

#endif

