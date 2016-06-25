#include "sg_internal.h"
#include "bbm_switch.h"
#include "bbm_rte.h"
#include "bbm_chk.h"
#include "bbm_rpc.h"

namespace ai
{
namespace sg
{

sg_bbasw_t * bbm_bbasw::get_switch()
{
	static sg_bbasw_t _bbasw[] = {
		{
			"MP", BBT_MP | BBF_ALRM, &sg_pte::smcreate, &sg_nte::smcreate, &sg_qte::smcreate, &sg_qte::smdestroy,
			&sg_qte::mbdestroy, &sg_qte::smupdate, &sg_qte::mbupdate, &sg_qte::smretrieve,
			&sg_sgte::smcreate, &sg_sgte::smdestroy, &sg_sgte::smupdate, &sg_sgte::smretrieve,
			&sg_ste::smcreate, &sg_ste::smdestroy, &sg_ste::mbdestroy, &sg_ste::smupdate, &sg_ste::mbupdate,
			&sg_ste::smretrieve, &sg_ste::smoffer, &sg_ste::smremove, &sg_scte::smcreate, &sg_scte::smdestroy,
			&sg_scte::mbdestroy, &sg_scte::smupdate, &sg_scte::mbupdate, &sg_scte::smretrieve, &sg_scte::smadd,
			&sg_rpc::smustat, &sg_rpc::nullubbver, &sg_rpc::smlaninc
		},
		{
			""
		}
	};

	return _bbasw;
}

sg_ldbsw_t * bbm_ldbsw::get_switch()
{
	static sg_ldbsw_t _ldbsw[] = {
		{ "RT", &sg_qte::rtenq, &sg_qte::rtdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqrt },
		{ "RR", &sg_qte::rrenq, &sg_qte::rrdeq, &sg_viable::upinviable, &sg_qte::choose, &sg_qte::syncqrr },
		{ "AUTO", &sg_qte::nullenq, &sg_qte::nulldeq, &sg_viable::upinviable, &sg_qte::nullchoose, &sg_qte::syncqnull },
		{ "" }
	};

	return _ldbsw;
}

sg_authsw_t * bbm_authsw::get_switch()
{
	static sg_authsw_t _authsw[] = {
		{ "SGMNGR", &sg_rte::venter, &sg_rte::vleave, &sg_rte::nullupdate, &sg_rte::smclean },
		{ "" }
	};

	return _authsw;
}

bbm_switch::bbm_switch()
{
}

bbm_switch::~bbm_switch()
{
}

bool bbm_switch::svrstop(sgt_ctx *SGT)
{
	int	cnt;
	int acsrcnt;
	static sg_ste_t gwste;
	int spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	bbm_chk& chk_mgr = bbm_chk::instance(SGT);
	bbm_rpc& brpc_mgr = bbm_rpc::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	/*
	 * Check for any blocked processes and cleanup any dead processes.
	 */
	chk_mgr.chk_blockers();
	bbclean(SGT, SGC->_SGC_proc.mid, NOTAB, spawn_flags);

	/*
	 * Don't allow a shutdown unless the BB we are tending is empty.
	 * Don't have to lock BB (tmbblock()) since only checking rgsty
	 * table. Thus, any accesses to other tables in BB are fine (we
	 * will fail anyway since other accessers exist).
	 */

	scoped_syslock syslock(SGT);

	/*
	 * Curaccsrs includes a sgmngr, servers, clients & admins.
	 * It is ok for tmshutdown to be included, so subtract it
	 * out. Then, only 1 process should be attached (the sgmngr)
	 * to allow for a shutdown.
	 */
	acsrcnt = bbp->bbmeters.caccsrs;

	/*
	 * The algorithm for tracking curaccsrs has changed so we now need
	 * to add in the reserved entries.  We'll skip over tmshutdown if
	 * we see it since that's allowed.
	 */
	sg_rte_t *rslotp = SGC->_SGC_rtes + SGC->MAXACCSRS();
	for (int i = 0; i < MAXADMIN; i++) {
		/*
		 * Skip over not-in-use entries.
		 * Don't count the sgmngr itself and SHUTDOWN, others do count.
		 */
		if (rslotp[i].pid <= 0 || i == AT_BBM || i == AT_SHUTDOWN)
			continue;

		acsrcnt++;
	}

	// get the administrative processes from the sgclst
	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_clinfo_t)));

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_RRTE | VERSION;
	rqst->arg1.flag = CLIENT;
	rqst->datalen = sizeof(sg_clinfo_t);
	sg_clinfo_t *info = reinterpret_cast<sg_clinfo_t *>(rqst->data());
	info->usrname[0] = '\0';
	info->cltname[0] = '\0';

	if (rpc_mgr.send_msg(msg, NULL, SGAWAITRPLY | SGSIGRSTRT)) {
		sg_rpc_rply_t *rply = msg->rpc_rply();
		if (rply->rtn > 0) {
			sg_rte_t *clients = reinterpret_cast<sg_rte_t *>(rply->data());
			for (int i = 0; i < rply->rtn; i++) {
				// stored excess 1
				if (strcmp(clients[i].cltname, "sgshut") != 0)
					acsrcnt++;
			}
		}
	}

	/*
	 * The paired bridge that accompanies some BBMs in a LAN implementation
	 * will also be brought down so don't count it here.
	 */

	if (brpc_mgr.pairedgw(SGC->_SGC_proc, &gwste, NULL)) {
		BBM->_BBM_gwproc = &gwste.svrproc();
		acsrcnt--;
		/*
		 * If we're running on the master site and there are non-master
		 * node BBMs running and if we have a paired bridge, then we
		 * cannot shutdown.
		 */
		boost::shared_array<sg_ste_t> auto_stes(new sg_ste_t[MAXBBM + 1]);
		sg_ste_t *stes = auto_stes.get();

		sg_ste_t dbbmste;
		dbbmste.svrid() = MNGR_PRCID;
		dbbmste.grpid() = CLST_PGID;
		if ((ste_mgr.retrieve(S_GRPID, &dbbmste, &dbbmste, NULL) == 1)
			&& !SGC->remote(dbbmste.mid())
			&& (cnt = ste_mgr.retrieve(S_SVRID, &dbbmste, stes, NULL)) > 1) {
			for (int i = 0; i < cnt; i++) {
				if (stes[i].svrproc().is_bbm() && SGC->remote(stes[i].mid()))
					return retval;
			}
		}
	}

	/*
	 * We don't count ourselves anymore (see above), so this should be 0.
	 */
	if (acsrcnt == 0) {
		bbp->bbmeters.status |= SHUTDOWN;
		retval = true;
	}

	/*
	 * Remove our entry from the sgclst's registry table.
	 */

	if (retval)
		brpc_mgr.resign();

	return retval;
}

void bbm_switch::bbclean(sgt_ctx *SGT, int mid, ttypes sect, int& spawn_flags)
{
	sg_ste_t *stes, *s, key;
	sg_qte_t dq;
	bool insane;
	int cnt;
	string temp;
	const char *machnm;
	short saved_slot;
	sg_rte_t *save_rte;
	int save_flag;
	int bbfd;
	sgp_ctx *SGP = sgp_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	bbm_chk& chk_mgr = bbm_chk::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_viable& viable_mgr = sg_viable::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("mid={1}, sect={2}, spawn_flags={3}") % mid % sect % spawn_flags).str(SGLOCALE), NULL);
#endif

	// if already doing bbclean or not a server yet, return immediately
	if (BBM->_BBM_inbbclean || !SGP->_SGP_amserver)
		return;

	// if already doing an RPC, just schedule a clean
	if (BBM->_BBM_inmasterbb) {
		BBM->_BBM_cleantime = bbp->bbparms.sanity_scan;
		return;
	}

	if (mid != SGC->_SGC_proc.mid)
		return;	// only clean up our own machine

	if ((machnm = SGC->mid2lmid(mid)) == NULL) {
		temp = boost::lexical_cast<string>(mid);
		machnm = temp.c_str();
	}

	save_flag = spawn_flags;
	BBM->_BBM_inbbclean = true;

	BOOST_SCOPE_EXIT((&BBM)) {
		BBM->_BBM_inbbclean = false;
	} BOOST_SCOPE_EXIT_END

	/*
	 * This section was changed to look up the sgclst tmproc each time a clean
	 * is done so that for HA implementations where the sgclst may migrate
	 * we don't lose track of it.
	 */
	sg_ste_t dbbmste;
	dbbmste.svrid() = MNGR_PRCID;
	dbbmste.grpid() = CLST_PGID;
	if (ste_mgr.retrieve(S_GRPID, &dbbmste, &dbbmste, NULL) < 0){
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr bbclean: cannot find the sgclst entry in the Bulletin Board")).str(SGLOCALE));
		goto svrchk;
	}

	/*
	 * Special for HA, only allow BBMs on the same host as the
	 * sgclst the priviledge of restarting the sgclst. This is because
	 * the sgmngr doesn't know enough to tell if its appropriate for
	 * it to restart a sgclst locally when it was running remote.
	 */
	if (SGC->remote(dbbmste.mid()))
		goto svrchk;

	if (!proc_mgr.alive(dbbmste.svrproc())) {
		if (SGT->_SGT_error_code == SGETIME){
			// bbm with lowest group id restarts dbbm
			boost::shared_array<sg_ste_t> auto_stes(new sg_ste_t[MAXBBM + 1]);
			stes = auto_stes.get();
			if (!stes) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure")).str(SGLOCALE));
				goto svrchk;
			}

			key.svrid() = MNGR_PRCID;
			if ((cnt = ste_mgr.retrieve(S_SVRID, &key, stes, NULL)) < 0){
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr bbclean cannot find sgmngr entries in the Bulletin Board")).str(SGLOCALE));
				goto svrchk;
			}
			spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
			s = stes;
			for (int i = 0; i < cnt; i++, s++){
				/*
				 * Is there another sgmngr with a lower group id
				 * that is up and running and not remote ? If
				 * so, it's the one that should restart
				 * the sgclst.
				 */
				if(s->grpid() < SGC->_SGC_proc.grpid
					&& s->svrproc().is_bbm()
					&& !SGC->remote(s->mid())
					&& !viable_mgr.inviable(s, spawn_flags))
					goto svrchk;
			}
			// fall through to restart dbbl
		}

		dbbmste.svrid() = MNGR_PRCID;
		dbbmste.grpid() = CLST_PGID;
		if(ste_mgr.retrieve(S_GRPID, &dbbmste, &dbbmste, NULL) < 0){
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr bbclean cannot find sgclst entry in the Bulletin Board")).str(SGLOCALE));
			goto svrchk;
		}
		dq.hashlink.rlink = dbbmste.queue.qlink;
		if(qte_mgr.retrieve(S_BYRLINK, &dq, &dq, NULL) < 0){
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr bbclean cannot find queue entry for sgclst in the Bulletin Board")).str(SGLOCALE));
			goto svrchk;
		}

		if ((bbfd = open(BBM->_BBM_rec_file.c_str(), O_CREAT | O_TRUNC | O_WRONLY, SGC->_SGC_perm)) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr bbclean cannot open Bulletin Board image, {1} while restarting the sgclst") % BBM->_BBM_rec_file).str(SGLOCALE));
			goto svrchk;
		}

		if (write(bbfd, &bbp->bbparms.bbversion, sizeof(bbp->bbparms.bbversion)) != sizeof(bbp->bbparms.bbversion)
			|| write(bbfd, &bbp->bbmeters, BBM->_BBM_pbbsize) != BBM->_BBM_pbbsize) {
			close(bbfd);
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr bbclean cannot write the Bulletin Board for restarting the sgclst")).str(SGLOCALE));
			goto svrchk;
		}
		close(bbfd);

		/*
		 * Reset the sgclst status so that if we're not the first
		 * restartsrv, the other one will pick the sgclst for restarting.
		 */
		dbbmste.global.status |= RESTARTING;
		dbbmste.global.status &= ~IDLE;
		ste_mgr.update(&dbbmste, 1, U_LOCAL);
		spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
		if (viable_mgr.rstsvr(&dq, &dbbmste, spawn_flags) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr bbclean attempting to restart the sgclst failed")).str(SGLOCALE));
			goto svrchk;
		}
	}

svrchk:
	do {
		insane = false;
		if (chk_mgr.chk_bbsanity(sect, spawn_flags) < 0) {
			// abandon old BB and get a new one
			if (BBM->_BBM_upload < 2) {
				GPP->write_log(__FILE__, __LINE__, (_("INFO: BB on node {1} corrupted--replacing BB;\n\t\tBB statistics will be lost (upload={2}).")
					% machnm % BBM->_BBM_upload).str(SGLOCALE));
			} else {
				GPP->write_log(__FILE__, __LINE__, (_("INFO: BB on node {1} corrupted; recovery attempts have failed.\n\tPlease contact system administrator.")
					% machnm).str(SGLOCALE));
				return;
			}
			/*
			 * Keep the same registry table slot.  There is no
			 * need to save and restore _tmauthp[_tmauthindx].leave
			 * since this is equal to nullfunc for a sgmngr.
			 */
			save_rte = SGC->_SGC_crte;
			saved_slot = SGC->_SGC_slot;
			SGC->detach();
			SGC->_SGC_slot = saved_slot;
			SGC->_SGC_crte = save_rte;
			if (!SGC->hookup("SGMNGR")) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr bbclean: cannot re-attach to the Bulletin Board for recovery")).str(SGLOCALE));
				return;
			}
			BBM->_BBM_upload++;
			insane = true;
		}
	} while (insane && BBM->_BBM_upload < 2);

	if (insane) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr bbclean: BB insane")).str(SGLOCALE));
		return;
	}

	/*
	 *	The round-robin (RR) heuristic load balancing algoritmm uses
	 *	cumulative workload to spread requests evenly over the system.
	 *	Each queue's local counter is reset here every sanity scan
	 *	cycle so the heurisitc isn't affected by stale info.
	 *	This prevents some servers from being swamped because other
	 *	servers did lots of work at some point in the past.
	 *	Conditions leading to such an imbalance include:  data
	 *	dependent routing, partitioning, service suspension,
	 *	local idle server preference, etc.
	 *	For SHM-mode (RT) load balancing, check queue statistics
	 *	and correct any errors due to a tmdequeue() race condition.
	 */
	qte_mgr.syncq();

	/*
	 *  Check in with the sgclst (asynchronously) to let it know this sgmngr
	 *  is still alive and sane.
	 */
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	message_pointer msg;
	{
		int i;
		sg_ste_t ste;
		sg_scte_t scte;

		// 保证获取的STE，QTE和SCTE一致
		sharable_bblock bblock(SGT);

		boost::shared_array<sg_ste_t> auto_stes(new sg_ste_t[SGC->MAXSVRS()]);
		sg_ste_t *stes = auto_stes.get();
		ste.mid() = SGC->_SGC_proc.mid;
		int ste_cnt = ste_mgr.retrieve(S_MACHINE, &ste, stes, NULL);
		if (ste_cnt < 0)
			ste_cnt = 0;

		boost::shared_array<sg_scte_t> auto_sctes(new sg_scte_t[SGC->MAXSVCS()]);
		sg_scte_t *sctes = auto_sctes.get();
		scte.mid() = SGC->_SGC_proc.mid;
		int scte_cnt = scte_mgr.retrieve(S_MACHINE, &scte, sctes, NULL);
		if (scte_cnt < 0)
			scte_cnt = 0;

		size_t datalen = sizeof(int) * 2
			+ (sizeof(sgid_t) + sizeof(sg_nserver_t) + sizeof(sgid_t) + sizeof(sg_lqueue_t)) * ste_cnt
			+ (sizeof(sgid_t) + sizeof(sg_lservice_t)) * scte_cnt;

		msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(datalen));
		if (msg == NULL) {
			GPP->write_log((_("WARN: O_IMOK message could not be sent to sgclst - {1}") % SGT->strerror()).str(SGLOCALE));
			return;
		}

		sg_rpc_rqst_t *rqst = msg->rpc_rqst();
		rqst->opcode = O_IMOK | VERSION;
		rqst->arg1.vers = bbp->bbparms.bbversion;
		rqst->datalen = datalen;

		sg_config& config_mgr = sg_config::instance(SGT);
		sg_bbparms_t& cfg_bbparms = config_mgr.get_bbparms();
		bbp->bbparms.sgversion = cfg_bbparms.sgversion;
		rqst->arg2.vers = bbp->bbparms.sgversion;
		rqst->arg3.usrcnt = 0;

		char *data = rqst->data();
		*reinterpret_cast<int *>(data) = ste_cnt;
		data += sizeof(int);
		*reinterpret_cast<int *>(data) = scte_cnt;
		data += sizeof(int);

		for (i = 0; i < ste_cnt; i++) {
			*reinterpret_cast<sgid_t *>(data) = stes[i].hashlink.sgid;
			data += sizeof(sgid_t);

			sg_lserver_t *local = &stes[i].local;
			sg_nserver_t *nlocal = reinterpret_cast<sg_nserver_t *>(data);
			nlocal->total_idle = local->total_idle;
			nlocal->total_busy = local->total_busy;
			nlocal->ncompleted = local->ncompleted;
			nlocal->wkcompleted = local->wkcompleted;
			data += sizeof(sg_nserver_t);

			sg_qte_t *qte = qte_mgr.link2ptr(stes[i].queue.qlink);
			*reinterpret_cast<sgid_t *>(data) = qte->hashlink.sgid;
			data += sizeof(sgid_t);

			*reinterpret_cast<sg_lqueue_t *>(data) = qte->local;
			data += sizeof(sg_lqueue_t);
		}

		for (i = 0; i < scte_cnt; i++) {
			*reinterpret_cast<sgid_t *>(data) = sctes[i].hashlink.sgid;
			data += sizeof(sgid_t);

			*reinterpret_cast<sg_lservice_t *>(data) = sctes[i].local;
			data += sizeof(sg_lservice_t);
		}
	}

	if ((save_flag & TIMED_CLEAN) && ++BBM->_BBM_IMOK_norply_count == BBM->_BBM_IMOK_rply_interval) {
		BBM->_BBM_IMOK_norply_count = 0;

		if (BBM->_BBM_master_BBM_sgid == BADSGID) {
			boost::shared_array<sgid_t> auto_sgids(new sgid_t[MAXBBM + 1]);
			sgid_t *sgids = auto_sgids.get();
			if (sgids == NULL) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure")).str(SGLOCALE));
				return;
			}

			boost::shared_array<sg_ste_t> auto_bbmstes(new sg_ste_t[MAXBBM + 1]);
			sg_ste_t *bbmstes = auto_bbmstes.get();
			if (bbmstes == NULL) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure")).str(SGLOCALE));
				return;
			}

			sg_ste_t bbmste;
			bbmste.svrid() = MNGR_PRCID;

			int i;
			int cnt = ste_mgr.retrieve(S_SVRID, &bbmste, bbmstes, sgids);
			for (i = 0; i < cnt; i++) {
				if (bbmstes[i].mid() == dbbmste.mid()
					&& bbmstes[i].grpid() != CLST_PGID) {
					BBM->_BBM_master_BBM_sgid = sgids[i];
					break;
				}
			}
			if (BBM->_BBM_master_BBM_sgid == BADSGID) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find master sgmngr")).str(SGLOCALE));
				return;
			}

			if (SGC->_SGC_proc.grpid == bbmstes[i].grpid()) {
				// I'm master sgmngr. Not necessary.
				BBM->_BBM_IMOK_rply_interval = 0;
				goto normal;
			}
		}

		if (BBM->_BBM_DBBM_status == DBBM_STATE_Down && !BBM->_BBM_master_suspended) {
			rpc_mgr.ustat(&BBM->_BBM_master_BBM_sgid, 1, SUSPENDED, STATUS_OP_OR);
			GPP->write_log((_("INFO: master node suspended.")).str(SGLOCALE));
			BBM->_BBM_master_suspended = true;
		} else if (BBM->_BBM_DBBM_status != DBBM_STATE_Down && BBM->_BBM_master_suspended) {
			rpc_mgr.ustat(&BBM->_BBM_master_BBM_sgid, 1, ~SUSPENDED, STATUS_OP_AND);
			GPP->write_log((_("INFO: master node resumed.")).str(SGLOCALE));
			BBM->_BBM_master_suspended = false;
		}

		if (BBM->_BBM_DBBM_status == DBBM_STATE_MayDown)
			BBM->_BBM_DBBM_status = DBBM_STATE_Down;
		else if (BBM->_BBM_DBBM_status == DBBM_STATE_NA)
			BBM->_BBM_DBBM_status = DBBM_STATE_MayDown;
		else if (BBM->_BBM_DBBM_status == DBBM_STATE_OK)
			BBM->_BBM_DBBM_status = DBBM_STATE_NA;
	}

normal:
	int retries = 60;
        while (!rpc_mgr.send_msg(msg, NULL, SGNOREPLY | SGNOBLOCK | SGSIGRSTRT)) {
                if (SGT->_SGT_error_code == SGENOGWS && retries--) {
                        sg_ste& ste_mgr = sg_ste::instance(SGT);
                        sg_ste_t ste;

                        ste.grpid() = SGC->_SGC_proc.grpid;
                        ste.svrid() = GWS_PRCID;
                        if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) == 1 && (ste.global.status & RESTARTING)) {
                                sleep(1);
                                continue;
                        }
                }

		GPP->write_log((_("WARN: O_IMOK message could not be sent to sgclst - {1}") % SGT->strerror()).str(SGLOCALE));
		return;
	}
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

		SGP->_SGP_bbasw_mgr = new bbm_bbasw();
		SGP->_SGP_ldbsw_mgr = new bbm_ldbsw();
		SGP->_SGP_authsw_mgr = new bbm_authsw();
		SGP->_SGP_switch_mgr = new bbm_switch();

		sgt_ctx *SGT = sgt_ctx::instance();

		if (SGT->_SGT_mgr_array[RTE_MANAGER]) {
			delete SGT->_SGT_mgr_array[RTE_MANAGER];
			SGT->_SGT_mgr_array[RTE_MANAGER] = NULL;
		}

		SGT->_SGT_mgr_array[RTE_MANAGER] = new bbm_rte();
		SGT->_SGT_mgr_array[CHK_MANAGER] = new bbm_chk();
		SGT->_SGT_mgr_array[BRPC_MANAGER] = new bbm_rpc();
	}
};

static derived_switch_initializer initializer;

}
}

