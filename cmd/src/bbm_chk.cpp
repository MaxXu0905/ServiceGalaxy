#include "bbm_chk.h"

namespace ai
{
namespace sg
{

bbm_chk& bbm_chk::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<bbm_chk *>(SGT->_SGT_mgr_array[CHK_MANAGER]);
}

int bbm_chk::chk_bbsanity(ttypes sect, int& spawn_flags)
{
	int inviables = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(400, __PRETTY_FUNCTION__, (_("sect={1}, spawn_flags={2}") % sect % spawn_flags).str(SGLOCALE), &inviables);
#endif

	// check servers with reserved registry entries
	chk_accsrs(ADMIN);

	/*
	 * Don't clean the registry table
	 * if a server is being cleaned up
	 * or is restarting.
	 */

	if (sect == NOTAB || sect == SVRTAB) {
		scoped_bblock bblock(SGT);
		inviables = chk_svrs(spawn_flags);
	}

	/*
	 * We don't want to be the BB owner when we check the accessers
	 * because we may need to clean up a table entry which can require
	 * waiting for the dead process to become the BB owner.
	 */
	if ((sect == NOTAB || sect == REGTAB) && inviables == 0) {
		chk_accsrs(0);
		chk_misc();
	}

	return inviables;
}

/*
 * chk_accsrs:  check status of servers & clients in registry entries
 *
 * input param:
 * flag  --     scope of registry entries to check
 *              0: all
 *              ADMIN: reserved registry entries
 */
void bbm_chk::chk_accsrs(int flag)
{
	sg_rte_t *p, *res, *endp;
	int b;
	bool server_context_entry = false;
	bool same_process = false;
	pid_t pid;
	pid_t save_pid = 0;
	sg_proc_t dummy;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_rte& rte_mgr = sg_rte::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, (_("flags={1}") % flag).str(SGLOCALE), NULL);
#endif

	scoped_syslock syslock(SGT);

	res = SGC->_SGC_rtes + SGC->MAXACCSRS();
	if (flag & ADMIN)
		p = res;
	else if (bbp->bbmap.rte_use == -1)
		p = res;
	else
		p = rte_mgr.link2ptr(bbp->bbmap.rte_use);
	endp = SGC->_SGC_rtes + (SGC->MAXACCSRS() + MAXADMIN);

	dummy = SGC->_SGC_proc;

	/*
	 * In the following loop, the variable b is used to hold the link
	 * value (a long) of the next entry in the in-use accesser list.
	 * It is set in the beginning of the loop because the removal of
	 * a dead accesser takes the current entry out of the in-use list
	 * and we would lose our place.
	 */
	while (p < endp) {
		b = p->nextreg;
		if (p->pid > 0) {
			dummy.pid = p->pid;
			if(!proc_mgr.alive(dummy)) {
				pid = p->pid;
				/*
				 * Is this a server context entry? This
				 * needs to be checked before invoking
				 * tmclean.
				 */
				if (p->rflags & SERVER_CTXT)
					server_context_entry = true;
				else
					server_context_entry = false;

				/*
				 * In the case of a multi-threaded client,
			 	 * we can attempt to suppress the "process
				 * died" message for each of the threads by
				 * comparing the current process id with the
				 * previous one. However, it may be
				 * the case that the threads in a process are
				 * not in sequential registry table slots, so
			 	 * there is no guarentee this test will work.
				 */
				if (p->pid == save_pid)
					same_process = true;
				else
					same_process = false;
				save_pid = p->pid;

				if (rte_mgr.clean(p->pid, p)) {
					/*
					 * There is no need to display this message
					 * for SERVER_CTXT entries, since it is
					 * displayed for the associated SERVER entry.
				 	 * There is also no reason to display it for
					 * each thread in a client process.
					 */
					if (!server_context_entry && !same_process)
						GPP->write_log((_("WARN: Process {1} on slot {2} died; removing from BB") % pid % p->slot).str(SGLOCALE));
				}
			}
		}

		if (p >= res) {
			p++;
			continue;
		}

		if (b != -1)
			p = rte_mgr.link2ptr(b);
		else
			p = res;
	}
}

int bbm_chk::chk_svrs(int& spawn_flags)
{
	sg_ste_t ste;
	int inviables = 0;
	int i;
	int cnt;
	sgid_t *tp;
	int amode;
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_viable& viable_mgr = sg_viable::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(400, __PRETTY_FUNCTION__, (_("spawn_flags={1}") % spawn_flags).str(SGLOCALE), &retval);
#endif

	boost::shared_array<sgid_t> auto_sgids(new sgid_t[SGC->MAXSVRS()]);
	sgid_t *sgids = auto_sgids.get();

	scoped_bblock bblock(SGT);

	if (!SGC->_SGC_proc.is_dbbm()) {
		ste.mid() = SGC->_SGC_proc.mid;
		amode = S_MACHINE;
	} else {
		// sgclst checks all BBMs
		ste.svrid() = MNGR_PRCID;
		amode = S_SVRID;
	}

	if ((cnt = ste_mgr.retrieve(amode, &ste, NULL, sgids)) < 0)
		return retval;

	for(i = 0, tp = sgids; i < cnt; i++,tp++) {
		if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, tp) < 0)
			continue;

		if (ste.grpid() == CLST_PGID)
			continue;	// sgclst handled differently

		switch(viable_mgr.inviable(&ste, spawn_flags)) {
		case 0:
			// viable
			break;
		case 1:
			inviables++;
			continue;
		default:
			if (SGT->_SGT_error_code != SGESVRSTAT)
				inviables++;
			break;
		}
	}

	// don't complain here since the caller will
	if (inviables == cnt) {
		// we're all crazy
		return retval;
	}

	retval = inviables;
	return retval;
}

void bbm_chk::chk_misc()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_misc_t *misc = SGC->_SGC_misc;
	sg_qitem_t *items = misc->items;
	scoped_bblock bblock(SGT);

	for (int cque = misc->que_used; cque != -1;) {
		sg_qitem_t& item = items[cque];
		cque = item.next;

		// 需要等待至少一个周期才释放，重启进程时需要重用
		if (item.flags & PROC_DEAD) {
			if (item.pid > 0 && kill(item.pid, 0) != -1) {
				// 如果正好被重用，则不需要清除
				item.flags &= ~PROC_DEAD;
			} else {
				// 删除消息队列
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: queue {1} is removed since owner {2} is dead") % item.qaddr % item.pid).str(SGLOCALE));
				proc_mgr.removeq(item.qaddr);
			}
		} else if (item.pid <= 0 || kill(item.pid, 0) == -1) {
			item.flags |= PROC_DEAD;
		}
	}
}

void bbm_chk::pclean(int deadpe)
{
	sg_rpc_rqst_t *rqst;
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, (_("deadpe={1}") % deadpe).str(SGLOCALE), NULL);
#endif

	if (BBM->_BBM_gwproc == NULL) {
		if (proc_mgr.getq(SGC->_SGC_proc.mid, SGC->_SGC_wka) >= 0) {
			BBM->_BBM_gwproc = &BBM->_BBM_gwste.svrproc();
			*BBM->_BBM_gwproc = SGC->_SGC_proc;
			BBM->_BBM_gwproc->svrid = GWS_PRCID;
			BBM->_BBM_gwproc->rqaddr = SGC->_SGC_wka;
			BBM->_BBM_gwproc->rpaddr = SGC->_SGC_wka;
		}
	}

	if (BBM->_BBM_gwproc == NULL)
		return;

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(0));

	sg_msghdr_t& msghdr = *msg;
	msghdr.sendmtype = GWMTYP;
	msghdr.rplymtype = NULLMTYPE;
	msghdr.rcvr = *BBM->_BBM_gwproc;

	// set message meta-header
	sg_metahdr_t& metahdr = *msg;
	metahdr.flags |= METAHDR_FLAGS_GW;
	metahdr.mid = msghdr.rcvr.mid;
	metahdr.qaddr = msghdr.rcvr.rqaddr;
	metahdr.mtype = msghdr.sendmtype;

	rqst = msg->rpc_rqst();
	rqst->opcode = OB_PCLEAN | VERSION;
	rqst->datalen = 0;
	rqst->arg1.from = deadpe;
	ipc_mgr.send(msg, 0);
}

/*
 * After checking for transaction timeout,
 * this routine goes through the registry table entry list and decrements
 * the timeleft field for blocking processes.  If this becomes less than
 * or equal to zero, then the process is sent a message to be woken up.
 */
void bbm_chk::chk_blockers()
{
	sg_ste_t tmpste, *ste;
	sg_rte_t *rte, *res, *endp;
	sg_proc_t dummy;
	time_t timebuf;
	struct tm *tmptr;
	time_t decrement;
	int spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_switch& switch_mgr = sg_switch::instance();
	sg_rte& rte_mgr = sg_rte::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", NULL);
#endif

	/*
	 * Check if new day - if so, write message to userlog
	 * so that version information is logged.
	 */
	timebuf = bbp->bbmeters.timestamp;
	tmptr = localtime(&timebuf);

	if (tmptr->tm_mday != BBM->_BBM_oldmday) {
		/*
		 * Don't log when process first starts since startup
		 * message will already cause version information to
		 * be written.
		 */
		if (BBM->_BBM_oldmday) {
			// cause version info to print
			GPP->_GPP_ver_flag = false;
			GPP->write_log("");
		}
		BBM->_BBM_oldmday = tmptr->tm_mday;
	}

	scoped_syslock syslock(SGT);

	res = SGC->_SGC_rtes + SGC->MAXACCSRS();	// begin of reserved
	if (bbp->bbmap.rte_use == -1)
		rte = res;
	else
		rte = rte_mgr.link2ptr(bbp->bbmap.rte_use);
	endp = SGC->_SGC_rtes + (SGC->MAXACCSRS() + MAXADMIN);

	message_pointer msg = sg_message::create(SGT);
	if (msg == NULL) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure")).str(SGLOCALE));
		return;
	}

	msg->set_length(0);
	SGC->inithdr(*msg);
	sg_metahdr_t& metahdr = *msg;
	sg_msghdr_t& msghdr = *msg;
	metahdr.flags = METAHDR_FLAGS_ALARM;

	decrement = time(0) - BBM->_BBM_interval_start_time;
	if (decrement < 0)
		decrement = 0;

	/*
	 * First scan for services which have timed out and RPC operations
	 * which have timed out.
	 */
	scoped_bblock bblock(SGT);

	while (rte < endp) {
		if (rte->rt_svctimeout > 0 && (rte->rflags & (SERVER | SERVER_CTXT))) {
			// Processing a service, decrement time left to svc
			if (rte->rt_svctimeout <= decrement) {
				int	svr_signal = SIGTERM;
				rte->rflags |= R_SENT_SIGTERM;

				dummy.mid = SGC->_SGC_proc.mid;
				dummy.pid = rte->pid;
				if (!proc_mgr.nwkill(dummy, svr_signal, 5)) {
					/*
					 * If the process doesn't exist, the server information of the
					 * service timed out server is not cleaned up before the server exit
					 * or is killed. Do BB cleanup for that dead server immediately
					 * to avoid subsequent signals sending to dead process.
					 */
					if (SGT->_SGT_error_code == SGEOS && errno == ESRCH) {
						// the server process doens't exist

						/*
						 * status with TMSVCTIMEDOUT
						 * indicates timeout occured
						 * so sgclear can detect a
						 * timeout for the errordetail
						 */
						tmpste.grpid() = rte->rt_grpid;
						tmpste.svrid() = rte->rt_svrid;
						if (ste_mgr.retrieve(S_GRPID, &tmpste, &tmpste, NULL) == 1) {
							ste = ste_mgr.link2ptr(tmpste.hashlink.rlink);
							ste->global.status |= SVCTIMEDOUT | SVRKILLED;
						}

						/*
						 * Note that we do not reset
						 * spawn_flags before this
						 * call, so that multiple calls
						 * will result in at most one
						 * restartsrv and one
						 * sgclear process.
						 */
						switch_mgr.bbclean(SGT, SGC->_SGC_proc.mid, NOTAB, spawn_flags);
						rte->rt_svctimeout = 0;
					} else {
						// the signal cannot be sent to the active server process correctly
						GPP->write_log((_("WARN: Could not terminate process({1}) processing after SVCTIMEOUT")
							% rte->pid).str(SGLOCALE));
					}
				} else {
					if (svr_signal == SIGKILL) {
						GPP->write_log((_("WARN: Process({1}) processing terminated with SIGKILL after SVCTIMEOUT")
							% rte->pid).str(SGLOCALE));
					} else {
						GPP->write_log((_("WARN: Process({1}) processing terminated with SIGTERM after SVCTIMEOUT")
							% rte->pid).str(SGLOCALE));
					}

					if (svr_signal == SIGKILL) {
						/*
						 * status with TMSVCTIMEDOUT
						 * indicates timeout occured
						 * so sgclear can detect a
						 * timeout for the errordetail
						 */
						tmpste.grpid() = rte->rt_grpid;
						tmpste.svrid() = rte->rt_svrid;
						if (ste_mgr.retrieve(S_GRPID, &tmpste, &tmpste, NULL) == 1) {
							ste = ste_mgr.link2ptr(tmpste.hashlink.rlink);
							ste->global.status |= SVCTIMEDOUT;
						}
						/*
						 * Note that we do not reset
						 * spawn_flags before this
						 * call, so that multiple calls
						 * will result in at most one
						 * restartsrv and one
						 * sgclear process.
						 */
						switch_mgr.bbclean(SGT, SGC->_SGC_proc.mid, NOTAB, spawn_flags);
						rte->rt_svctimeout = 0;
					}
				}
			} else {
				// decrement by actual time since last scan
				rte->rt_svctimeout -= decrement;
			}
		}

		for (int i = 0; i < MAXHANDLES; i++) {
			int m_index = rte->slot * MAXHANDLES + i;
			/*
			 * rte->hndl.bbscan_mtype[i] may be reset by the
			 * accessor, because no system lock will be hold by both sgmngr and accessor
			 * so mtype has to be cached in case it is reset to 0 before usage
			 */
			int wkmtype;
			if ((wkmtype = rte->hndl.bbscan_mtype[i]) > 0) {
				// found a blocker
				if (rte->hndl.bbscan_timeleft[i] <= decrement && BBM->_BBM_rte_scan_marks.test(m_index)) {
					// timeout
					rte->hndl.bbscan_timeleft[i] = 0;
					BBM->_BBM_rte_scan_marks.reset();
					metahdr.mtype = wkmtype;
					msghdr.rplyiter = rte->hndl.bbscan_rplyiter[i];
					metahdr.qaddr = rte->hndl.qaddr;

					if (!ipc_mgr.send_internal(msg, SGSIGRSTRT | SGNOBLOCK)) {
						if (SGT->_SGT_error_code != SGEBLOCK) {
							GPP->write_log((_("WARN: sgmngr failed to wake up the blocking process - {1}")
								% rte->pid).str(SGLOCALE));
						}
					}
				} else {
					// decrement by actual time since last scan
					if (BBM->_BBM_rte_scan_marks.test(m_index))
						rte->hndl.bbscan_timeleft[i] -= decrement;
					else
						rte->hndl.bbscan_timeleft[i] -= bbp->bbparms.scan_unit;
					BBM->_BBM_rte_scan_marks.set(m_index);
				}
			} else {
				if ( rte->pid != 0) {
					/* 3-per blocktime: existing bug that the blocktime does not work for
					  * service at slot 0 because it's status overide by registery table with
					  * default value.
					  */
					BBM->_BBM_rte_scan_marks.reset(m_index);
				}
			}
		}

		if (rte >= res) {
			rte++;
			continue;
		}
		if (rte->nextreg == -1)
			rte = res;
		else
			rte = rte_mgr.link2ptr(rte->nextreg);
	}
}

bbm_chk::bbm_chk()
{
	BBM = bbm_ctx::instance();
}

bbm_chk::~bbm_chk()
{
}

}
}

