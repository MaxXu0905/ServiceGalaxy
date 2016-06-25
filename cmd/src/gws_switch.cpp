#include "sg_internal.h"
#include "gws_switch.h"
#include "gwp_ctx.h"

namespace ai
{
namespace sg
{

sg_bbasw_t * gws_bbasw::get_switch()
{
	static sg_bbasw_t _bbasw[] = {
		{
			"MP", BBT_MP | BBF_ALRM, &sg_pte::mbcreate, &sg_nte::mbcreate, &sg_qte::mbcreate, &sg_qte::mbdestroy,
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

		SGP->_SGP_bbasw_mgr = new gws_bbasw();
	}
};

static derived_switch_initializer initializer;

}
}

