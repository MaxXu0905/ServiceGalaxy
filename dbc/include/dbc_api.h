#if !defined(__DBC_API_H__)
#define __DBC_API_H__

#include "sql_control.h"
#include "compiler.h"
#include "dbc_internal.h"

namespace ai
{
namespace sg
{

#if !defined(__DBCT_UPDATE_FUNC_DEFINED__)
#define __DBCT_UPDATE_FUNC_DEFINED__
typedef void (*DBCC_UPDATE_FUNC)(void *result, const void *old_data, const void *new_data);
#endif

class cursor_compare
{
public:
	cursor_compare(char *data_start_, compiler::func_type order_func_);

	bool operator()(long left, long right) const;

private:
	char *data_start;
	compiler::func_type order_func;
};

class dbc_cursor
{
public:
	~dbc_cursor();

	bool next();
	bool prev();
	bool status();
	bool seek(int cursor_);
	const char * data() const;
	long rowid() const;
	long size() const;
	void order(compiler::func_type order_func);

protected:
	dbc_cursor(dbct_ctx *DBCT_, vector<long> *rowids_, dbc_te_t *te_);

private:
	dbct_ctx *DBCT;
	vector<long> *rowids;
	dbc_te_t *te;
	int cursor;
	char *unmarshal_buf;

	friend class dbc_api;
};

class dbc_api : public dbc_manager
{
public:
	static dbc_api& instance();
	static dbc_api& instance(dbct_ctx *DBCT);

	// 登录到DBC
	bool login();
	// 从DBC注销
	void logout();
	// 连接DBC
	bool connect(const string& user_name, const string& password);
	// 是否已经连接
	bool connected() const;
	// 从DBC断开连接
	void disconnect();
	// 设置用户ID
	bool set_user(int64_t user_id);
	// 获取用户ID
	int64_t get_user() const;
	// 设置注册节点上下文
	void set_ctx(dbc_rte_t *rte);

	// 根据表名获取表ID
	int get_table(const string& table_name);
	// 根据表名、索引名获取表ID
	int get_index(const string& table_name, const string& index_name);
	// 设置最大返回行数
	void set_rownum(long rownum);
	// 获取当前设置的最大返回行数
	long get_rownum() const;
	// 全表查找记录
	long find(int table_id, const void *data, void *result, long max_rows = 1);
	// 按索引查找记录
	long find(int table_id, int index_id, const void *data, void *result, long max_rows = 1);
	// 全表打开游标
	auto_ptr<dbc_cursor> find(int table_id, const void *data);
	// 按索引打开游标
	auto_ptr<dbc_cursor> find(int table_id, int index_id, const void *data);
	// 按rowid查找记录
	long find_by_rowid(int table_id, long rowid, void *result, long max_rows = 1);
	// 按rowid打开游标
	auto_ptr<dbc_cursor> find_by_rowid(int table_id, long rowid);
	// 全表删除记录
	long erase(int table_id, const void *data);
	// 按索引删除记录
	long erase(int table_id, int index_id, const void *data);
	// 按rowid删除记录
	bool erase_by_rowid(int table_id, long rowid);
	// 插入记录
	bool insert(int table_id, const void *data);
	// 全表更新记录
	long update(int table_id, const void *old_data, const void *new_data);
	// 按索引更新记录
	long update(int table_id, int index_id, const void *old_data, const void *new_data);
	// 按rowid更新记录
	long update_by_rowid(int table_id, long rowid, const void *new_data);
	// 获取rowid
	long get_rowid(int seq_no);
	// 获取序列的当前值
	long get_currval(const string& seq_name);
	// 获取序列的下一值
	long get_nextval(const string& seq_name);
	// 预提交
	bool precommit();
	// 二阶段提交
	bool postcommit();
	// 提交
	bool commit();
	// 回滚
	bool rollback();
	// 当前表的记录数
	size_t size(int table_id);
	// 当前表的外部结构字节数
	int struct_size(int table_id);
	// 设置自动提交
	bool set_autocommit(bool autocommit);
	// 获取自动提交标识
	bool get_autocommit();
	// 设置超时
	bool set_timeout(long timeout);
	// 获取超时
	long get_timeout();

	// 以下函数由内部使用，提供SQL方式的操作
	// 设置更新函数
	void set_update_func(DBCC_UPDATE_FUNC update_func);
	// 清除更新函数
	void clear_update_func();
	// 设置条件函数
	void set_where_func(compiler::func_type where_func);
	// 清除条件函数
	void clear_where_func();

	// 清空表
	bool truncate(const string& table_name);
	// 创建表
	bool create(dbc_te_t& dbc_te_);
	// 删除表
	bool drop(const string& table_name);
	// 创建索引
	bool create_index(const dbc_ie_t& ie_node);
	// 创建序列
	bool create_sequence(const dbc_se_t& se_node);
	// 变更序列
	bool alter_sequence(dbc_se_t *se);
	// 删除序列
	bool drop_sequence(const string& seq_name);

	// 获取错误代码
	int get_error_code() const;
	// 获取本地错误代码
	int get_native_code() const;
	// 获取错误消息
	const char * strerror() const;
	// 获取本地错误消息
	const char * strnative() const;
	// 获取用户自定义错误代码
	int get_ur_code() const;
	// 设置用户自定义错误代码
	void set_ur_code(int ur_code);

	// 获取partition_id
	static int get_partition_id(int partition_type, int partitions, const char *key);

private:
	dbc_api();
	virtual ~dbc_api();

	bool erase_by_rowid_internal(int table_id, long rowid);
	bool insert(int table_id, const void *data, long old_rowid);
	void erase_undo(int table_id, long rowid, long redo_sequence);
	void insert_undo(int table_id, long rowid, long redo_sequence);
	void commit_save(const redo_row_t *root, set<redo_prow_t>& row_set);
	bool commit_internal(redo_row_t *root);
	bool rollback_internal(redo_row_t *root);
	bool commit(bool do_save);
	void recover(int table_id, long redo_sequence);
	void clone_data(const char *data);

	static void logout_atexit();
	static int match_myself(void *client, char **global, const char **in, char **out);

	friend class dbc_server;
	friend class dbct_ctx;
};

}
}

#endif

