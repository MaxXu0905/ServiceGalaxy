#if !defined(__DBCT_CTX_H__)
#define __DBCT_CTX_H__

#include "dbc_struct.h"
#include "common.h"
#include "uunix.h"
#include "dbc_ue.h"
#include "dbc_te.h"
#include "dbc_ie.h"
#include "dbc_se.h"
#include "dbc_rte.h"
#include "dbc_redo.h"
#include "redo_rbtree.h"
#include "inode_rbtree.h"
#include "inode_list.h"
#include "dbc_sqlite.h"
#include "compiler.h"

namespace ai
{
namespace sg
{

struct dbc_user_t
{
	string user_name;
	string password;
	string dbname;
	string openinfo;
	int perm;
};

struct data_index_t
{
	dbc_te_t data;
	vector<dbc_ie_t> index_vector;
};

#if !defined(__DBCT_UPDATE_FUNC_DEFINED__)
#define __DBCT_UPDATE_FUNC_DEFINED__
typedef void (*DBCC_UPDATE_FUNC)(void *result, const void *old_data, const void *new_data);
#endif

struct seq_cache_t
{
	long currval;
	long cache;
	bool refetch;
};

class dbct_ctx
{
public:
	static dbct_ctx * instance();
	~dbct_ctx();

	bool create_shm();
	bool extend_shm(int add_count);
	bool attach_shm();
	void detach_shm(bool auto_close = true);

	void init_globals();
	int get_hash(const char *key, int buckets);
	dbc_index_switch_t * get_index_switches();
	bool changed(int table_id, long rowid);
	bool change_insert(int table_id, int row_size, int old_row_size, long rowid, long old_rowid);
	bool change_erase(int table_id, long rowid, long redo_sequence);
	void set_for_update();
	void clear_for_update();
	bool rows_enough() const;
	bool row_visible(const row_link_t *link);
	void set_update_func(DBCC_UPDATE_FUNC update_func);
	void set_where_func(compiler::func_type where_func);
	dbc_inode_t * long2inode(long offset);
	long inode2long(const dbc_inode_t *inode);
	dbc_snode_t * long2snode(long offset);
	long snode2long(const dbc_snode_t *snode);
	redo_bucket_t * long2redobucket(long offset);
	long redobucket2long(const redo_bucket_t *redo_bucket);
	redo_row_t * long2redorow(long offset);
	long redorow2long(const redo_row_t *redo_row);
	row_link_t * rowid2link(long rowid);
	long link2rowid(const row_link_t *link);
	char * rowid2data(long rowid);
	long data2rowid(const void *data);

	int execute_where_func(char **global);
	void enter_execute();

	const char * strerror() const;
	const char * strnative() const;

	void fixed2struct(char *src_buf, char *dst_buf, const dbc_data_t& te_meta);
	int fixed2record(char *src_buf, char *dst_buf, const dbc_data_t& te_meta);
	void variable2struct(const std::string& src_buf, char *dst_buf, const dbc_data_t& te_meta);
	int variable2record(const std::string& src_buf, char *dst_buf, const dbc_data_t& te_meta);
	string get_ready_file(const std::string& lmid);

	static const char * strerror(int error_code);
	static void bb_change(int signo);

	dbc_bboard_t *_DBCT_bbp;
	dbc_ue_t *_DBCT_ues;
	dbc_te_t *_DBCT_tes;
	dbc_se_t *_DBCT_ses;
	dbc_rte_t *_DBCT_rtes;
	dbc_redo_t *_DBCT_redo;

	bool _DBCT_login;					// 是否已经login
	int64_t _DBCT_user_id;				// 登录用户ID
	vector<long> *_DBCT_rowids;			// 数据记录rowid数组
	vector<long> _DBCT_rowids_buf;		// 默认的数据记录rowid数组
	vector<long> _DBCT_erase_rowids;	// 删除数据
	long _DBCT_rownum;					// 最大允许查询的记录数
	int _DBCT_find_flags;				// 查找时设置的标识

	char *_DBCT_data_start;
	dbc_switch_t *_DBCT_dbc_switch;

	time_t _DBCT_mark_ref_time;			// 标记引用记录可回收时间

	char *_DBCT_data_buf;
	int _DBCT_data_buf_size;
	char *_DBCT_row_buf;
	int _DBCT_row_buf_size;
	DBCC_UPDATE_FUNC _DBCT_update_func;
	compiler::func_type _DBCT_update_func_internal;
	compiler::func_type _DBCT_where_func;
	char **_DBCT_input_binds;
	int _DBCT_input_bind_count;
	char **_DBCT_set_binds;
	int _DBCT_set_bind_count;

	int _DBCT_bbversion;

	int _DBCT_error_code;
	int _DBCT_native_code;

	long _DBCT_redo_sequence;
	long _DBCT_timestamp;
	int _DBCT_seq_fd;	// fd for sequence.
	boost::unordered_map<string, seq_cache_t> _DBCT_seq_map;	// 已获取的序列号数组
	bool _DBCT_atexit_registered;

	compiler::func_type _DBCT_group_func;

	// 在SQL语句中使用
	long _DBCT_result_set_size;
	// 生成的字符串连接函数使用的局部变量
	vector<string> _DBCT_strcat_buf;
	int _DBCT_strcat_buf_index;		// strcat使用的索引
	bool _DBCT_internal_format;		// 是否使用内部格式
	int _DBCT_internal_size;		// 内部格式定长部分长度

	dbc_manager *_DBCT_mgr_array[MAX_MANAGERS];
	bool _DBCT_skip_marshal;		// 跳过marshal/unmarshal处理
	int _DBCT_insert_size;			// 插入记录长度

private:
	dbct_ctx();

	bool reserve_addr(const dbc_bbparms_t& config_bbparms);

	static boost::thread_specific_ptr<dbct_ctx> instance_;
};

}
}

#endif

