#include "dbbm_server.h"
#include "bbm_svc.h"
#include "bbm_rpc.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

dbbm_server::dbbm_server()
{
	BBM = bbm_ctx::instance();
}

dbbm_server::~dbbm_server()
{
}

int dbbm_server::svrinit(int argc, char **argv)
{
	sg_ste_t currste;
	sg_ste_t bbmste;
	int cnt;
	int i;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	bbm_rpc& brpc_mgr = bbm_rpc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	// 关闭框架提供的线程机制
	SGP->_SGP_threads = 0;

	if (ste_mgr.retrieve(S_BYRLINK, &SGP->_SGP_ste, &SGP->_SGP_ste, NULL) < 0) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - cannot find the sgclst entry in the Bulletin Board")).str(SGLOCALE));
		return retval;
	}

	SGP->_SGP_ste.global.maxmtype = sg_metahdr_t::bbrqstmtype();
	SGP->_SGP_ste.global.srelease = SGRELEASE;

	if (ste_mgr.update(&SGP->_SGP_ste, 1, U_LOCAL) <= 0) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - cannot update the sgclst entry in the Bulletin Board")).str(SGLOCALE));
		return retval;
	}

	if (BBM->_BBM_rec_file.empty())
		BBM->_BBM_rec_file = (boost::format("/tmp/sgclst.%1%") % SGC->_SGC_wka).str();

	if (bbp == NULL)
		return retval;

	if (bbp->bbparms.dbbm_wait * 2 > bbp->bbparms.block_time) {
		GPP->write_log((_("WARN: CLSTWAIT * 2 is greater than BLKTIME.  CLSTWAIT = {1},\tBLKTIME = {2}\n")
			% bbp->bbparms.dbbm_wait % bbp->bbparms.block_time).str(SGLOCALE));
	}

	if (bbp->bbparms.dbbm_wait * 2 <= bbp->bbparms.sanity_scan) {
		GPP->write_log((_("WARN: CLSTWAIT * 2 is not greater than ROBOSTINT.  CLSTWAIT = {1},\tROBOSTINT = {2}\n")
			% bbp->bbparms.dbbm_wait % bbp->bbparms.sanity_scan).str(SGLOCALE));
	}

	BBM->_BBM_pbbsize = sg_bboard_t::pbbsize(bbp->bbparms);

	if (SGP->_SGP_boot_flags & BF_RESTART) {
		int bbfd;
		struct stat buf;

		if ((bbfd = open(BBM->_BBM_rec_file.c_str(), O_RDWR)) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - cannot open Bulletin Board image file, {1} while restarting the sgclst, errno {2}")
				% BBM->_BBM_rec_file % errno).str(SGLOCALE));
			return retval;
		}

		BOOST_SCOPE_EXIT((&bbfd)(&BBM)) {
			close(bbfd);
			unlink(BBM->_BBM_rec_file.c_str());
		} BOOST_SCOPE_EXIT_END

		if (fstat(bbfd, &buf) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit: cannot get file system information of the Bulletin Board image file, {1} while restarting the sgclst, errno {2}")
				% BBM->_BBM_rec_file % errno).str(SGLOCALE));
			return retval;
		}

		if (buf.st_size != sizeof(bbp->bbparms.bbversion) + BBM->_BBM_pbbsize) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - the size of the Bulletin Board image file, {1} inconsistent with the SGPROFILE file")
				% BBM->_BBM_rec_file).str(SGLOCALE));
			return retval;
		}

		/*
		 * Read BB in from file, then update own entry
		 * so as to reflect current ste in new BB.
		 */
		scoped_bblock bblock(SGT);

		sg_bbmap_t bbmap = bbp->bbmap;

		if (read(bbfd, &bbp->bbparms.bbversion, sizeof(bbp->bbparms.bbversion)) != sizeof(bbp->bbparms.bbversion)
			|| read(bbfd, &bbp->bbmeters, BBM->_BBM_pbbsize) != BBM->_BBM_pbbsize) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit: error in reading the Bulletin Board image file, {1} errno {2}")
				% BBM->_BBM_rec_file % errno).str(SGLOCALE));
			return retval;
		}

		// store bbmap indexes so that we don't lose them
		bbmap.qte_free = bbp->bbmap.qte_free;
		bbmap.sgte_free = bbp->bbmap.sgte_free;
		bbmap.ste_free = bbp->bbmap.ste_free;
		bbmap.scte_free = bbp->bbmap.scte_free;
		SGC->_SGC_bbp->bbmap = bbmap;

		currste.grpid() = CLST_PGID;
		currste.svrid() = MNGR_PRCID;
		if (ste_mgr.retrieve(S_GRPID, &currste, &currste, NULL) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - cannot find sgclst entry in the Bulletin Board")).str(SGLOCALE));
			return retval;
		}

		bbp->bbparms.current = 0;

		for (i = 0; i < SGC->MAXQUES(); i++)
			memset(&SGC->_SGC_qtes[i].local, 0, sizeof(SGC->_SGC_qtes[i].local));

		for (i = 0; i < SGC->MAXSVRS(); i++)
			memset(&SGC->_SGC_stes[i].local, 0, sizeof(SGC->_SGC_stes[i].local));

		for (i = 0; i < SGC->MAXSVCS(); i++)
			memset(&SGC->_SGC_sctes[i].local, 0, sizeof(SGC->_SGC_sctes[i].local));

		bbp->bbmeters.maxaccsrs = 0;
		bbp->bbmeters.wkinitiated = 0;
		bbp->bbmeters.wkcompleted = 0;

		currste.pid() = SGC->_SGC_proc.pid;
		currste.global.status |= RESTARTING;
		currste.local.currclient.clear();
		currste.local.currservice[0] = '\0';

		if (ste_mgr.update(&currste, 1, U_LOCAL) <= 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - cannot update the sgclst entry in the Bulletin Board")).str(SGLOCALE));
			return retval;
		}

		/* in order for the sgclst to run the standard server
		 *recovery code, it has to call enroll()
		 */

		vector<sg_svcparms_t> svcparms;
		for (sg_svcdsp_t *svcdsp = SGP->_SGP_svcdsp; svcdsp->index != -1; svcdsp++) {
			const string& svc_name = svcdsp->svc_name;

			if (!SGP->_SGP_adv_all
				&& std::find(SGP->_SGP_adv_svcs.begin(), SGP->_SGP_adv_svcs.end(), svc_name) == SGP->_SGP_adv_svcs.end())
				continue;

			sg_svcparms_t item;
			strncpy(item.svc_name, svcdsp->svc_name, sizeof(item.svc_name) - 1);
			item.svc_name[sizeof(item.svc_name) - 1] = '\0';
			strncpy(item.svc_cname, svcdsp->class_name, sizeof(item.svc_cname) - 1);
			item.svc_cname[sizeof(item.svc_cname) - 1] = '\0';
			item.svc_index = svcdsp->index;
			svcparms.push_back(item);
		}

		SGP->_SGP_amserver = false;
		if (enroll("sgclst", svcparms) == BADSGID) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - system standard init function failed")).str(SGLOCALE));
			return retval;
		}

		/*
		 * Now we need to set up the registry table info on the active
		 * BBMs in the system.
		 */
		bbmste.svrid() = MNGR_PRCID;
		cnt = ste_mgr.retrieve(S_SVRID, &bbmste, NULL, NULL);
		if (cnt < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - cannot find the current sgmngr entry in the Bulletin Board")).str(SGLOCALE));
			return retval;
		}

		boost::shared_array<sgid_t> auto_bbmsgids(new sgid_t[cnt]);
		sgid_t *bbmsgids = auto_bbmsgids.get();
		ste_mgr.retrieve(S_SVRID, &bbmste, NULL, bbmsgids);

		for (i = 0; i < cnt; i++) {
			if (ste_mgr.retrieve(S_BYSGIDS, &bbmste, &bbmste, &bbmsgids[i]) != 1)
				continue;

			if (bbmste.grpid() != CLST_PGID) {
				if (BBM->_BBM_regmap.size() == MAXBBM) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - exceeds maximum number of sgmngr on all the application nodes")).str(SGLOCALE));
					return retval;
				}

				registry_t reg;
				reg.proc = bbmste.svrproc();
				reg.status = 0;
				reg.sgid = bbmsgids[i];
				reg.release = bbmste.global.srelease;
				reg.protocol = SGPROTOCOL;
				if (bbmste.global.status & PSUSPEND)
					reg.status = PARTITIONED;
				reg.usrcnt = 0;
				reg.sgvers = 0;
				reg.bbvers = 0;
				reg.tardy = 0;
				reg.timestamp = 0;
				BBM->_BBM_regmap[bbmste.mid()] = reg;
			}
		}

		if (!(currste.local.curroptions & DR_TEMP)) {
			/*
			 * Now format and broadcast a O_UDBBM message to update
			 * the DBBMs mid and pid in all shared memory BBs.
			 * This saves us having to do a complete upload to each
			 * site.
			 */
			if (rqst_msg == NULL) {
				rqst_msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_ste_t)));
			} else {
				rqst_msg->set_length(sg_rpc_rqst_t::need(sizeof(sg_ste_t)));
				rpc_mgr.init_rqst_hdr(rqst_msg);
			}

			sg_rpc_rqst_t *rqst = rqst_msg->rpc_rqst();
			rqst->opcode = O_UDBBM | VERSION;
			rqst->datalen = sizeof(sg_ste_t);
			sg_ste_t *step = reinterpret_cast<sg_ste_t *>(rqst->data());
			step->grpid() = CLST_PGID;
			step->svrid() = MNGR_PRCID;
			if (ste_mgr.retrieve(S_GRPID, step, step, NULL) != 1
				|| !brpc_mgr.udbbm(step)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: svrinit: can't retrieve own table entry")).str(SGLOCALE));
				return retval;
			}

			step->local.currclient.clear();
			step->local.currservice[0] = '\0';
			brpc_mgr.bbmsnd(rqst_msg, SGAWAITRPLY | SGSIGRSTRT);
		}

		/*
		 * Reset our STE one more time to get the currclient back in
		 * there for MIGRATE and CDBBM responses.
		 */
		if (currste.local.currservice[0]) {
			sg_ste_t tmpste = SGP->_SGP_ste;
			tmpste.local = currste.local;
			BBM->_BBM_restricted = (currste.local.curroptions & DR_TEMP);

			if (ste_mgr.update(&tmpste, 1, U_LOCAL) <= 0){
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst svrinit - cannot update the sgclst entry in the Bulletin Board")).str(SGLOCALE));
				return retval;
			}
		}
	} else {
		bbp->bbparms.bbversion = time(0);
	}

	sg_api& api_mgr = sg_api::instance(SGT);
	api_mgr.advertise(DBBM_SVC_NAME, "bbm_svc");

	usignal::signal(SIGTERM, dbbm_server::sigterm);

	BBM->_BBM_lbbvers = bbp->bbparms.bbversion;
	BBM->_BBM_lsgvers = bbp->bbparms.sgversion;

	// get system-wide defaults
	SGC->get_defaults();

	GPP->write_log((_("INFO: sgclst started successfully.")).str(SGLOCALE));
	retval = 0;
	return retval;
}

int dbbm_server::svrfini()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	unlink(BBM->_BBM_rec_file.c_str());
	if (BBM->_BBM_restricted)
		GPP->write_log((_("INFO: temporary sgclst stopped successfully.")).str(SGLOCALE));
	else
		GPP->write_log((_("INFO: sgclst stopped successfully.")).str(SGLOCALE));

	retval = 0;
	return retval;
}

int dbbm_server::run(int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_svcdsp& svcdsp_mgr = sg_svcdsp::instance(SGT);
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	int remain = bbp->bbparms.scan_unit;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	if (rqst_msg == NULL)
		rqst_msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(0));

	BBM->_BBM_rte_scan_marks.resize((SGC->MAXACCSRS() + MAXADMIN) * MAXHANDLES);

	// continuously receive messages
	while (1) {
		time_t rtime = time(0);
		if (!rcvrq(rqst_msg, 0, remain)) {
			if (SGT->_SGT_error_code == SGETIME) {
				if (++BBM->_BBM_querytime >= bbp->bbparms.bbm_query) {
					bbchk();
					BBM->_BBM_querytime = 0;
				}

				remain = bbp->bbparms.scan_unit;
				continue;
			} else if (BBM->_BBM_sig_ign) {
				GPP->write_log((_("WARN: SIGTERM was ignored while shutting down the sgclst")).str(SGLOCALE));
				BBM->_BBM_sig_ign = false;
				continue;
			}

			if (!SGP->_SGP_shutdown) {
				// Not a fatal error, not shutting down
				continue;
			}

			cleanup();
			return retval;
		}

		/*
		 * Must copy svc_name because rmsg is
		 * invalid after call to svcdsp
		 */
		sg_msghdr_t& msghdr = *rqst_msg;
		string svc_name = msghdr.svc_name;

		SGT->_SGT_fini_rval = 0;
		svcdsp_mgr.svcdsp(rqst_msg, 0);

		if (!finrq()) {
			cleanup();
			return retval;
		}

		if (SGT->_SGT_fini_rval < 0) {
			if (SGT->_SGT_fini_rval == SGFATAL) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Administrative operation request failed")).str(SGLOCALE));
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: System unstable - reboot recommended")).str(SGLOCALE));
			} else {
				string temp;
				if (SGT->_SGT_error_code == SGEOS) {
					temp = (_("ERROR: Administrative operation request {1} handled by sgclst failed - {2}, {3}")
						% svc_name % SGT->strnative() % SGT->strerror()).str(SGLOCALE);
				} else {
					temp = (_("ERROR: Administrative operation request {1} handled by sgclst failed - {2}")
						% svc_name % SGT->strerror()).str(SGLOCALE);
				}
				GPP->write_log(__FILE__, __LINE__, temp);
			}
		}

		time_t currtime = time(0);
		// if the scanunit time period has expired
		if (currtime - rtime >= bbp->bbparms.scan_unit) {
			BBM->_BBM_querytime += (currtime - rtime) / bbp->bbparms.scan_unit;
			remain = bbp->bbparms.scan_unit;
		} else {
			remain = bbp->bbparms.scan_unit - (currtime - rtime);
		}

		if (BBM->_BBM_querytime >= bbp->bbparms.bbm_query) {
			bbchk();
			BBM->_BBM_querytime = 0;
		}
	}

	return retval;
}

/*
 *  bbchk:  Check status of all BBMs, mark unreachable BBMs,
 *	    and then call cleanup() if necessary.
 */
void dbbm_server::bbchk()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	int ok = 0;	       /* Number of BBMs that are OK */
	time_t low;
	boost::unordered_map<int, registry_t>::iterator iter;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (rply_msg == NULL)
		rply_msg = rpc_mgr.create_msg(sg_rpc_rply_t::need(0));

	/*
	 * Check if new day - if so, write message to userlog
	 * so that version information is logged.
	 */
	boost::gregorian::date today = bp::second_clock::universal_time().date();
	if (BBM->_BBM_oldmday && today.day() != BBM->_BBM_oldmday) {
		for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) { // For each sgmngr
			registry_t& regent = iter->second;
			GPP->write_log((_("INFO: sgmngr on {1} - Release {2}")
				% SGC->_SGC_ptes[SGC->midpidx(regent.proc.mid)].lname % regent.release).str(SGLOCALE));
		}
		BBM->_BBM_oldmday = today.day();
	}

	BBM->_BBM_checkiter++;

	/*
	 * A restricted, temporary sgclst should see if shutdown has "gone away"
	 * and exit at that time.
	 */
	if (BBM->_BBM_restricted) {
		registry_t& regent = BBM->_BBM_regadm[AT_SHUTDOWN];
		if ((regent.proc.pid == 0 || !proc_mgr.alive(regent.proc)) && BBM->_BBM_noticed++) {
			SGC->qcleanup(true);
			svrfini();
			exit(0);
		}
	}

	low = time(0) - (2 * bbp->bbparms.scan_unit * bbp->bbparms.bbm_query);

	for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
		registry_t& regent = iter->second;

		if (regent.tardy++ == 0) {
			if (BBM->_BBM_upgrade) {
				regent.status |= NACK_DONE;
				continue;
			}
			regent.status |= (NACK_DONE | SGCFGFAIL);
			/*
			 * Before marking this BB ok, check the BB and SG
			 * version numbers also to see if either needs refreshed
			 * on this particular site.  Either is ok if it a)
			 * matches the version in effect at the last check or
			 * b) the version's been updated recently and they're
			 * version is a timestamp within the last querytime
			 * cycle.
			 */
			if (regent.bbvers == BBM->_BBM_lbbvers || regent.bbvers == 0
				|| (regent.bbvers >= low && regent.bbvers <= bbp->bbparms.bbversion)) {
				regent.status &= ~NACK_DONE;
			} else {
				GPP->write_log((_("WARN: BB out of date, node={1}")
					% SGC->pmtolm(regent.proc.mid)).str(SGLOCALE));
			}
			if (regent.sgvers == BBM->_BBM_lsgvers || regent.sgvers == 0
				|| (regent.sgvers >= low && regent.sgvers <= bbp->bbparms.sgversion)) {
				regent.status &= ~SGCFGFAIL;
			} else {
				GPP->write_log((_("WARN: SGPROFILE out of date, node={1}")
					% SGC->pmtolm(regent.proc.mid)).str(SGLOCALE));
			}

			if (!(regent.status & (NACK_DONE | SGCFGFAIL))) {
				/*
				 * We've heard from it recently (O_IMOK message)
				 * so assume OK. This should be the normal case.
				 */
				ok++;
			}
			continue;
		}

		if (regent.status & PARTITIONED) {
			/*
			 *  Already known to be partitioned, so don't
			 *  waste time trying to contact it.  When it's
			 *  back, receipt of O_IMOK will unpartition.
			 */
			continue;
		}

		if (!BBM->_BBM_force_check) {
			GPP->write_log((_("WARN: Slow sgmngr response, node= {1}")
				% SGC->pmtolm(regent.proc.mid)).str(SGLOCALE));
		}

		rqst_msg->set_length(sg_rpc_rqst_t::need(0));
		rpc_mgr.init_rqst_hdr(rqst_msg);
		sg_rpc_rqst_t *rqst = rqst_msg->rpc_rqst();
		rqst->opcode = O_SNDSTAT | VERSION;
		rqst->datalen = 0;

		sg_msghdr_t *msghdr = *rqst_msg;
		strcpy(msghdr->svc_name, BBM_SVC_NAME);
		msghdr->rcvr = regent.proc;
		msghdr->rplyiter = BBM->_BBM_checkiter;

		sg_metahdr_t& metahdr = *rqst_msg;
		metahdr.mid = regent.proc.mid;
		metahdr.qaddr = regent.proc.rqaddr;
		metahdr.flags |= METAHDR_FLAGS_DBBMFWD;

		if (!ipc_mgr.send(rqst_msg, SGNOTIME)) {
			regent.status |= BCSTFAIL;
			continue;
		}

		while (1) {
			msghdr = *rply_msg;
			msghdr->rcvr = SGC->_SGC_proc;
			msghdr->rplymtype = sg_metahdr_t::bbrplymtype(SGC->_SGC_proc.pid);
			msghdr->rplyiter = BBM->_BBM_checkiter;

			if (!ipc_mgr.receive(rply_msg, SGREPLY, bbp->bbparms.scan_unit)) {
				regent.status |= BCSTFAIL;
				break;
			}

			msghdr = *rply_msg;
			if (msghdr->rplyiter != BBM->_BBM_checkiter)
				continue;

			if (regent.proc != msghdr->sender)
				continue;

			if (msghdr->flags & SGNACK) {
				regent.status |= BCSTFAIL;
				break;
			}

			sg_rpc_rply_t *rply = rply_msg->rpc_rply();
			if (rply->rtn < 0) {
				regent.status |= BCSTFAIL;
				break;
			}

			/*
			 *  Good news!  The sgmngr responded successfully and will
			 *  not be partitioned.
			 */
			ok++;
			break;
		}
	}

	BBM->_BBM_force_check = false;
	BBM->_BBM_upgrade = false;
	BBM->_BBM_lbbvers = bbp->bbparms.bbversion;
	BBM->_BBM_lsgvers = bbp->bbparms.sgversion;

	bbm_rpc& brpc_mgr = bbm_rpc::instance(SGT);
	if (ok != BBM->_BBM_regmap.size()) {
		// One or more BBMs in trouble, so try to clean up
		brpc_mgr.cleanup();
	}

	// 同步STE、SCTE、和QTE的统计信息
	sg_ste_t *ste;
	sg_scte_t *scte;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);

	boost::shared_array<char> auto_ste_buffer(new char[(sizeof(sgid_t) + sizeof(sg_nserver_t) + sizeof(sgid_t) + sizeof(sg_lqueue_t)) * SGC->MAXSVRS()]);
	char *ste_buffer = auto_ste_buffer.get();
	int ste_cnt = 0;

	boost::shared_array<char> auto_scte_buffer(new char[(sizeof(sgid_t) + sizeof(sg_lservice_t)) * SGC->MAXSVCS()]);
	char *scte_buffer = auto_scte_buffer.get();
	int scte_cnt = 0;

	{
		// 获取数据时需要保证STE，QTE和SCTE一致
		sharable_bblock bblock(SGT);

		for (int bucket = 0; bucket < SGC->MAXSVRS(); bucket++) {
			for (int idx = SGC->_SGC_ste_hash[bucket]; idx != -1; idx = ste->hashlink.fhlink) {
				ste = ste_mgr.link2ptr(idx);
				*reinterpret_cast<sgid_t *>(ste_buffer) = ste->sgid();
				ste_buffer += sizeof(sgid_t);

				sg_lserver_t *local = &ste->local;
				sg_nserver_t *nlocal = reinterpret_cast<sg_nserver_t *>(ste_buffer);
				nlocal->total_idle = local->total_idle;
				nlocal->total_busy = local->total_busy;
				nlocal->ncompleted = local->ncompleted;
				nlocal->wkcompleted = local->wkcompleted;
				ste_buffer += sizeof(sg_nserver_t);

				sg_qte_t *qte = qte_mgr.link2ptr(ste->queue.qlink);
				*reinterpret_cast<sgid_t *>(ste_buffer) = qte->sgid();
				ste_buffer += sizeof(sgid_t);

				*reinterpret_cast<sg_lqueue_t *>(ste_buffer) = qte->local;
				ste_buffer += sizeof(sg_lqueue_t);
				ste_cnt++;
			}
		}

		for (int bucket = 0; bucket < SGC->MAXSVCS(); bucket++) {
			for (int idx = SGC->_SGC_scte_hash[bucket]; idx != -1; idx = scte->hashlink.fhlink) {
				scte = scte_mgr.link2ptr(idx);
				*reinterpret_cast<sgid_t *>(scte_buffer) = scte->sgid();
				scte_buffer += sizeof(sgid_t);

				*reinterpret_cast<sg_lservice_t *>(scte_buffer) = scte->local;
				scte_buffer += sizeof(sg_lservice_t);
				scte_cnt++;
			}
		}
	}

	size_t datalen = sizeof(int) * 2
		+ (sizeof(sgid_t) + sizeof(sg_nserver_t) + sizeof(sgid_t) + sizeof(sg_lqueue_t)) * ste_cnt
		+ (sizeof(sgid_t) + sizeof(sg_lservice_t)) * scte_cnt;

	if (rqst_msg == NULL) {
		rqst_msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(datalen));
	} else {
		rqst_msg->set_length(sg_rpc_rqst_t::need(datalen));
		rpc_mgr.init_rqst_hdr(rqst_msg);
	}

	sg_rpc_rqst_t *rqst = rqst_msg->rpc_rqst();
	rqst->opcode = O_UPLOCAL | VERSION;
	rqst->datalen = datalen;

	char *data = rqst->data();
	*reinterpret_cast<int *>(data) = ste_cnt;
	data += sizeof(int);
	*reinterpret_cast<int *>(data) = scte_cnt;
	data += sizeof(int);

	if (ste_cnt > 0) {
		ste_buffer = auto_ste_buffer.get();
		memcpy(data, ste_buffer, (sizeof(sgid_t) + sizeof(sg_nserver_t) + sizeof(sgid_t) + sizeof(sg_lqueue_t)) * ste_cnt);
		data += (sizeof(sgid_t) + sizeof(sg_nserver_t) + sizeof(sgid_t) + sizeof(sg_lqueue_t)) * ste_cnt;
	}
	if (scte_cnt > 0) {
		scte_buffer = auto_scte_buffer.get();
		memcpy(data, scte_buffer, (sizeof(sgid_t) + sizeof(sg_lservice_t)) * scte_cnt);
	}

	// 不检查错误
	(void)brpc_mgr.broadcast(rqst_msg);
}

void dbbm_server::sigterm(int signo)
{
	bbm_ctx *BBM = bbm_ctx::instance();

	usignal::signal(signo, dbbm_server::sigterm);
	BBM->_BBM_sig_ign = true;
}

}
}

