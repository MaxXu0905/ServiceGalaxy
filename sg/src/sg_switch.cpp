#include "sg_internal.h"

namespace ai
{
namespace sg
{

sg_bbasw_t * sg_bbasw::get_switch()
{
	static sg_bbasw_t _bbasw[] = {
		{
			"MP", BBT_MP, &sg_pte::mbcreate, &sg_nte::mbcreate, &sg_qte::mbcreate, &sg_qte::mbdestroy,
			&sg_qte::mbdestroy, &sg_qte::smupdate, &sg_qte::mbupdate, &sg_qte::smretrieve,
			&sg_sgte::mbcreate, &sg_sgte::nulldestroy, &sg_sgte::mbupdate, &sg_sgte::smretrieve,
			&sg_ste::mbcreate, &sg_ste::mbdestroy, &sg_ste::mbdestroy, &sg_ste::smupdate, &sg_ste::mbupdate,
			&sg_ste::smretrieve, &sg_ste::mboffer, &sg_ste::mbremove, &sg_scte::mbcreate, &sg_scte::mbdestroy,
			&sg_scte::mbdestroy, &sg_scte::smupdate, &sg_scte::mbupdate, &sg_scte::smretrieve, &sg_scte::mbadd,
			&sg_rpc::mbustat, &sg_rpc::nullubbver, &sg_rpc::mblaninc
		},
		{
			""
		}
	};

	return _bbasw;
}

void sg_bbasw::set_switch(const string& name)
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();

	for (sg_bbasw_t *bbaswp = get_switch(); bbaswp->name[0] != '\0'; bbaswp++) {
		if (name == bbaswp->name) {
			SGC->_SGC_bbasw = bbaswp;
			return;
		}
	}

	throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: unknown BBA switch name {1}") % name).str(SGLOCALE));
}

sg_ldbsw_t * sg_ldbsw::get_switch()
{
	static sg_ldbsw_t _ldbsw[] = {
		{ "RT", &sg_qte::rtenq, &sg_qte::rtdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqnull },
		{ "RR", &sg_qte::rrenq, &sg_qte::rrdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqnull },
		{ "AUTO", &sg_qte::rtenq, &sg_qte::rtdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqnull },
		{ "" }
	};

	return _ldbsw;
}

void sg_ldbsw::set_switch(const string& name)
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();

	for (sg_ldbsw_t *ldbswp = get_switch(); ldbswp->name[0] != '\0'; ldbswp++) {
		if (name == ldbswp->name) {
			SGC->_SGC_ldbsw = ldbswp;
			return;
		}
	}

	throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: unknown LDB switch name {1}") % name).str(SGLOCALE));
}

sg_authsw_t * sg_authsw::get_switch()
{
	static sg_authsw_t _authsw[] = {
		{ "SGCLST", &sg_rte::nullenter, &sg_rte::nullleave, &sg_rte::nullupdate, &sg_rte::nullclean },
		{ "SGMNGT", &sg_rte::nullenter, &sg_rte::nullleave, &sg_rte::nullupdate, &sg_rte::nullclean },
		{ "ADMIN", &sg_rte::smenter, &sg_rte::smleave, &sg_rte::nullupdate, &sg_rte::smclean },
		{ "SERVER", &sg_rte::smenter, &sg_rte::smleave, &sg_rte::nullupdate, &sg_rte::smclean },
		{ "CLIENT", &sg_rte::smenter, &sg_rte::smleave, &sg_rte::smupdate, &sg_rte::smclean },
		{ "" }
	};

	return _authsw;
}

void sg_authsw::set_switch(const string& name)
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();

	for (sg_authsw_t *authswp = get_switch(); authswp->name[0] != '\0'; authswp++) {
		if (name == authswp->name) {
			SGC->_SGC_authsw = authswp;
			return;
		}
	}

	throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: unknown AUTH switch name {1}") % name).str(SGLOCALE));
}

sg_switch& sg_switch::instance()
{
	sgp_ctx *SGP = sgp_ctx::instance();
	return *SGP->_SGP_switch_mgr;
}

bool sg_switch::svrstop(sgt_ctx *SGT)
{
	return true;
}

void sg_switch::bbclean(sgt_ctx *SGT, int mid, ttypes sect, int& spawn_flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("mid={1}, sect={2}, spawn_flags={3}") % mid % sect % spawn_flags).str(SGLOCALE), NULL);
#endif

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(int)));
	if (msg == NULL) {
		// error_code already set
		return;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_BBCLEAN | VERSION;
	rqst->datalen = sizeof(int);

	sg_msghdr_t& msghdr = *msg;
	msghdr.rplymtype = NULLMTYPE;

	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_ste_t bbm;
	bbm.grpid() = SGC->mid2grp(mid);
	bbm.svrid() = MNGR_PRCID;
	if (ste_mgr.retrieve(S_GRPID, &bbm, &bbm, NULL) < 0) {
		if (bbm.svrproc().is_dbbm())
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: bbclean failed, couldn't find sgclst")).str(SGLOCALE));
		else
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: bbclean failed, couldn't find sgmngr")).str(SGLOCALE));
		return;
	}

	if (!rpc_mgr.send_msg(msg, &bbm.svrproc(), SGNOREPLY | SGNOBLOCK)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: bbclean failed, message send/receive error")).str(SGLOCALE));
		return;
	}
}

bool sg_switch::getack(sgt_ctx *SGT, message_pointer& msg, int flags, int timeout)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("flags={1}, timeout={2}") % flags % timeout).str(SGLOCALE), &retval);
#endif

	sg_msghdr_t *msghdr = *msg;
	msghdr->rcvr = SGC->_SGC_proc;
	if (!ipc_mgr.receive(msg, flags | SGREPLY, timeout))
		return retval;

	// msg可能会重新分配内存，需要重置
	msghdr = *msg;
	if (msghdr->flags & SGNACK) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOCLST;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not find sgclst")).str(SGLOCALE));
		}

		return retval;
	}

	sg_rpc_rply_t *rply = msg->rpc_rply();
	if (rply->rtn < 0) {
		SGT->_SGT_error_code = rply->error_code;
		if (SGT->_SGT_error_code == SGEOS)
			SGT->_SGT_native_code = msghdr->native_code;
	}

	retval = true;
	return retval;
}

void sg_switch::shutrply(sgt_ctx *SGT, message_pointer& msg, int status, sg_bbparms_t *bbparms)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("status={1}") % status).str(SGLOCALE), NULL);
#endif

	sg_rpc_rply_t *rply = msg->rpc_rply();
	memset(rply, 0, sizeof(sg_rpc_rply_t));
	rply->rtn = -status;
	rply->error_code = status;
}

sg_switch::sg_switch()
{
	GPP = gpp_ctx::instance();
	SGP = sgp_ctx::instance();
}

sg_switch::~sg_switch()
{
}

class base_switch_initializer
{
public:
	base_switch_initializer()
	{
		sgp_ctx *SGP = sgp_ctx::instance();

		if (SGP->_SGP_bbasw_mgr == NULL)
			SGP->_SGP_bbasw_mgr = new sg_bbasw();
		if (SGP->_SGP_ldbsw_mgr == NULL)
			SGP->_SGP_ldbsw_mgr = new sg_ldbsw();
		if (SGP->_SGP_authsw_mgr == NULL)
			SGP->_SGP_authsw_mgr = new sg_authsw();
		if (SGP->_SGP_switch_mgr == NULL)
			SGP->_SGP_switch_mgr = new sg_switch();
	}
};

static base_switch_initializer initializer;

}
}

