#include "sg_internal.h"
#include "bsgws_switch.h"
#include "gwp_ctx.h"

namespace ai
{
namespace sg
{

sg_bbasw_t * bsgws_bbasw::get_switch()
{
	static sg_bbasw_t _bbasw[] = {
		{
			"MP", BBT_MP | BBF_DBBM, &sg_pte::smcreate, &sg_nte::smcreate, &sg_qte::smcreate, &sg_qte::smdestroy,
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

sg_ldbsw_t * bsgws_ldbsw::get_switch()
{
	static sg_ldbsw_t _ldbsw[] = {
		{ "RT", &sg_qte::rtenq, &sg_qte::rtdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqrt },
		{ "RR", &sg_qte::rrenq, &sg_qte::rrdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqrr },
		{ "AUTO", &sg_qte::nullenq, &sg_qte::nulldeq, &sg_viable::upinviable, &sg_qte::nullchoose, &sg_qte::syncqnull },
		{ "" }
	};

	return _ldbsw;
}

sg_authsw_t * bsgws_authsw::get_switch()
{
	static sg_authsw_t _authsw[] = {
		{ "SERVER", &sg_rte::smenter, &sg_rte::smleave, &sg_rte::nullupdate, &sg_rte::nullclean },
		{ "" }
	};

	return _authsw;
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

		SGP->_SGP_bbasw_mgr = new bsgws_bbasw();
		SGP->_SGP_ldbsw_mgr = new bsgws_ldbsw();
		SGP->_SGP_authsw_mgr = new bsgws_authsw();

		// ÉèÖÃÎªBSGWS
		SGP->_SGP_ambsgws = true;
	}
};

static derived_switch_initializer initializer;

}
}

