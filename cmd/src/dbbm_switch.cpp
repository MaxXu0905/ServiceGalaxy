#include "sg_internal.h"
#include "dbbm_switch.h"
#include "dbbm_rte.h"
#include "bbm_chk.h"
#include "bbm_rpc.h"

namespace ai
{
namespace sg
{

sg_bbasw_t * dbbm_bbasw::get_switch()
{
	static sg_bbasw_t _bbasw[] = {
		{
			"MP", BBF_DBBM | BBF_ALRM, &sg_pte::smcreate, &sg_nte::smcreate, &sg_qte::smcreate, &sg_qte::smdestroy,
			&sg_qte::smdestroy, &sg_qte::smupdate, &sg_qte::smupdate, &sg_qte::smretrieve,
			&sg_sgte::smcreate, &sg_sgte::smdestroy, &sg_sgte::smupdate, &sg_sgte::smretrieve,
			&sg_ste::smcreate, &sg_ste::smdestroy, &sg_ste::smdestroy, &sg_ste::smupdate, &sg_ste::smupdate,
			&sg_ste::smretrieve, &sg_ste::smoffer, &sg_ste::smremove, &sg_scte::smcreate, &sg_scte::smdestroy,
			&sg_scte::smdestroy, &sg_scte::smupdate, &sg_scte::smupdate, &sg_scte::smretrieve, &sg_scte::smadd,
			&sg_rpc::smustat, &sg_rpc::nullubbver, &sg_rpc::smlaninc
		},
		{
			""
		}
	};

	return _bbasw;
}

sg_ldbsw_t * dbbm_ldbsw::get_switch()
{
	static sg_ldbsw_t _ldbsw[] = {
		{ "RT", &sg_qte::rtenq, &sg_qte::rtdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqnull },
		{ "RR", &sg_qte::rrenq, &sg_qte::rrdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqnull },
		{ "AUTO", &sg_qte::nullenq, &sg_qte::nulldeq, &sg_viable::upinviable, &sg_qte::nullchoose, &sg_qte::syncqnull },
		{ "" }
	};

	return _ldbsw;
}

sg_authsw_t * dbbm_authsw::get_switch()
{
	static sg_authsw_t _authsw[] = {
		{ "SGCLST", &sg_rte::venter, &sg_rte::nullleave, &sg_rte::nullupdate, &sg_rte::smclean },
		{ "" }
	};

	return _authsw;
}

dbbm_switch::dbbm_switch()
{
	BBM = bbm_ctx::instance();
}

dbbm_switch::~dbbm_switch()
{
}

bool dbbm_switch::svrstop(sgt_ctx *SGT)
{
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	/*
	 * Don't have to lock anything since sgclst has exclusive access
	 * to its structures by definition. Find out if all BBMs and admins
	 * (except shutdown) have exited. If so, it's ok to shutdown.
	 */

	int acsrcnt = BBM->_BBM_regmap.size();	/* number of BBMs currently registered */

	for (int i = 0; i < MAXADMIN; i++) {
		registry_t& regent = BBM->_BBM_regadm[i];
		if (regent.proc.pid == 0)
			continue;

		if (i != AT_SHUTDOWN && proc_mgr.alive(regent.proc))
			acsrcnt++;
	}

	if (acsrcnt == 0) {
		sgc_ctx *SGC = SGT->SGC();
		SGC->_SGC_bbp->bbmeters.status |= SHUTDOWN;
		retval = true;
		return retval;
	}

	return retval;
}

void dbbm_switch::bbclean(sgt_ctx *SGT, int mid, ttypes sect, int& spawn_flags)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("mid={1}, sect={2}, spawn_flags={3}") % mid % sect % spawn_flags).str(SGLOCALE), NULL);
#endif
}

class derived_switch_initializer
{
public:
	derived_switch_initializer()
	{
		sgp_ctx *SGP = sgp_ctx::instance();

		if (SGP->_SGP_bbasw_mgr) {
			delete SGP->_SGP_bbasw_mgr;
			SGP->_SGP_bbasw_mgr = NULL;
		}

		if (SGP->_SGP_ldbsw_mgr) {
			delete SGP->_SGP_ldbsw_mgr;
			SGP->_SGP_ldbsw_mgr = NULL;
		}

		if (SGP->_SGP_authsw_mgr) {
			delete SGP->_SGP_authsw_mgr;
			SGP->_SGP_authsw_mgr = NULL;
		}

		if (SGP->_SGP_switch_mgr) {
			delete SGP->_SGP_switch_mgr;
			SGP->_SGP_switch_mgr = NULL;
		}

		SGP->_SGP_bbasw_mgr = new dbbm_bbasw();
		SGP->_SGP_ldbsw_mgr = new dbbm_ldbsw();
		SGP->_SGP_authsw_mgr = new dbbm_authsw();
		SGP->_SGP_switch_mgr = new dbbm_switch();

		sgt_ctx *SGT = sgt_ctx::instance();

		if (SGT->_SGT_mgr_array[RTE_MANAGER]) {
			delete SGT->_SGT_mgr_array[RTE_MANAGER];
			SGT->_SGT_mgr_array[RTE_MANAGER] = NULL;
		}

		SGT->_SGT_mgr_array[RTE_MANAGER] = new dbbm_rte();
		SGT->_SGT_mgr_array[CHK_MANAGER] = new bbm_chk();
		SGT->_SGT_mgr_array[BRPC_MANAGER] = new bbm_rpc();
	}
};

static derived_switch_initializer initializer;

}
}

