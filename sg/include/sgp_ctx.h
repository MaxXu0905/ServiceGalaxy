#if !defined(__SGP_CTX_H__)
#define __SGP_CTX_H__

#include <string>
#include <vector>
#include <boost/thread/once.hpp>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include "sg_struct.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

enum sg_ctx_state_t
{
	CTX_STATE_UNINIT = 0,
	CTX_STATE_SINGLE = 1,
	CTX_STATE_MULTI = 2
};

struct sgp_ctx
{
public:
	static sgp_ctx * instance();
	~sgp_ctx();

	void setenv();
	void set_bba(int index);

	int create_ctx();
	int destroy_ctx(int context);
	bool valid_ctx(int context);

	bool putenv(const std::string& filename, const sg_mchent_t& mchent);
	void setids(sg_proc_t& proc);

	bool admin_grp(int grpid) const { return grpid > DFLT_PGID; }
	bool admin_name(const char *svc_name) const { return svc_name[0] == '.'; }
	bool int_name(const char *svc_name) const { return (svc_name[0] == '.' && svc_name[1] == '.'); }

	bool do_auth(int perm, int ipckey, std::string& passwd, bool verify);
	bool encrypt(const std::string& password, std::string& encrypted_password);
	void clear();

	int _SGP_boot_flags;			// 启动标识
	int _SGP_shutdown;			// 进程是否退出状态
	bool _SGP_shutdown_due_to_sigterm;	// 程序是否由于收到信号退出
	bool _SGP_shutdown_due_to_sigterm_called;

	sg_ste_t _SGP_ste;				// STE节点的拷贝
	sg_qte_t _SGP_qte;				// QTE节点的拷贝
	sgid_t _SGP_ssgid;				// STE节点的SGID
	sgid_t _SGP_qsgid;				// QTE节点的SGID

	bool _SGP_svrinit_called;		// svrinit()被正确调用

	sg_svrent_t _SGP_svrent;		// server entry for current server
	sg_proc_t _SGP_svrproc; 		// 当前进程

	int _SGP_svridmin;				// 启动的最小svrid
	bool _SGP_stdmain_booted;		// 是否通知sgboot
	int _SGP_boot_sync;

	std::string _SGP_stdmain_ofile;
	std::string _SGP_stdmain_efile;

	std::vector<std::string> _SGP_adv_svcs;	// 服务列表
	bool _SGP_adv_all;						// 发布所有服务
	sg_svcdsp_t *_SGP_svcdsp;				// 服务对象静态列表
	int _SGP_svcidx;						// 最后一次调用的svcidx
	int _SGP_grpid;					// 当前进程grpid
	int _SGP_svrid;					// 当前进程svrid
	int _SGP_threads;				// 当前启动的线程数，服务端有效
	bool _SGP_let_svc;				// internal flag

	bool _SGP_amserver;
	bool _SGP_ambbm;
	bool _SGP_amdbbm;
	bool _SGP_amgws;
	bool _SGP_ambsgws;
	bool _SGP_amsgboot;
	pid_t _SGP_oldpid;			// 重启时原进程pid

	int _SGP_ctx_state;							// SGC状态
	std::vector<sgc_pointer> _SGP_ctx_array;	// SGC context数组
	int _SGP_ctx_count;							// SGC有效个数，不包括SGSINGLECONTEXT
	boost::recursive_mutex _SGP_ctx_mutex;		// 访问SGC时的锁
	long _SGP_netload;
	int _SGP_dbbm_resp_timeout;

	// For sg_message memory control
	int _SGP_max_cached_msgs;
	int _SGP_max_keep_size;

	// BB switches
	sg_bbasw *_SGP_bbasw_mgr;
	sg_ldbsw *_SGP_ldbsw_mgr;
	sg_authsw *_SGP_authsw_mgr;
	sg_switch *_SGP_switch_mgr;

	// Used for debug
	int _SGP_debug_level_lower;
	int _SGP_debug_level_upper;

	// qaddr与消息队列对象之间的映射，需要考虑版本
	// 当应用删除消息队列，又创建一个新的时，需要重新创建对象
	boost::unordered_map<sg_queue_key_t, sg_queue_info_t> _SGP_queue_map;
	bool _SGP_queue_warned;
	time_t _SGP_queue_expire;
	bi::interprocess_upgradable_mutex _SGP_queue_mutex;

private:
	sgp_ctx();

	void bbfail();
	static void init_once();

	static boost::once_flag once_flag;
	static sgp_ctx *_instance;
};

}
}

#endif

