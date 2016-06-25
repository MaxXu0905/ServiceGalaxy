#include "sg_internal.h"

namespace ai
{
namespace sg
{

sg_viable& sg_viable::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_viable *>(SGT->_SGT_mgr_array[VIABLE_MANAGER]);
}

int sg_viable::inviable(sg_ste_t *s, int& spawn_flags)
{
	return (this->*SGT->SGC()->_SGC_ldbsw->viable)(s, spawn_flags);
}

/*
 * This routine returns an indication of whether or not the
 * given server is inviable. Return values include:
 *
 *	-1	-  internal error or server is gone for good
 *	0	-  server is viable
 *	1	-  server is inviable but restarting
 *
 */
int sg_viable::upinviable(sg_ste_t *s, int& spawn_flags)
{
	sg_qte_t q;		/* server's QTE */
	sg_ste_t p;		/* server's parent STE */
	time_t now = 0;
	int restarting = -1;
	sg_rte_t *reg;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_rte& rte_mgr = sg_rte::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("spawn_flags={1}") % spawn_flags).str(SGLOCALE), &retval);
#endif

	if ((s->global.status & SUSPENDED)
		&& !(s->global.status & (RESTARTING | CLEANING | SHUTDOWN))) {
		SGT->_SGT_error_code = SGESVRSTAT;
		return retval;
	}

	// don't restart migrating server
	if (s->global.status & MIGRATING) {
		retval = 0;
		return retval;
	}

	// Partitioned server is not viable
	if (s->global.status & PSUSPEND) {
		SGT->_SGT_error_code = SGESVRSTAT;
		return retval;
	}

	if (s->global.status & RESTARTING) {
		restarting = 1;

		// avoid multiple instances of restartsrv
		now = time(0);
		if (s->global.ctime + 1 + bbp->bbparms.scan_unit / 2 > now) {
			retval = 1;
			return retval;
		}
	}

	/*
	 * If already marked as restarting or cleaning,
	 * and we're a mere mortal, return an indication
	 * of the status.
	 *
	 * If we are a sgmngr, then start a restartsrv or
	 * cleanupsrv process if one has not already
	 * been started (i.e. IDLE is still set in status).
	 */

	if ((s->global.status & (RESTARTING | CLEANING)) && !SGP->_SGP_ambbm) {
		SGT->_SGT_error_code = SGESVRSTAT;
		retval = restarting;
		return retval;
	} else if (!(s->global.status & IDLE)) {
		if (s->global.status & RESTARTING) {
			/* Before we return, set the status
			 * back to non-bogus (IDLE flag on) --
			 * to keep a STE from staying in a
			 * "stuck" status (IDLE off, RESTARTING
			 * on), with no way to escape.
			 * This will cause a restartsrv process
			 * to be spawned next bbclean.
			 */
			s->global.status |= IDLE;
			if (ste_mgr.update(s, 1, U_LOCAL) < 0) {
				GPP->write_log(__FILE__, __LINE__, (_("WARN: Bulletin Board update error - {1}") % SGT->strerror()).str(SGLOCALE));
			}
		} else if (s->global.status & CLEANING) {
			/*
			 * IDLE off, CLEANING on means a previous cleaupsrv
			 * was not able to do the job. Try again.
			 */
			q.hashlink.rlink = s->queue.qlink;
			if (qte_mgr.retrieve(S_BYRLINK, &q, &q, NULL) < 0)
				return retval;

			if (clnupsvr(&q, s, spawn_flags) < 0)
				dltsvr(&q, s);
		}

		SGT->_SGT_error_code = SGESVRSTAT;
		return retval;
	}

	// check status of server's parent.. if there is one
	if ((p.hashlink.rlink = s->family.plink) != -1) {
		if (ste_mgr.retrieve(S_BYRLINK, &p, &p, NULL) < 0) {
			// orphaned entry?
			return retval;
		}
		// check to see sgmngr is suspended or partition suspend
		if ((p.global.status & SUSPENDED) || (p.global.status & PSUSPEND)) {
			SGT->_SGT_error_code = SGEBBINSANE;
			return retval;
		}
	}

	if (!SGC->_SGC_amadm && !SGP->_SGP_ambbm) {
		retval = 0;
		return retval;
	}

	/*
	 * BBMs need only continue for processes local to themselves, i.e.,
	 * those whose failures they can do something about (restart/cleanup).
	 */
	if (s->mid() != SGC->_SGC_proc.mid) {
		retval = 0;
		return retval;
	}

	q.hashlink.rlink = s->queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &q, &q, NULL) < 0)
		return retval;

	// check that process is actually there

	SGT->_SGT_error_code = 0;
	if (!proc_mgr.alive(s->svrproc()) || (s->global.status & SVRKILLED)) {
		if (SGT->_SGT_error_code == SGENOGWS) {
			retval = 0;
			return retval;
		}

		GPP->write_log((_("WARN: Process {1}/{2} terminated")
			% s->grpid() % s->svrid()).str(SGLOCALE));
		goto NOT_VIABLE;
	}

	retval = 0;
	return retval;

NOT_VIABLE:
	/*
	 * The following code changes the status of the
	 * passed in STE to either RESTARTING or CLEANING
	 * as appropriate. In either case, this requires
	 * a global BB update (i.e. every copy of the BB
	 * must be updated). If there is only one copy of
	 * the BB (i.e. MODEL=SHM) or we are a sgmngr, then
	 * we can do the update here; otherwise, we update
	 * the entry locally and recommend a bbclean run to
	 * the sgmngr on the machine where this server resides.
	 * Updating the entry locally ensures that no other
	 * process on this PE will complain to the "owning"
	 * sgmngr.
	 *
	 * In any case, return an indication
	 * of whether or not this server is restarting.
	 */

	if (!now)
		now = time(0);

	/*
	 * Set any in progress transaction for which the dead server was
	 * the committer to timeout as soon as possible rather than waiting for the
	 * registered timeout interval to expire. we only handle restartable server
	 * here, for non-restartable servers, _tmsmclean will handle transaction
	 * timeout issue if the server was a committer.
	 */
	if (q.parms.options & RESTARTABLE) {
		/*
		  * Give maxgen more lives if
		  * grace period has expired.
		  * Note that a 0 grace period is ALWAYS exceeded. We
		  * need the extra check in case the ctime and now time
		  * are computed on different machines and are out of
		  * sync.
		  */
		if ((now - s->global.ctime) > q.parms.grace || !q.parms.grace) {
			s->global.gen = 0;
			s->global.ctime = now;
		}
    }

	if (!(q.parms.options & RESTARTABLE)
		|| (s->global.status & SHUTDOWN)
		|| (s->global.gen >= q.parms.maxgen)) {
		if (q.parms.options & RESTARTABLE) {
			/*
			  * Restartable and gen is still greater than
			  * maxgen, then exceeded MAXGEN limit.
			  * Generate a system event.
			  */
     		if (s->global.gen >= q.parms.maxgen && q.parms.maxgen) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: {1}, group {2}, id {3} process exceeded MAXGEN LIMIT")
					% q.parms.filename % s->grpid() % s->svrid()).str(SGLOCALE));
     		}
		}

		if (s->global.gen || (s->global.status & SHUTDOWN)) {
			/*
			 * No restart provided by user, or generation
			 * count exceeded or shutting down.
			 */
			s->global.status |= CLEANING;
			s->global.ctime = now;

			/*
			 * Non-SHM implementation; do a local update
			 * and recommend a bbclean().
			 */

			if (!SGP->_SGP_ambbm) {
				// put out of order for mere mortals
				s->global.status |= SUSPENDED;
				if (ste_mgr.update(s, 1, U_LOCAL) < 0) {
					GPP->write_log((_("WARN: Bulletin Board update error - {1}")
						% SGT->strerror()).str(SGLOCALE));
				}
				sg_switch::instance().bbclean(SGT, s->mid(), SVRTAB, spawn_flags);
			} else {
				/*
				 * we are a sgmngr: mark
				 * with a bogus status so that we don't
				 * attempt a second recovery.
				 */

				s->global.status &= ~IDLE;
				if (ste_mgr.update(s, 1, U_LOCAL) < 0) {
					GPP->write_log((_("WARN: Bulletin Board update error - {1}")
						% SGT->strerror()).str(SGLOCALE));
				}
				/*
				 * If server is on same PE and NODE mark as
				 * dead so that sgmngr knows that it can clean
				 * up the registry table entry.
				 */
				if (s->mid() == SGC->_SGC_proc.mid && !SGP->_SGP_amdbbm) {
					reg = s->svrproc().is_dbbm() ? NULL : rte_mgr.find_slot(s->pid());
					if (reg == NULL) {
						cln_no_reg(s);
						return retval;
					}
					reg->rflags |= PROC_DEAD;
				}

				if (clnupsvr(&q, s, spawn_flags) < 0) {
					if (dltsvr(&q, s) < 0) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to delete process {1}/{2}")
							% s->grpid() % s->svrid()).str(SGLOCALE));
					}
				}
			}

			return retval;
		}
		// else grace expired--now restartable
	}

	s->global.status |= RESTARTING;
	s->global.ctime = now;	// timestamp the recovery process

	/*
	 * Non-SHM implementation; do a local update
	 * and recommend a bbclean().
	 */
	if (!SGP->_SGP_ambbm) {
		// put out of order for mere mortals
		s->global.status |= SUSPENDED;
		if (ste_mgr.update(s, 1, U_LOCAL) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to update Bulletin Board - {1}")
				% SGT->strerror()).str(SGLOCALE));
		}
		sg_switch::instance().bbclean(SGT, s->mid(), SVRTAB, spawn_flags);
	} else {
		sg_proc_t currclient = s->local.currclient;

		/*
		 * we are a sgmngr: mark
		 * with a bogus status so that we don't
		 * attempt a second recovery.
		 */
		s->global.status &= ~IDLE;

		/*
		 * In previous incarnations of /T, we did a global update of
		 * the STE here for all but BRIDGE processes.  However, since
		 * restartsrv itself does a global update to instantiate the
		 * new pid, a global update here is unnecessary and error prone.
		 * Therefore, do a local update and let restartsrv take care
		 * of propagation/cleanup.
		 */
		if (ste_mgr.update(s, 1, U_LOCAL) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to update Bulletin Board - {1}")
				% SGT->strerror()).str(SGLOCALE));
		}

		/*
		 * We are restarting a process.  If it is on the same PE and
		 * NODE, then mark its registry table entry as restarting.
		 * If we fail, mark it as dead.  This provides the sgmngr info
		 * on how to process the cleaning of the registry table
		 * entry.
		 */
		if (s->mid() == SGC->_SGC_proc.mid && !SGP->_SGP_amdbbm) {
			/* sgclst has no reg entry even when alive. Without this fix, if I'm a sgmngr,
			 * I'll fall into cln_no_reg->_tmmbdsvrs->tmmbdtmid->_tmmbasnd.
			 * But sgclst is gone, and I'll hang there. So, return -1 immediately.
			 */
			if (SGP->_SGP_ambbm && s->svrproc().is_dbbm())
				return retval;

			reg = rte_mgr.find_slot(s->pid());
			if (reg == NULL) {
				cln_no_reg(s);
				return retval;
			}
			reg->rflags |= PROC_REST;
		}

		if (rstsvr(&q, s, spawn_flags) < 0) {
			// mark as dead
			if (s->mid() == SGC->_SGC_proc.mid && !SGP->_SGP_amdbbm) {
				reg = (s->svrproc().is_dbbm()) ? NULL : rte_mgr.find_slot(s->pid());
				if (reg == NULL) {
					cln_no_reg(s);
					return retval;
				}
				reg->rflags |= PROC_DEAD;
			}
			if (clnupsvr(&q, s, spawn_flags) < 0 && s->svrid() != MNGR_PRCID)
				dltsvr(&q, s);

			return retval;
		}
	}

	retval = 1;
	return retval;	// inviable, restarting
}

int sg_viable::nullinviable(sg_ste_t *s, int& spawn_flags)
{
	int retval = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("spawn_flags={1}") % spawn_flags).str(SGLOCALE), &retval);
#endif
	return retval;
}

// remove resources allocated to the specified server (last resort)
int sg_viable::dltsvr(sg_qte_t *q, sg_ste_t *s)
{
	int i;
	sgid_t sgid;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	if ((sgid = s->hashlink.sgid) == BADSGID) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to determine ID of process to be removed")).str(SGLOCALE));
		return retval;
	}

	if ((i = ste_mgr.remove(sgid)) < 0)
		return retval;

	if (i == 1) {
		sg_proc& proc_mgr = sg_proc::instance(SGT);
		proc_mgr.removeq(s->rpaddr());
		proc_mgr.removeq(s->rqaddr());
	}

	retval = i;
	return retval;
}

int sg_viable::clnupsvr(sg_qte_t *q, sg_ste_t *s, int& spawn_flags)
{
	int mid;
	string args;
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("spawn_flags={1}") % spawn_flags).str(SGLOCALE), &retval);
#endif

	if (s != NULL) {
		if (s->svrid() == MNGR_PRCID) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to clean up sgmngr processes")).str(SGLOCALE));
			return retval;
		}
		if (s->svrproc().is_gws()) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to clean up sggws processes")).str(SGLOCALE));
			return retval;
		}
		if (SGC->remote(s->mid())) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to clean up processes on remote node(s)")).str(SGLOCALE));
			return retval;
		}

		if (q->parms.options & RESTARTABLE) {
			GPP->write_log((_("WARN: Cleaning up restartable process {1}/{2}")
				% s->grpid() % s->svrid()).str(SGLOCALE));
		} else {
			GPP->write_log((_("WARN: Cleaning up process {1}/{2}")
				% s->grpid() % s->svrid()).str(SGLOCALE));
		}

		args = (boost::format("-g %1% -p %2%") % s->grpid() % s->svrid()).str();
		mid = SGC->_SGC_proc.is_bbm() ? SGC->_SGC_proc.mid : s->mid();
	} else  {
		args = "-a";
		mid = SGC->_SGC_proc.mid;
	}

	sg_config& config_mgr = sg_config::instance(SGT);
	sg_mchent_t mchent;
	mchent.mid = mid;
	if (!config_mgr.find(mchent)) {
		GPP->write_log(__FILE__, __LINE__, (_("WARN: SGROOT environment variable not set")).str(SGLOCALE));
		return retval;
	}

	string cmd = (boost::format("%1%/bin/sgclear %2%") % mchent.sgdir % args).str();

	reset_pmid(mid);

	// if caller wants a cleanupsrv process
	if (spawn_flags & SPAWN_CLEANUPSRV) {
		if ((retval = sys_func::instance().new_task(cmd.c_str(), (char **)NULL, _GP_P_NOWAIT)) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to exec {1} - errno={2}") % cmd % errno).str(SGLOCALE));
		} else {
			// set indication that cleanupsrv launched OK
			spawn_flags &= ~SPAWN_CLEANUPSRV;
		}
	} else {
		retval = 0;
	}

	reset_pmid(SGC->_SGC_proc.mid);
	return retval;
}

// attempt to restart a server
int sg_viable::rstsvr(sg_qte_t *q, sg_ste_t *s, int& spawn_flags)
{
	string args;
	int mid;
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("spawn_flags={1}") % spawn_flags).str(SGLOCALE), &retval);
#endif

	GPP->write_log((_("INFO: Process {1}/{2} being restarted") % s->grpid() % s->svrid()).str(SGLOCALE));

	if (SGP->_SGP_ambbm && s->svrid() == GWS_PRCID) {
		// 调整版本
		sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
		rpc_mgr.smlaninc();

		// sghgws的队列可能重建，需要重新打开
		SGP->clear();
	}

	mid = SGC->_SGC_proc.is_bbm() ? SGC->_SGC_proc.mid : s->mid();
	if (SGC->remote(mid)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Restarting a remote process not permitted")).str(SGLOCALE));
		return retval;
	}

	sys_func& func_mgr = sys_func::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_mchent_t mchent;
	mchent.mid = mid;
	if (!config_mgr.find(mchent)) {
		GPP->write_log(__FILE__, __LINE__, (_("WARN: SGROOT environment variable not set for sgrecover process")).str(SGLOCALE));
		return retval;
	}

	/*
	 * Set up the restart command and user specified command. Note
	 * that the PMID environment variable is only necessary in non-3b1500
	 * machines but it does not hurt to set it here. In fact, it makes
	 * startup of restartsrv more efficient by avoiding mid calculations.
	 *
	 * restartsrv is always executed and the user specified restart
	 * command (RCMD parameter in *SERVERS) is executed in parallel if
	 * specified.
	 */
	string cmd = (boost::format("%1%/bin/sgrecover -g %2% -p %3%") % mchent.sgdir % s->grpid() % s->svrid()).str();

	string ucmd;
	if (*q->parms.rcmd && strcmp(q->parms.rcmd, "-") != 0)
		ucmd = (boost::format("%1% %2% %3%") % q->parms.rcmd % s->grpid() % s->svrid()).str();

	/*
	 * Execute the system restartsrv command. If this at least starts
	 * up ok, the go ahead and try to run the user specified RCMD.
	 * The return value from this function is that returned by the
	 * system() call for the system restartsrv command.
	 */
	reset_pmid(mid);

	// if caller wants a restartsrv process
	if (spawn_flags & SPAWN_RESTARTSRV) {
		// try to create one
		if ((retval = func_mgr.new_task(cmd.c_str(), (char **)NULL, _GP_P_NOWAIT)) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to exec {1} - errno={2}") % cmd % errno).str(SGLOCALE));
		} else {
			// set indication that restartsrv launched OK
			spawn_flags &= ~SPAWN_RESTARTSRV;
		}
	} else {
		// simulate success
		retval = 0;
	}

	// if restartsrv started OK, and a user-specified RCMD exists
	if (retval >= 0 && ucmd.empty()) {
		if (func_mgr.new_task(ucmd.c_str(), (char **)NULL, _GP_P_NOWAIT) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to exec {1} application RCMD command - errno={2}") % ucmd % errno).str(SGLOCALE));
		}
	}

	reset_pmid(SGC->_SGC_proc.mid);
	return retval;
}

sg_viable::sg_viable()
{
}

sg_viable::~sg_viable()
{
}

void sg_viable::cln_no_reg(sg_ste_t *ste)
{
	sg_ste& ste_mgr = sg_ste::instance(SGT);

	/*
	 * At this point we already know that the server is dead.  However,
	 * the sgmngr was unable to restart and/or clean it up due to a missing
	 * registry table entry.  At those points, this routine is called,
	 * which deletes the table entries associated with the server.
	 */
	GPP->write_log((_("WARN: Forcing deletion of table entries for process {1}/{2}")
		% ste->grpid() % ste->svrid()).str(SGLOCALE));
	ste_mgr.gdestroy(&ste->hashlink.sgid, 1);
}

void sg_viable::reset_pmid(int mid)
{
	sgc_ctx *SGC = SGT->SGC();
	string pmid_str = (boost::format("PMID=%1%") % SGC->mid2pmid(mid)).str();
	gpenv::instance().putenv(pmid_str.c_str());
}

}
}

