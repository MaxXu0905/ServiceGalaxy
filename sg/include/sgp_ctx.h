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

	int _SGP_boot_flags;			// ������ʶ
	int _SGP_shutdown;			// �����Ƿ��˳�״̬
	bool _SGP_shutdown_due_to_sigterm;	// �����Ƿ������յ��ź��˳�
	bool _SGP_shutdown_due_to_sigterm_called;

	sg_ste_t _SGP_ste;				// STE�ڵ�Ŀ���
	sg_qte_t _SGP_qte;				// QTE�ڵ�Ŀ���
	sgid_t _SGP_ssgid;				// STE�ڵ��SGID
	sgid_t _SGP_qsgid;				// QTE�ڵ��SGID

	bool _SGP_svrinit_called;		// svrinit()����ȷ����

	sg_svrent_t _SGP_svrent;		// server entry for current server
	sg_proc_t _SGP_svrproc; 		// ��ǰ����

	int _SGP_svridmin;				// ��������Сsvrid
	bool _SGP_stdmain_booted;		// �Ƿ�֪ͨsgboot
	int _SGP_boot_sync;

	std::string _SGP_stdmain_ofile;
	std::string _SGP_stdmain_efile;

	std::vector<std::string> _SGP_adv_svcs;	// �����б�
	bool _SGP_adv_all;						// �������з���
	sg_svcdsp_t *_SGP_svcdsp;				// �������̬�б�
	int _SGP_svcidx;						// ���һ�ε��õ�svcidx
	int _SGP_grpid;					// ��ǰ����grpid
	int _SGP_svrid;					// ��ǰ����svrid
	int _SGP_threads;				// ��ǰ�������߳������������Ч
	bool _SGP_let_svc;				// internal flag

	bool _SGP_amserver;
	bool _SGP_ambbm;
	bool _SGP_amdbbm;
	bool _SGP_amgws;
	bool _SGP_ambsgws;
	bool _SGP_amsgboot;
	pid_t _SGP_oldpid;			// ����ʱԭ����pid

	int _SGP_ctx_state;							// SGC״̬
	std::vector<sgc_pointer> _SGP_ctx_array;	// SGC context����
	int _SGP_ctx_count;							// SGC��Ч������������SGSINGLECONTEXT
	boost::recursive_mutex _SGP_ctx_mutex;		// ����SGCʱ����
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

	// qaddr����Ϣ���ж���֮���ӳ�䣬��Ҫ���ǰ汾
	// ��Ӧ��ɾ����Ϣ���У��ִ���һ���µ�ʱ����Ҫ���´�������
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

