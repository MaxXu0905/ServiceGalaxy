#if !defined(__SGT_CTX_H__)
#define __SGT_CTX_H__

#include "sg_message.h"
#include "sg_struct.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

int const MAX_MANAGERS = 32;

class sg_svrent_t;
class sg_ste_t;

class sgt_ctx
{
private:
	bool ctx_exiting;

public:
	static sgt_ctx * instance();
	~sgt_ctx();

	sgc_ctx * SGC();
	std::string formatopts(const sg_svrent_t& svrent, sg_ste_t *ste);
	char ** setargv(const char *aout, const std::string& clopt);
	bool exiting() const { return ctx_exiting; }
	void clear();
	// ����mid��qaddr�õ���Ϣ���ж����Ҳ���ʱ����ʧ�ܣ����׳��쳣
	queue_pointer get_queue(int mid, int qaddr);
	const char * strerror() const;
	const char * strnative() const;

	static const char * strerror(int error_code);

	pthread_t _SGT_thrid;

	bool _SGT_thrinit_called;						// thrinit()����ȷ����
	std::vector<message_pointer> _SGT_cached_msgs;	// �������Ϣ
	sg_scte_t *_SGT_active_scte;					// ��ǰ���ڴ���ķ���
	sgid_t _SGT_asvc_sgid;							// ��ǰ���ڴ���ķ����sgid

	int _SGT_error_code;			// �������
	int _SGT_native_code;			// ���ش������
	int _SGT_ur_code;				// �û��Զ������
	int _SGT_error_detail;			// ��ϸ�������

	int _SGT_lock_type_by_me;
	int _SGT_blktime;

	bool _SGT_context_invalid;
	int _SGT_curctx;
	sgc_pointer _SGT_SGC;
	int _SGT_intcall;

	int _SGT_nwkill_counter;
	int _SGT_time_left;
	string _SGT_send_shmname;

	sg_msghdr_t _SGT_msghdr;
	bool _SGT_fini_called;
	int _SGT_fini_rval;
	int _SGT_fini_urcode;
	message_pointer _SGT_fini_msg;
	int _SGT_fini_cmd;
	bool _SGT_forward_called;

	char *_SGT_setargv_argv[256];

	int _SGT_sndprio;
	int _SGT_lastprio;
	int _SGT_getrply_inthandle;

	sg_manager *_SGT_mgr_array[MAX_MANAGERS];
	bool _SGT_svcdsp_inited;
	vector<sg_svc *> _SGT_svcdsp;

	bool _SGT_sgconfig_opened;

	// ��ӡ������Ϣʱ��ǰ׺
	std::string debug_prefix;

private:
	sgt_ctx();

	static boost::thread_specific_ptr<sgt_ctx> _instance;
};

#if defined(DEBUG)

template<typename RETVAL>
class scoped_debug
{
public:
	scoped_debug(int level, const std::string& func_name, const string& args = "", RETVAL *retval = NULL)
		: level_(level),
		  func_name_(func_name),
		  retval_(retval)
	{
		sgp_ctx *SGP = sgp_ctx::instance();
		if (level_ < SGP->_SGP_debug_level_lower || level_ > SGP->_SGP_debug_level_upper)
			return;

		sgt_ctx *SGT = sgt_ctx::instance();
		std::cout << SGT->debug_prefix << "{" << getpid() << " " << func_name_;
		if (!args.empty())
			std::cout << "(" << args << ")";
		std::cout << std::endl;
		SGT->debug_prefix += "  ";
	}

	~scoped_debug()
	{
		sgp_ctx *SGP = sgp_ctx::instance();
		if (level_ < SGP->_SGP_debug_level_lower || level_ > SGP->_SGP_debug_level_upper)
			return;

		sgt_ctx *SGT = sgt_ctx::instance();
		if (SGT->debug_prefix.length() < 2)
			return;

		SGT->debug_prefix.resize(SGT->debug_prefix.length() - 2);
		std::cout << SGT->debug_prefix << "}" << getpid() << " " << func_name_;
		if (retval_)
			std::cout << "(" << *retval_ << ")";
		std::cout << std::endl;
	}

private:
	int level_;
	std::string func_name_;
	RETVAL *retval_;
};

#endif

}
}

#endif

