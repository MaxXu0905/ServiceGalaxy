#include "bbm_rpc.h"
#include "bbm_chk.h"

namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

bbm_rpc& bbm_rpc::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<bbm_rpc *>(SGT->_SGT_mgr_array[BRPC_MANAGER]);
}

int bbm_rpc::bbmsnd(message_pointer& msg, int flags)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif
	sgc_ctx *SGC = SGT->SGC();
	boost::unordered_map<int, registry_t>::iterator iter;

	int bcnt = broadcast(msg);

	if (!(flags & SGAWAITRPLY)) {
		if (bcnt != BBM->_BBM_regmap.size())
			cleanup();

		retval = bcnt;
		return retval;
	}

	int rcnt = getreplies(bcnt);
	if (rcnt < bcnt) {
		for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
			registry_t& regent = iter->second;

			if (regent.status & (ACK_DONE | NACK_DONE | PARTITIONED | BCSTFAIL))
				continue;

			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr broadcast reply timeout, node= {1} opcode= {2}")
				% SGC->pmtolm(regent.proc.mid) % curopcode).str(SGLOCALE));
			regent.status |= BCSTFAIL;
		}
	}

	if (bcnt != BBM->_BBM_regmap.size() || rcnt != BBM->_BBM_regmap.size())
		cleanup();

	retval = rcnt;
	return retval;
}

/*
 *  broadcast a message to all non-partitioned BBMs.
 *		The number of messages sent is returned.
 *		After this call finishes currbcst contains a unique
 *		reply iteration number that identifies responses.
 */
int bbm_rpc::broadcast(message_pointer& msg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	boost::unordered_map<int, registry_t>::iterator iter;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	// save the opcode to make userlog messages more helpful in case of error.
	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	curopcode = (rqst->opcode & ~VERSMASK);

	/*
	 *  Sending broadcast messages is limited to DBBM_WAIT*SCAN_UNIT
	 *  seconds, to protect against the unlikely event of message
	 *  queue blocking.
	 */
	int timeout = bbp->bbparms.dbbm_wait * bbp->bbparms.scan_unit;
	time_t etime = time(0) + timeout;

	/*
	 * Turn on the METAHDR_FLAGS_DBBMFWD bit in the metahdr flags to keep errors
	 * relating to this broadcast from "bouncing" back to us.
	 */
	sg_metahdr_t& metahdr = *msg;
	metahdr.flags |= METAHDR_FLAGS_DBBMFWD;

	/*
	 * Before sending the message, see if we need to update any version
	 * numbers and if so, populate the message header fields that will
	 * indicate to our BBMs the new and previous version numbers.
	 */
	sg_msghdr_t& msghdr = *msg;
	msghdr.alt_flds.prevbbv = bbp->bbparms.bbversion;
	msghdr.alt_flds.prevsgv = -1; // Not changed
	switch (curopcode) {
	case O_UDBBM:		// These opcodes update the config file
	case O_USGTE:
		// SGPROFILE version number update
		msghdr.alt_flds.prevsgv = bbp->bbparms.sgversion;
		break;
	}

	// O_UPLOCAL不更新版本，不需应答
	if (curopcode == O_UPLOCAL) {
		msghdr.rplymtype = NULLMTYPE;
	} else {
		if (rpc_mgr.smubbver() < 0)
			return retval;

		// attach sequence #
		msghdr.rplyiter = ++cmsgid;
	}

	msghdr.alt_flds.newver = bbp->bbparms.bbversion;
	if (msghdr.alt_flds.prevsgv != -1)
		updatevs(bbp->bbparms.bbversion);

	int cnt = 0;
	for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
		registry_t& regent = iter->second;

		if (regent.status & PARTITIONED) {
			// Ignore partitioned BBMs
			continue;
		}

		if (regent.status & BCSTFAIL) {
			/*
			 *  Ignore BBMs that had recent broadcast
			 *  failures.  They will be cleaned up
			 *  (partitioned) soon.
			 */
			continue;
		}

		// clear status bits from previous broadcasts
		regent.status &= ~(ACK_DONE | NACK_DONE);

		if (time(0) >= etime) {
			// Timeout occurred, probably message queue blocking
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr broadcast send timeout, node = {1} opcode = {2}")
				% SGC->pmtolm(regent.proc.mid) % curopcode).str(SGLOCALE));
			regent.status |= BCSTFAIL;
			continue;
		}

		msghdr.rcvr = regent.proc;
		metahdr.mid = msghdr.rcvr.mid;
		metahdr.qaddr = msghdr.rcvr.rqaddr;
		strcpy(msghdr.svc_name, BBM_SVC_NAME);

		if (!ipc_mgr.send(msg, SGNOTIME)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr broadcast send failure, node = {1} error = {2} opcode = {3}") % SGC->pmtolm(regent.proc.mid) % SGT->strerror() % curopcode).str(SGLOCALE));
			regent.status |= BCSTFAIL;
			continue;
		}

		cnt++;  // Count messages sent OK
	}

	retval = cnt;
	return retval;
}

// Collect all responses from previous message sent by broadcast().
// The number of replies is returned.
int bbm_rpc::getreplies(int cnt)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	boost::unordered_map<int, registry_t>::iterator iter;
	int ackcnt = 0;  /* Number of positive replies rcvd */
	int rplycnt = 0; /* Number of replies rcvd */
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif

	if (rply_msg == NULL)
		rply_msg = rpc_mgr.create_msg(sg_rpc_rply_t::need(0));

	/*
	 *  Time to collect all replies is limited to DBBM_WAIT*SCAN_UNIT
	 *  seconds.
	 */
	int timeout = bbp->bbparms.dbbm_wait * bbp->bbparms.scan_unit;
	time_t etime = time(0) + timeout;

	while (rplycnt < cnt && time(0) < etime) {
		sg_msghdr_t *msghdr = *rply_msg;
		msghdr->rplymtype = sg_metahdr_t::bbrplymtype(SGC->_SGC_proc.pid);
		msghdr->rcvr = SGC->_SGC_proc;

		if (!ipc_mgr.receive(rply_msg, SGREPLY, timeout)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot get sgmngr broadcast replies - {1}") % SGT->strerror()).str(SGLOCALE));
			break;
		}

		msghdr = *rply_msg;
		sg_rpc_rply_t *rply = rply_msg->rpc_rply();

		// Search for the sgmngr sending back this reply message.
		registry_t *regent = NULL;
		for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
			regent = &iter->second;

			if (regent->proc == msghdr->sender)
				break;
		}

		if (iter == BBM->_BBM_regmap.end()) {
			GPP->write_log(__FILE__, __LINE__, (_("WARN: Stray {1} received from process {2}")
				% (rply->rtn < 0 ? "NACK" : "ACK") % msghdr->sender.pid).str(SGLOCALE));
			continue;
		}

		if (regent->status & (PARTITIONED | BCSTFAIL))
			continue;  // Ignore BBMs not sent the broadcast

		if (msghdr->rplyiter != cmsgid) {
			GPP->write_log((_("WARN: Stale message received from sgmngr on node {1} opcode = {2}")
				% SGC->pmtolm(regent->proc.mid) % curopcode).str(SGLOCALE));
			continue;	// Ignore stale message and keep trying
		}

		rplycnt++;	// This is a valid reply message

		if (msghdr->flags & SGNACK) {
			/*
			 *  The service failed.  Partition this sgmngr
			 *  since something is very wrong.
			 */
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr broadcast operation failure, node= {1} opcode= {2}")
				% SGC->pmtolm(regent->proc.mid) % curopcode).str(SGLOCALE));
			regent->status |= BCSTFAIL;
			continue;
		}

		if (rply->rtn < 0) {
			/*
			 *  The RPC request failed.  Force this sgmngr to
			 *  upload a new BB since it's now out of sync.
			 */
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr broadcast RPC failure, node= {1} opcode= {2}")
				% SGC->pmtolm(regent->proc.mid) % curopcode).str(SGLOCALE));
			regent->status |= NACK_DONE;
			continue;
		}

		regent->status |= ACK_DONE;	// Success!
		ackcnt++;
	}

	retval = ackcnt;
	return retval;
}

/*
 *  cleanup:  take corrective action for BBMs that are in trouble,
 *	      as indicated by regtbl[x].status bits.
 *
 *	      NACK_DONE   --> force upload of new BB
 *	      BCSTFAIL    --> partition this sgmngr
 *	      PARTITIONED --> leave it alone
 *			      (sgclst receiving O_IMOK msg will unpartition it)
 */
void bbm_rpc::cleanup()
{
	int fail_cnt = 0;  /* Count of BCSTFAIL machines */
	int part_cnt = 0;  /* Count of PARTITIONED machines */
	int nack_cnt = 0;  /* Count of NACK_DONE machines */
	int ok_cnt = 0;    /* Count of machines not PARTITIONED */
	int cfg_cnt = 0;	/* Count of machines that need new sgconfig */
	sg_ste_t ste;	   /* sgclst STE */
	sgid_t *sgids;	   /* Array of TMIDs for O_VUSTAT */
	sg_rpc_rqst_t *rqst;	   /* User data of bmsg */
	boost::unordered_map<int, registry_t>::iterator iter;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_config& config_mgr = sg_config::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", NULL);
#endif

	/*
	 *  PHASE ONE:  take care of BBMs with NACK_DONE or SGCFGFAIL set.
	 *  ALL of these must be handled first, since BCSTFAIL logic results
	 *  in another call to broadcast() which then clears NACK_DONE.
	 */
	for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
		registry_t& regent = iter->second;

		if (regent.status & NACK_DONE)
			nack_cnt++;
		if (regent.status & SGCFGFAIL)
			cfg_cnt++;
	}

	if (cfg_cnt > 0) {
		for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
			registry_t& regent = iter->second;

			if (!(regent.status & SGCFGFAIL))
				continue;  // Skip all but SGCFGFAIL

			GPP->write_log(__FILE__, __LINE__, (_("INFO: Sending new SGPROFILE file to node {1}")
				% SGC->pmtolm(regent.proc.mid)).str(SGLOCALE));

			// Propagation SGPROFILE
			sg_mchent_t mchent;
			mchent.mid = regent.proc.mid;
			if (!config_mgr.find(mchent)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to find configuration information on node {1}")
					% regent.proc.mid).str(SGLOCALE));
				continue;
			}

			if (!config_mgr.propagate(mchent)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to propagate updated configuration information to node {1}") % SGC->pmtolm(regent.proc.mid)).str(SGLOCALE));
				continue;
			}

			regent.sgvers = bbp->bbparms.sgversion;
		}
	}

	if (nack_cnt > 0) {
		/*
		 *  Reset the sgclst STE's current client and service to NULL
		 *  before sending it out.
		 */
		ste.svrid() = MNGR_PRCID;
		ste.grpid() = CLST_PGID;
		if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) == 1) {
			ste.local.currclient.clear();
			ste.local.currservice[0] = '\0';
			ste_mgr.update(&ste, 1, 0);
		}

		message_pointer rqst_msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(BBM->_BBM_pbbsize));
		sg_msghdr_t& msghdr = *rqst_msg;
		rqst = rqst_msg->rpc_rqst();

		for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
			registry_t& regent = iter->second;

			if (!(regent.status & NACK_DONE))
				continue;  // Skip all but NACK_DONE

			GPP->write_log(__FILE__, __LINE__, (_("INFO: Sending sgmngr on node {1} new version of bulletin board")
				% SGC->pmtolm(regent.proc.mid)).str(SGLOCALE));

			/*
			 *  To prevent the master machine's sggws or network
			 *  buffers from being flooded during migration,
			 *  do a short sleep() here.  Then confirm that the
			 *  intended sgmngr can be reached over the network.
			 */
			sleep(bbp->bbparms.scan_unit);

			if (!proc_mgr.nwkill(regent.proc, 0, bbp->bbparms.scan_unit)) {
				regent.status |= BCSTFAIL;
				continue;
			}

			// Prepare an O_UPLOAD message.
			msghdr.rplymtype = NULLMTYPE;	// no reply expected

			rqst->opcode = O_UPLOAD | VERSION;
			memcpy(rqst->data(), &bbp->bbmeters, BBM->_BBM_pbbsize);
			rqst->datalen = BBM->_BBM_pbbsize;
			rqst->arg1.flag = 0;
			rqst->arg2.vers = bbp->bbparms.bbversion;

			if (!rpc_mgr.send_msg(rqst_msg, &regent.proc, SGSIGRSTRT)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr upload request not sent, node = {1} - {2}")
					% SGC->pmtolm(regent.proc.mid) % SGT->strerror()).str(SGLOCALE));

				// Partition this sgmngr if upload fails.
				regent.status |= BCSTFAIL;
				regent.status &= ~NACK_DONE;
				continue;
			}
			regent.bbvers = bbp->bbparms.bbversion;

			// Clear the NACK_DONE bit
			regent.status &= ~NACK_DONE;
		}
	}

	/*
	 *  PHASE TWO:  Take care of BCSTFAIL machines.
	 *  If ANY machine has state BCSTFAIL, the status of ALL machines
	 *  are broadcast -- both partitioned and unpartitioned.  This may
	 *  be redundant, but gives the BBMs more accurate info in case
	 *  previous messages were lost or incorrect.
	 */
	for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
		registry_t& regent = iter->second;

		if (regent.status & BCSTFAIL) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr partitioned, node= {1}")
				% SGC->pmtolm(regent.proc.mid)).str(SGLOCALE));
			fail_cnt++;
			rpc_mgr.ustat(&regent.sgid, 1, PSUSPEND, STATUS_OP_OR);
			regent.status = PARTITIONED;
		}

		if (regent.status & PARTITIONED)
			part_cnt++;
	}

	if (fail_cnt > 0) {
		// Broadcast an O_VUSTAT message listing all PARTITIONED machines.
		message_pointer rqst_msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(BBM->_BBM_pbbsize));
		int datalen = part_cnt * sizeof(sgid_t);

		sg_msghdr_t& msghdr = *rqst_msg;
		msghdr.rplymtype = NULLMTYPE;	// no reply expected

		rqst = rqst_msg->rpc_rqst();
		rqst->opcode = O_VUSTAT | VERSION;
		rqst->datalen = datalen;
		rqst->arg1.flag = STATUS_OP_OR;
		rqst->arg2.status = PSUSPEND;
		rqst->arg3.sgids = part_cnt;

		sgids = reinterpret_cast<sgid_t *>(rqst->data());
		for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
			registry_t& regent = iter->second;

			if (regent.status & PARTITIONED)
				*sgids++ = regent.sgid;
		}

		broadcast(rqst_msg);

		// Broadcast an O_VUSTAT message listing all UNPARTITIONED machines.
		for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
			registry_t& regent = iter->second;

			if (!(regent.status & PARTITIONED)) {
				ok_cnt++;
				*sgids++ = regent.sgid;
			}
		}

		datalen = ok_cnt * sizeof(sgid_t);
		rqst_msg->set_length(sg_rpc_rqst_t::need(datalen));

		rqst->datalen = datalen;
		rqst->opcode = O_VUSTAT | VERSION;
		rqst->arg1.flag = STATUS_OP_AND;
		rqst->arg2.status = ~PSUSPEND;
		rqst->arg3.sgids = ok_cnt;

		broadcast(rqst_msg);
	}

	sg_agent::instance(SGT).close(ALLMID);
}

/*
 * NET/T specific routine to clean up all server and service entries from
 * a partitioned PE or node.
 */
void bbm_rpc::pclean(int deadpe, sg_rpc_rply_t *rply)
{
	sgc_ctx *SGC = SGT->SGC();
	boost::unordered_map<int, registry_t>::iterator iter;
	sg_ste_t ste;
	sg_sgte_t sgte;
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	bool partitioned = true;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", NULL);
#endif

	rply->rtn = 0;	/* Initialize the count of removed servers */
	rply->num = 0;	/* Initialize the count of possibile servers */

	/*
	 * If we're the sgclst, then it's our responsibility to make sure
	 * it's ok to cleanup. To do that we need to make sure that the
	 * sgmngr is marked PARTITIONED AND that it's either dead or that
	 * the _tmprocalive times out.  If we're the sgmngr, then we know that
	 * the sgclst already said it's ok, so go ahead without checking the
	 * sgmngr
	 */
	if (SGP->_SGP_amdbbm) {
		iter = BBM->_BBM_regmap.find(deadpe);
		if (iter == BBM->_BBM_regmap.end()) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot retrieve sgmngr entry for pclean operation")).str(SGLOCALE));
			rply->rtn = -1;
			return;
		}

		registry_t& regent = iter->second;
		/*
		 * If it's PARTITIONED, then we still want to drop through
		 * and see how many we would have cleaned if we did so that
		 * sgadmin can report accurately.  First check the
		 * PARTITIONED bit, if that's set then the sgmngr must be
		 * unreachable.  What does unreachable mean ?  It means that
		 * a alive() on the sgmngr returns success and
		 * sets tperrno to SGETIME.  A sgmngr that is dead but whose
		 * site is reachable is not a candidate for PCLEAN processing
		 * since it will eventually restart and rejoin the system.
		 * Don't check for SGENOGWS error because this causes
		 * sites that are partitioned but that have not yet gotten
		 * a sggws disconnect to appear connected.
		 */
		if ((partitioned = (regent.status & PARTITIONED))) {
			SGT->_SGT_error_code = 0;
			if (proc_mgr.alive(regent.proc) && SGT->_SGT_error_code != SGETIME) {
				partitioned = false;
				GPP->write_log(__FILE__, __LINE__, (_("INFO: pclean - {1} now reachable and not removed from bulletin board")
					% SGC->mid2lmid(deadpe)).str(SGLOCALE));
			}
		} else {
			GPP->write_log(__FILE__, __LINE__, (_("INFO: pclean - {1} not partitioned and not removed from bulletin board")
				% SGC->mid2lmid(deadpe)).str(SGLOCALE));
		}
	}

	{
		scoped_bblock bblock(SGT);

		// Make space for svrsgids tables
		boost::shared_array<sgid_t> auto_svrsgids(new sgid_t[SGC->MAXSVRS()]);
		sgid_t *svrsgids = auto_svrsgids.get();

		/*
		 * Populate svrsgids tables.
		 * Retrieve all of the servers residing
		 * on that machine.
		 */
		/*
		 * Retrieve server table entries for native deadpe and
		 * populate the svrsgids table.
		 */
		ste.mid() = deadpe;
		int svrcnt = ste_mgr.retrieve(S_MACHINE, &ste, NULL, svrsgids);

		// Cleanup sgmngr and server entries on deadpe
		for (int i = 0; i < svrcnt; i++) {
			if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, &svrsgids[i]) != 1)
				continue;

			if (ste.grpid() == CLST_PGID)
				continue;

			rply->num++;	// It's a possibility so count it
			if (!partitioned)
				continue;

			if (ste_mgr.remove(svrsgids[i]) < 0) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not remove process entry for sgid {1}")
					% svrsgids[i]).str(SGLOCALE));
			} else {
				if (SGP->_SGP_amdbbm && ste.svrid() == MNGR_PRCID) {
					if (iter != BBM->_BBM_regmap.end()) {
						BBM->_BBM_regmap.erase(iter);
						iter = BBM->_BBM_regmap.end();
					}
				}
				rply->rtn++;
			}
		}
	}

	if (!SGP->_SGP_amdbbm) {
		bbm_chk& chk_mgr = bbm_chk::instance(SGT);
		chk_mgr.pclean(deadpe);
	} else {
		// 队列可能被重建，需要重新打开
		if (SGC->_SGC_proc.mid == deadpe)
			SGP->clear();
	}
}

// update the version number of the SGPROFILE file
void bbm_rpc::updatevs(time_t now)
{
	try {
		sg_config& config_mgr = sg_config::instance(SGT);

		if (now == 0)
			now = time(0);

		sg_bbparms_t& bbparms = config_mgr.get_bbparms();
		bbparms.sgversion = now;
		config_mgr.flush();

		sgc_ctx *SGC = SGT->SGC();
		SGC->_SGC_bbp->bbparms.sgversion = now;
	} catch (exception& ex) {
	}
}

bool bbm_rpc::pairedgw(const sg_proc_t& proc, sg_ste_t *ste, sgid_t *gwsgid)
{
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_ste_t gwste;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	gwste.grpid() = proc.grpid;
	gwste.svrid() = GWS_PRCID;
	if (ste_mgr.retrieve(S_GRPID, &gwste, &gwste, gwsgid) != 1)
		return retval;

	if (gwste.mid() == proc.mid) {
		if (ste != NULL)
			*ste = gwste;
		retval = true;
		return retval;
	}

	return retval;
}

void bbm_rpc::updatemaster(sg_ste_t *ste)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	int i;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", NULL);
#endif

	const char *lmid = SGC->mid2lmid(ste->mid());
	if (strcmp(lmid, bbp->bbparms.master[bbp->bbparms.current]) == 0) {
		// Master not changing, no need to update anything.
		return ;
	}

	for (i = 0; i < MAX_MASTERS; i++) {
		if (i == bbp->bbparms.current)
			continue;

		if (strcmp(lmid, bbp->bbparms.master[i]) == 0)
			break;
	}

	if (i == MAX_MASTERS) {
		// Problem, current DBB< lmid not master or backup
		GPP->write_log(__FILE__, __LINE__, (_("WARN: Could not update master site information")).str(SGLOCALE));
		return;
	}

	bbp->bbparms.current = i;

	sg_config& config_mgr = sg_config::instance(SGT);
	sg_bbparms_t& bbparms = config_mgr.get_bbparms();

	sg_mchent_t *mchentp = config_mgr.find_by_lmid(lmid);
	if (mchentp == NULL) {
		GPP->write_log(__FILE__, __LINE__, (_("WARN: Could not update master site information completely")).str(SGLOCALE));
	} else {
		SGC->_SGC_master_sgconfig = mchentp->sgconfig;
		SGC->_SGC_master_pmid = mchentp->pmid;
	}

	strcpy(bbparms.master[0], bbp->bbparms.master[i]);
	strcpy(bbparms.master[i], bbp->bbparms.master[0]);

	config_mgr.flush();
}

bool bbm_rpc::udbbm(sg_ste_t *ste)
{
	int cnt;
	sg_qte_t qte;
	sg_scte_t scte;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	scoped_bblock bblock(SGT);

	// STE contains new MID & PID for restarted/migrated sgclst
	if (ste_mgr.update(ste, 1, U_LOCAL) < 0)
		return retval;

	scte.svrid() = MNGR_PRCID;
	scte.grpid() = CLST_PGID;
	scte.parms.svc_name[0] = '\0';
	if ((cnt = scte_mgr.retrieve(S_GRPID, &scte, NULL, NULL)) < 0)
		return retval;

	boost::shared_array<sgid_t> auto_sgids(new sgid_t[cnt + 1]);
	sgid_t *sgids = auto_sgids.get();
	if (scte_mgr.retrieve(S_GRPID, &scte, NULL, sgids) != cnt) {
		SGT->_SGT_error_code = SGESYSTEM;
		return retval;
	}
	/*
	 * Now loop through the service table entry sgids one by one,
	 * retrieving them and updating them individually.
	 */
	for (int i = 0; i < cnt; i++) {
		if (scte_mgr.retrieve(S_BYSGIDS, &scte, &scte, &sgids[i]) != 1)
			continue;

		scte.svrproc() = ste->svrproc();
		scte_mgr.update(&scte, 1, U_VIEW | U_LOCAL);
	}

	// Lastly update the QTE with the new physical queue address
	qte.hashlink.rlink = ste->queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &qte, &qte, NULL) == 1) {
		qte.parms.paddr = ste->rqaddr();
		qte_mgr.update(&qte, 1, 0);
	}

	/*
	 * Now update the master site information in the parms and
	 * the tm_master structure.
	 */
	updatemaster(ste);
	retval = true;
	return retval;
}

void bbm_rpc::resign()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (BBM->_BBM_amregistered) {
		sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
		message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(0));

		sg_rpc_rqst_t *rqst = msg->rpc_rqst();
		rqst->opcode = O_URGSTR | VERSION;
		rqst->datalen = 0;
		rqst->arg1.sgid = SGP->_SGP_ssgid;

		/*
		 * Ask dbbm to globally unregister entry.
		 * Wait for reply so BBMs don't race during
		 * shutdown.
		 */
		if(!rpc_mgr.send_msg(msg, NULL, SGSIGRSTRT | SGAWAITRPLY)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to unregister - remote procedure call failure - can't send request - {1}")
				% SGT->strerror()).str(SGLOCALE));
			return;
		}
	}

	BBM->_BBM_amregistered = false;
}

bbm_rpc::bbm_rpc()
{
	BBM = bbm_ctx::instance();

	cmsgid = 0;
	curopcode = -1;
}

bbm_rpc::~bbm_rpc()
{
}

}
}

