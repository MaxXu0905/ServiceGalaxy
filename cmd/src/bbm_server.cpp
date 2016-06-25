#include "bbm_server.h"
#include "bbm_svc.h"
#include "bbm_switch.h"
#include "bbm_chk.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

bbm_server::bbm_server()
{
	BBM = bbm_ctx::instance();
}

bbm_server::~bbm_server()
{
}

int bbm_server::svrinit(int argc, char **argv)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	// 关闭框架提供的线程机制
	SGP->_SGP_threads = 0;

	if (BBM->_BBM_rec_file.empty())
		BBM->_BBM_rec_file = (boost::format("/tmp/sgclst.%1%") % SGC->_SGC_wka).str();

	BBM->_BBM_pbbsize = sg_bboard_t::pbbsize(bbp->bbparms);

	{
		scoped_bblock bblock(SGT);

		bbp->bbparms.bbversion = 0;

		dbbmste.svrid() = MNGR_PRCID;
		dbbmste.grpid() = CLST_PGID;
		if (ste_mgr.retrieve(S_GRPID, &dbbmste, &dbbmste, NULL) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr svrinit: cannot find the sgclst entry in the Bulletin Board")).str(SGLOCALE));
			return retval;
		}

		dbbmste.global.status &= ~BUSY;
		if (ste_mgr.update(&dbbmste, 1, U_LOCAL) <= 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr svrinit: cannot update the sgclst entry in the Bulletin Board")).str(SGLOCALE));
			return retval;
		}

		if (ste_mgr.retrieve(S_BYRLINK, &SGP->_SGP_ste, &SGP->_SGP_ste, NULL) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr svrinit: cannot find the sgmngr entry in the Bulletin Board")).str(SGLOCALE));
			return retval;
		}

		SGP->_SGP_ste.family.plink = dbbmste.hashlink.rlink;
		if (ste_mgr.update(&SGP->_SGP_ste, 1, U_GLOBAL) <= 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr svrinit: cannot update the sgmngr entry in the Bulletin Board")).str(SGLOCALE));
			return retval;
		}
	}

	usignal::signal(SIGTERM, bbm_server::sigterm);

	// get system-wide defaults
	SGC->get_defaults();

	BBM->_BBM_interval_start_time = time(0);

	sg_api& api_mgr = sg_api::instance(SGT);
	api_mgr.advertise(BBM_SVC_NAME, "bbm_svc");

	bbp->bbmeters.timestamp = time(0);

	GPP->write_log((_("INFO: sgmngr started successfully.")).str(SGLOCALE));
	retval = 0;
	return retval;
}

int bbm_server::svrfini()
{
	bbm_ctx *BBM = bbm_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	bool duplicate = (SGT->_SGT_error_code == SGEDUPENT);

	if (BBM->_BBM_gwproc == NULL && !duplicate) {
		if ((SGC->_SGC_ptes != NULL && (SGC->_SGC_ptes[SGC->midpidx(SGC->_SGC_proc.mid)].flags & NP_NETWORK))
			|| (SGC->_SGC_ptes == NULL && SGT->_SGT_error_code == SGELIMIT)) {
			/*
			 * There might be a bootstrap bridge in certain
			 * error cases so check for the existence of the
			 * well known queue address and dummy up the
			 * tmproc for the bridge.
			 * E.g., occurs when O_RGSTR fails because of
			 * license violation (hence TPELIMIT test).
			 */
			sg_wkinfo_t wkinfo;
			int wka = SGC->midkey(SGC->_SGC_proc.mid);
			if (proc_mgr.get_wkinfo(wka, wkinfo)) {
				BBM->_BBM_gwproc = &BBM->_BBM_gwste.svrproc();
				*BBM->_BBM_gwproc = SGC->_SGC_proc;
				BBM->_BBM_gwproc->svrid = GWS_PRCID;
				BBM->_BBM_gwproc->rqaddr = wkinfo.qaddr;
				BBM->_BBM_gwproc->rpaddr = wkinfo.qaddr;
			}
		}
	}

	if (BBM->_BBM_gwproc != NULL && !duplicate) {
		message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(0));
		if (msg == NULL) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: The sgmngr could not bring down a paired sggws")).str(SGLOCALE));
		} else {
			/*
			 * Make the message type large so that the sggws will
			 * receive the shutdown last whether it does FIFO or
			 * priority order receives.
			 */
			sg_msghdr_t& msghdr = *msg;
			msghdr.sendmtype = std::numeric_limits<int>::max();
			msghdr.rplymtype = NULLMTYPE;
			msghdr.rcvr = *BBM->_BBM_gwproc;

			// set message meta-header
			sg_metahdr_t& metahdr = *msg;
			metahdr.flags |= METAHDR_FLAGS_GW;
			metahdr.mid = msghdr.rcvr.mid;
			metahdr.qaddr = msghdr.rcvr.rqaddr;
			metahdr.mtype = msghdr.sendmtype;

			sg_rpc_rqst_t *rqst = msg->rpc_rqst();
			rqst->opcode = OB_EXIT | VERSION;
			rqst->datalen = 0;
			if (!ipc_mgr.send(msg, 0))
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: The sgmngr could not bring down a paired sggws")).str(SGLOCALE));
		}
	}

	GPP->write_log((_("INFO: sgmngr stopped successfully.")).str(SGLOCALE));
	retval = 0;
	return retval;
}

int bbm_server::run(int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_switch& switch_mgr = sg_switch::instance();
	sg_svcdsp& svcdsp_mgr = sg_svcdsp::instance(SGT);
	bbm_chk& chk_mgr = bbm_chk::instance(SGT);
	int	spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	message_pointer msg = sg_message::create(SGT);
	int remain = bbp->bbparms.scan_unit;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	BBM->_BBM_rte_scan_marks.resize((SGC->MAXACCSRS() + MAXADMIN) * MAXHANDLES);

	// continuously receive messages
	while (1) {
		if (BBM->_BBM_upload >= 2) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Exceeds the maximum number of times for the sgmngr to get a new Bulletin Board")).str(SGLOCALE));
			break;
		}

		if (!rcvrq(msg, 0, remain)) {
			if (SGT->_SGT_error_code == SGETIME) {
				chk_mgr.chk_blockers();
				if (++BBM->_BBM_cleantime >= bbp->bbparms.sanity_scan) {
					spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV | TIMED_CLEAN;
					switch_mgr.bbclean(SGT, SGC->_SGC_proc.mid, NOTAB, spawn_flags);
					BBM->_BBM_cleantime = 0;
				}

				BBM->_BBM_interval_start_time = time(0);
				remain = bbp->bbparms.scan_unit;
				continue;
			} else if (BBM->_BBM_sig_ign) {
				GPP->write_log((_("WARN: SIGTERM was ignored while shutting down the sgmngr")).str(SGLOCALE));
				BBM->_BBM_sig_ign = false;
				continue;
			}

			if (!SGP->_SGP_shutdown) {
				// Not a fatal error, not shutting down
				continue;
			}

			return retval;
		}

		char rqustsrvc[MAX_SVC_NAME + 1];
		time_t currtime;
		bool did_the_scan = false;

		sg_msghdr_t& msghdr = *msg;
		/*
		 * Must copy rqustsrvc because rmsg is
		 * invalid after call to svcdsp
		 */
		strcpy(rqustsrvc, msghdr.svc_name);

		SGT->_SGT_fini_rval = 0;
		svcdsp_mgr.svcdsp(msg, 0);

		if (!finrq())
			return retval;

		if (SGT->_SGT_fini_rval < 0) {
			string temp;
			if (SGT->_SGT_error_code == SGEOS) {
				temp = (_("ERROR: Administrative operation request {1} handled by sgmngr failed - {2}, {3}")
					% rqustsrvc % SGT->strnative() % SGT->strerror()).str(SGLOCALE);
			} else {
				temp = (_("ERROR: Administrative operation request {1} handled by sgmngr failed - {2}")
					% rqustsrvc % SGT->strerror()).str(SGLOCALE);
			}
			GPP->write_log(__FILE__, __LINE__, temp);
			spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
			switch_mgr.bbclean(SGT, SGC->_SGC_proc.mid, NOTAB, spawn_flags);
			BBM->_BBM_cleantime = 0;
		}

		currtime = time(0);
		// if the scanunit time period has expired
		if (currtime - BBM->_BBM_interval_start_time >= bbp->bbparms.scan_unit) {
			chk_mgr.chk_blockers();
			BBM->_BBM_cleantime += (currtime - BBM->_BBM_interval_start_time) / bbp->bbparms.scan_unit;
			did_the_scan = true;
		}
		if (BBM->_BBM_cleantime >= bbp->bbparms.sanity_scan) {
			// clean the BB
			spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV | TIMED_CLEAN;
			switch_mgr.bbclean(SGT, SGC->_SGC_proc.mid, NOTAB, spawn_flags);
			BBM->_BBM_cleantime = 0;
		}

		if (did_the_scan) {
			BBM->_BBM_interval_start_time = time(0);
			remain = bbp->bbparms.scan_unit;
		} else {
			remain = bbp->bbparms.scan_unit - (currtime - BBM->_BBM_interval_start_time);
		}
	}

	return retval;
}

void bbm_server::sigterm(int signo)
{
	bbm_ctx *BBM = bbm_ctx::instance();

	usignal::signal(signo, bbm_server::sigterm);
	BBM->_BBM_sig_ign = true;
}

}
}

