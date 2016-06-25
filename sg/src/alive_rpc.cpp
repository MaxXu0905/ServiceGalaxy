#include "alive_rpc.h"
#include "alive_struct.h"

namespace ai
{
namespace sg
{

alive_rpc& alive_rpc::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<alive_rpc *>(SGT->_SGT_mgr_array[ALIVE_MANAGER]);
}

// 检查指定主机下的进程是否存活，任何异常都假定存活
bool alive_rpc::alive(const string& usrname, const string& hostname, time_t expire, pid_t pid)
{
	bool retval = true;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("usrname={1}, hostname={2}, expire={3}, pid={4}") % usrname % hostname % expire % pid).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	send_msg->set_length(sizeof(alive_rqst_t));
	alive_rqst_t *rqst = reinterpret_cast<alive_rqst_t *>(send_msg->data());
	strcpy(rqst->usrname, usrname.c_str());
	strcpy(rqst->hostname, hostname.c_str());
	rqst->expire = expire;
	rqst->pid = pid;

	sgc_ctx *SGC = SGT->SGC();
	SGC->_SGC_special_mid = SGC->_SGC_proc.mid;
	BOOST_SCOPE_EXIT((&SGC)) {
		SGC->_SGC_special_mid = BADMID;
	} BOOST_SCOPE_EXIT_END

	if (!api_mgr.call(ALIVE_SVC, send_msg, recv_msg, 0)) {
		// error_code already set
		return retval;
	}

	alive_rply_t *rply = reinterpret_cast<alive_rply_t *>(recv_msg->data());
	retval = rply->rtn;
	return retval;
}

alive_rpc::alive_rpc()
{
	send_msg = sg_message::create(SGT);
	recv_msg = sg_message::create(SGT);
}

alive_rpc::~alive_rpc()
{
}

}
}

