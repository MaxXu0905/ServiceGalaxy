#include "boot_switch.h"

namespace ai
{
namespace sg
{

sg_bbasw_t * boot_bbasw::get_switch()
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
			"MSG", BBT_MSG, &sg_pte::mbcreate, &sg_nte::mbcreate, &sg_qte::mbcreate, &sg_qte::mbdestroy,
			&sg_qte::mbdestroy, &sg_qte::mbupdate, &sg_qte::mbupdate, &sg_qte::mbretrieve,
			&sg_sgte::mbcreate, &sg_sgte::nulldestroy, &sg_sgte::mbupdate, &sg_sgte::mbretrieve,
			&sg_ste::mbcreate, &sg_ste::mbdestroy, &sg_ste::mbdestroy, &sg_ste::mbupdate, &sg_ste::mbupdate,
			&sg_ste::mbretrieve, &sg_ste::mboffer, &sg_ste::mbremove, &sg_scte::mbcreate, &sg_scte::mbdestroy,
			&sg_scte::mbdestroy, &sg_scte::mbupdate, &sg_scte::mbupdate, &sg_scte::mbretrieve, &sg_scte::mbadd,
			&sg_rpc::mbustat, &sg_rpc::nullubbver, &sg_rpc::mblaninc
		},
		{
			"PRIVATE", BBT_MSG | BBF_DBBM | BBF_ALRM, &sg_pte::smcreate, &sg_nte::smcreate, &sg_qte::smcreate, &sg_qte::smdestroy,
			&sg_qte::smdestroy, &sg_qte::smupdate, &sg_qte::smupdate, &sg_qte::smretrieve,
			&sg_sgte::smcreate, &sg_sgte::nulldestroy, &sg_sgte::smupdate, &sg_sgte::smretrieve,
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

sg_ldbsw_t * boot_ldbsw::get_switch()
{
	static sg_ldbsw_t _ldbsw[] = {
		{ "RT", &sg_qte::rtenq, &sg_qte::rtdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqnull },
		{ "RR", &sg_qte::rrenq, &sg_qte::rrdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqnull },
		{ "AUTO", &sg_qte::nullenq, &sg_qte::nulldeq, &sg_viable::upinviable, &sg_qte::nullchoose, &sg_qte::syncqnull },
		{ "" }
	};

	return _ldbsw;
}

sg_authsw_t * boot_authsw::get_switch()
{
	static sg_authsw_t _authsw[] = {
		{ "SGBOOT", &sg_rte::smenter, &sg_rte::smleave, &sg_rte::nullupdate, &sg_rte::nullclean },
		{ "ADMIN", &sg_rte::smenter, &sg_rte::smleave, &sg_rte::nullupdate, &sg_rte::smclean },
		{ "CLIENT", &sg_rte::smenter, &sg_rte::smleave, &sg_rte::smupdate, &sg_rte::nullclean },
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

		SGP->_SGP_bbasw_mgr = new boot_bbasw();
		SGP->_SGP_ldbsw_mgr = new boot_ldbsw();
		SGP->_SGP_authsw_mgr = new boot_authsw();
	}
};

static derived_switch_initializer initializer;

}
}

