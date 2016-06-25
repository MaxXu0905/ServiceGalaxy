#include "bbm_ctx.h"

namespace ai
{
namespace sg
{

bbm_ctx bbm_ctx::_instance;

bbm_ctx * bbm_ctx::instance()
{
	return &_instance;
}

bbm_ctx::~bbm_ctx()
{
}

bbm_ctx::bbm_ctx()
{
	_BBM_inbbclean = false;
	_BBM_sig_ign = false;
	_BBM_restricted = false;

	_BBM_pbbsize = 0;
	_BBM_inmasterbb = false;

	_BBM_oldmday = 0;
	_BBM_checkiter = 0;
	_BBM_noticed = 0;
	_BBM_upgrade = false;

	_BBM_lbbvers = 0;
	_BBM_lsgvers = 0;

	_BBM_cleantime = 0;
	_BBM_querytime = 0;
	_BBM_oldflag = false;

	_BBM_amregistered = false;
	_BBM_force_check = false;

	_BBM_gwproc = NULL;

	_BBM_upload = 0;
	_BBM_interval_start_time = 0;

	_BBM_IMOK_norply_count = 0;
	_BBM_IMOK_rply_interval = 60;
	_BBM_master_BBM_sgid = BADSGID;
	_BBM_DBBM_status = DBBM_STATE_OK;
	_BBM_master_suspended = false;
}

}
}

