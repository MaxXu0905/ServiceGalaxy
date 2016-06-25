#if !defined(__DBCP_CTX_H__)
#define __DBCP_CTX_H__

#include <string>
#include <vector>
#include <boost/thread/once.hpp>
#include <boost/thread.hpp>
#include <sqlite3.h>
#include "dbc_struct.h"
#include "compiler.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

class switch_lib
{
public:
	switch_lib();
	~switch_lib();

	dbc_switch_t * get_switch(int table_id);
	void set_switch(int table_id, dbc_switch_t *value);
	const std::string& get_md5sum();

private:
	bool load_lib();

	typedef dbc_switch_t ** (*get_switch_func)();
	typedef const char * (*get_md5sum_func)();

	void *handle;
	dbc_switch_t **dbc_switch;
	std::string md5sum;
	int bbversion;
};

int const DBC_SQLITE_LOAD = 1;		// ����ʱ�ָ����ļ�¼ʹ��
int const DBC_SQLITE_INSERT = 2;	// �����¼ʹ��
int const DBC_SQLITE_MOVE = 3;		// ͬ������Ǩ�Ƽ�¼ʹ��
int const DBC_SQLITE_SYNC = 4;		// ͬ������ͬ�����ⲿ���ݿ�ʹ��

struct dbc_sqlite_t
{
	sqlite3 *db;
	sqlite3_stmt *select_stmt;
	sqlite3_stmt *insert_stmt;
	sqlite3_stmt *delete_stmt;
};

struct dbcp_ctx
{
public:
	static dbcp_ctx * instance();
	~dbcp_ctx();

	bool get_config(const std::string& dbc_key, int dbc_version, const std::string& sdc_key, int sdc_version);
	void clear();
	bool get_mlist(sgt_ctx *SGT);

	int _DBCP_proc_type;
	std::string _DBCP_dbcconfig;
	std::string _DBCP_sdcconfig;

	void *_DBCP_attach_addr;
	int _DBCP_attached_count;
	int _DBCP_attached_clients;
	boost::mutex _DBCP_shm_mutex;

	switch_lib _DBCP_once_switch;

	// ��SQLite��صĳ־û�����
	dbc_sqlite_t _DBCP_sqlites[MAX_SQLITES];

	bool _DBCP_reuse;			// ʹ�ô��ڵĹ����ڴ�
	bool _DBCP_enable_sync;		// ��������dbc_sync
	bool _DBCP_is_server;		// ��ǰ������dbc_server
	bool _DBCP_is_sync;			// ��ǰ������dbc_sync
	bool _DBCP_is_admin;		// ��ǰ������dbc_admin
	pid_t _DBCP_current_pid;	// ��ǰ���̵�pid
	search_type_enum _DBCP_search_type;
	vector<pair<int, string> > _DBCP_mlist;	// Master�ڵ��б�

private:
	dbcp_ctx();

	static void init_once();

	static boost::once_flag once_flag;
	static dbcp_ctx *_instance;
};

}
}

#endif

