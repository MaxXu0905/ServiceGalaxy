#include "sg_internal.h"

namespace ai
{
namespace sg
{

sg_svcdsp& sg_svcdsp::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_svcdsp *>(SGT->_SGT_mgr_array[SVCDSP_MANAGER]);
}

void sg_svcdsp::svcdsp(message_pointer& msg, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), NULL);
#endif

	sg_svc *svc_mgr = strt_work(*msg);

	SGC->_SGC_svrstate = SGIN_SVC; // doing a service
	if (svc_mgr == NULL) {
		sg_msghdr_t& msghdr = *msg;

		// unknown routine .. let user examine it
		SGT->_SGT_error_code = SGENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown operation request {1}") % msghdr.svc_name).str(SGLOCALE));

		if (msghdr.rplywanted()) {
			msg->set_length(0);

			msghdr.flags |= SGNACK;
			msghdr.sender = SGC->_SGC_proc;
			msghdr.error_code = SGT->_SGT_error_code;
			msghdr.rcvr = msghdr.rplyto;

			sg_metahdr_t& metahdr = *msg;
			metahdr.protocol = SGPROTOCOL;

			if (!ipc_mgr.send(msg, SGREPLY | SGSIGRSTRT))
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed sending reply")).str(SGLOCALE));
		}

		SGC->_SGC_svstats_doingwork = true;
	} else {
		SGT->_SGT_fini_called = false;
		SGT->_SGT_forward_called = false;
		// 调用服务函数，调用之后，输入的msg就不能再使用了
		try {
			svc_mgr->svc(msg);
		} catch (exception& ex) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: exception caught while doing operation, {1}") % ex.what()).str(SGLOCALE));

			SGT->_SGT_fini_rval = SGFAIL;
			SGT->_SGT_fini_urcode = 0;
			SGT->_SGT_fini_msg = msg;
			SGT->_SGT_error_code = SGESVCFAIL;
		} catch (...) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: unknown exception caught while doing operation")).str(SGLOCALE));

			SGT->_SGT_fini_rval = SGFAIL;
			SGT->_SGT_fini_urcode = 0;
			SGT->_SGT_fini_msg = msg;
			SGT->_SGT_error_code = SGESVCFAIL;
		}

		// 返回消息给发送者，BBM/DBBM特殊处理
		if (!SGP->_SGP_ambbm && !SGP->_SGP_amdbbm && !SGT->_SGT_forward_called) {
			if (!SGT->_SGT_fini_called) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation '{1}' failed to call fini or forward")
					% SGT->_SGT_msghdr.svc_name).str(SGLOCALE));

				SGT->_SGT_fini_rval = SGFAIL;
				SGT->_SGT_fini_urcode = 0;
				SGT->_SGT_error_code = SGESVCERR;
			}

			// 设置返回消息
			if (SGT->_SGT_msghdr.rplywanted()) {
				SGC->inithdr(*SGT->_SGT_fini_msg);

				sg_msghdr_t& msghdr = *SGT->_SGT_fini_msg;
				msghdr = SGT->_SGT_msghdr;
				if (SGT->_SGT_fini_rval == SGFAIL) {
					SGT->_SGT_fini_msg->set_length(0);
					msghdr.error_code = SGT->_SGT_error_code;
					if (SGT->_SGT_error_code == SGEOS)
						msghdr.native_code = SGT->_SGT_native_code;
					else
						msghdr.native_code = 0;
				} else {
					msghdr.error_code = 0;
					msghdr.native_code = 0;
				}
				msghdr.error_detail = SGT->_SGT_error_detail;
				msghdr.ur_code = SGT->_SGT_fini_urcode;
				msghdr.sender = SGC->_SGC_proc;
				msghdr.rcvr = msghdr.rplyto;

				if (!ipc_mgr.send(SGT->_SGT_fini_msg, SGSIGRSTRT | SGREPLY))
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to send message to sender")).str(SGLOCALE));
			}
		}

		done_work();

		if (SGT->_SGT_fini_cmd & SGEXIT) {
			/*
			 * Exit abnormally without a server shutdown.  If
			 * the server is restartable, then the sgmngr will
			 * use restartsrv to restart it; otherwise, cleanupsrv
			 * will be used to clean up the server.  Note that
			 * a normal shutdown can be used since this will
			 * cause the server not to be restarted.
			 */
			/* Mark this server as being killed so
			 * restartsrv/cleanupsrv won't kill an innocent
			 * process which reuses the pid.
			 */
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_ste_t *ste = ste_mgr.link2ptr(SGP->_SGP_ste.hashlink.rlink);
			ste->global.status |= SVRKILLED;
			exit(1);
		}
	}

	SGC->_SGC_svrstate = 0; // done with service
}

sg_svcdsp::sg_svcdsp()
{
}

sg_svcdsp::~sg_svcdsp()
{
}

sg_svc * sg_svcdsp::strt_work(sg_message& msg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!(SGP->_SGP_amserver || SGC->_SGC_amadm) || SGC->_SGC_svstats_doingwork) {
		SGT->_SGT_error_code = SGENOENT;
		return NULL;
	}

	/*
	 * Can't use SGP->_SGP_ste because it does have the most up-to-date
	 * child and family links for the service search needed later.
	 */
	sg_svc *svc_mgr = get_svc(ste_mgr.link2ptr(SGP->_SGP_ste.hashlink.rlink), msg);
	if (svc_mgr == NULL) {
		sg_msghdr_t& msghdr = msg;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot execute operation, address not known, hostid={1}, pgid={2}, prcid={3}, pid={4}, rqaddr={5}, rpaddr={6}")
			% msghdr.sender.mid % msghdr.sender.grpid % msghdr.sender.svrid
			% msghdr.sender.pid % msghdr.sender.rqaddr % msghdr.sender.rpaddr).str(SGLOCALE));
		return NULL;
	}

	SGC->_SGC_svstats_doingwork = true;
	return svc_mgr;
}

bool sg_svcdsp::done_work()
{
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (!SGC->_SGC_svstats_doingwork) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Work completed, cannot stop twice")).str(SGLOCALE));
		}

		return retval;
	}

	BOOST_SCOPE_EXIT((&SGC)) {
		SGC->_SGC_svstats_doingwork = false;
	} BOOST_SCOPE_EXIT_END

	if (svr_done() != -1) {
		retval = true;
		return retval;
	}

	return retval;
}

/*
 *	routine called by a server to indicate that is has started
 *	a service.
 */
sg_svc * sg_svcdsp::get_svc(sg_ste_t *step, sg_message& msg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_scte_t *sctp = NULL;
	sg_ste_t *step2 = NULL;
	sg_lserver_t *lp = &step->local;
	int indx;
	int svcindx;
	int svrindx;
	int index;
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	sg_msghdr_t& msghdr = msg;
	if (msghdr.svc_name[0] == '\0') {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: NULL operation name, cannot dispatch service request")).str(SGLOCALE));
		return NULL;
	}

	/*
	 * NOTE: This code depends on a ONE step link change to add
	 * a new service table entry, i.e. the information in the new
	 * service table entry is completely filled before it is added
	 * to the list by changing the server family child link.  See
	 * sg_scte::create().  Any server in a MSSQ set can add a service
	 * entry for any server on the queue by the advertise meta-
	 * message.
	 */
	sgid_t msg_svcsgid = msghdr.alt_flds.svcsgid;

	bool sgid_match = (msg_svcsgid != BADSGID
		&& (svcindx = sg_hashlink_t::get_index(msg_svcsgid)) >= 0
		&& svcindx < SGC->MAXSVCS()
		&& (sctp = scte_mgr.link2ptr(svcindx)) != NULL
		&& (svrindx = sctp->family.plink) < SGC->MAXSVRS()
		&& (step2 = ste_mgr.link2ptr(svrindx)) != NULL
		&& SGP->_SGP_ste.hashlink.sgid == step2->hashlink.sgid);

	if (sgid_match && (bbp->bbparms.bbversion == msghdr.alt_flds.bbversion
		|| strcmp(msghdr.svc_name, sctp->parms.svc_name) == 0)) {
		// emptys
	} else {
		for (indx = step->family.clink; indx != -1; indx = sctp->family.fslink) {
			sctp = scte_mgr.link2ptr(indx);
			if (!strcmp(msghdr.svc_name, sctp->parms.svc_name))
				break;
		}
		if (indx == -1) {
			SGT->_SGT_error_code = SGENOENT;
			return NULL;
		}
	}

	step = ste_mgr.link2ptr(sctp->family.plink);

	scoped_bblock bblock(SGT, bi::defer_lock);
	if (bbp->bbparms.options & SGACCSTATS) {
		// If accurate stats are wanted lock BB.
		bblock.lock();
	}

	step->global.status |= BUSY;

	/*
	 * Since no BB lock for service cache, load data of the server will not be accurate
	 * remedy the current load
	 */
	sg_qte_t *curq = qte_mgr.link2ptr(sctp->queue.qlink);
	if (curq->local.wkqueued <= 0 && !SGC->remote(msghdr.sender.mid)) {
		curq->local.wkqueued = sctp->parms.svc_load + SGC->netload(sctp->mid());
		if (curq->local.nqueued <= 0)
			curq->local.nqueued = 1;
	}
	/*
	 * to multi-thread server, only main thread can change
	 * currclient in STE.local, working threads only needs
	 * take care their own server context in their own RTE
	 */
	if (SGC->_SGC_crte->rflags & SERVER_CTXT) {
		// do nothing to lp
	} else {
	 	lp->currclient = msghdr.sender;
		lp->currrtype = msghdr.rplymtype;
		lp->currriter = msghdr.rplyiter;
		lp->curroptions = msghdr.flags;
		lp->currmsgid = msghdr.alt_flds.msgid;
		char *p = lp->currservice;
		char *q = sctp->parms.svc_name;
		int lcv = 0;
		while ((lcv < (MAX_SVC_NAME + 1)) && ((*p++ = *q++) != '\0'))
			lcv++;
		lp->currservice[MAX_SVC_NAME] = '\0';
	}

	SGC->_SGC_asvcindx = sctp->hashlink.rlink;
	SGC->_SGC_asvcsgid = sctp->hashlink.sgid;

	// 初始化服务调用对象
	if (!SGT->_SGT_svcdsp_inited) {
		for (sg_svcdsp_t *svcdsp = SGP->_SGP_svcdsp; svcdsp->instance != NULL; svcdsp++)
			SGT->_SGT_svcdsp.push_back((*svcdsp->instance)());
		SGT->_SGT_svcdsp_inited = true;
	}

	index = sctp->parms.svc_index;
	if (index < 0 || index >= SGT->_SGT_svcdsp.size()) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOFUNC;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: operation index out of range")).str(SGLOCALE));
		}
		return NULL;
	}

	/*
	 * Set service timeout in registry table, add in scanunit so
	 * the timer doesn't go off too early.  Don't worry about locking
	 * to set timeout since sgmngr only manipulates this value if non-0
	 * to begin with.
	 */
	if (sctp->parms.svctimeout > 0)
		SGC->_SGC_crte->rt_svctimeout = sctp->parms.svctimeout + bbp->bbparms.scan_unit;

	/*
	 * set service execution time to MAXINT, so that sgmngr
	 * is aware of the server is running the service and count
	 * the service execution time.
	 */
	if(!step->svrproc().is_bbm() && !step->svrproc().is_dbbm())
		SGC->_SGC_crte->rt_svcexectime = std::numeric_limits<int>::max();

	return SGT->_SGT_svcdsp[index];
}

int sg_svcdsp::svr_done()
{
	sg_scte_t *sctep;
	sg_qte_t *qtp;
	bool remote_client = false;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	/*
	 * First reset the registry table service timeout if we set it to
	 * begin with so the sgmngr's timer doesn't go off and kill us
	 * unnecessarily.  Have to syslock() to protect from interference
	 * from sgmngr.
	 */
	if (SGC->_SGC_crte->rt_svctimeout > 0) {
		scoped_syslock syslock(SGT);
		SGC->_SGC_crte->rt_svctimeout = 0;
	}

	/*
	 * This two fields are used by sgmngr to check service execution time,
	 * if it execeed the pre-defined threshold, post a event
	 * It's not critical data, so don't protect them by lock.
	 */
	SGC->_SGC_crte->rt_svcexectime = 0;

	sctep = scte_mgr.link2ptr(SGC->_SGC_asvcindx);
	/*
	 * Use the cached server table entry to get our table entry.
	 * We can't go from the service entry as it may have been taken
	 * over by another service entry from another server while we
	 * were doing the request via unadvertise and advertise.
	 */
	sg_ste_t *step = ste_mgr.link2ptr(SGP->_SGP_ste.hashlink.rlink);
	qtp = qte_mgr.link2ptr(step->queue.qlink);

	scoped_bblock bblock(SGT, bi::defer_lock);
	if (bbp->bbparms.options & SGACCSTATS) {
		// If accurate stats are wanted lock BB.
		bblock.lock();
	}

	step->local.ncompleted++;
	step->local.wkcompleted += sctep->parms.svc_load;

	if (SGC->remote(step->local.currclient.mid))
		remote_client = true;
	step->local.currclient.clear();
	step->local.currservice[0] = '\0';
	step->global.status &= ~BUSY;

	if (SGT->_SGT_fini_cmd & SGEXIT){
		step->global.status |= EXITING;
		ste_mgr.update(step, 1, U_GLOBAL);
	}

	if (SGC->_SGC_asvcsgid != sctep->hashlink.sgid) {
		/*
		 * Our service entry was pulled out from under us,
		 * act like everything went ok as only service and
		 * queue stats are lost.
		 */
		retval = 1;
		return retval;
	}

	/*
	 * In service cache mode, the load information increase by clients may be lost
	 * due to no lock. Then it will make the load balancing incorrect and some servers
	 * may get high load always and impact throughput. Here it is just to introduce one mechanism to remedy the
	 * lost of the wkqueued bye the server itself. Since this remedy will introduce
	 * some slight cost (msgctl), and it is not required to do the remedy
	 * for every request. Then, the environment variable TM_SVRLOAD_REMEDY_ROUNDS
	 * is just to control how many rounds the server will do one remedy. It should
	 * depends on the processing time of the service, the processing time is longer,
	 * the number should be smaller
	 * And, since in MP mode with LDBAL=Y wkqueued will not be decreased always,
	 * this remedy will not work.
	 */
	if (SGC->_SGC_do_remedy > 0) {
		int indx;
		short lowload = -1;
		sg_scte_t *newsctp = NULL;
		if (++SGC->_SGC_svc_rounds >= SGC->_SGC_do_remedy) {
			SGC->_SGC_svc_rounds = 0;

			// remedy the load based on # of messags in Q and the lowest load of the server
			for (indx = step->family.clink; indx != -1; indx = newsctp->family.fslink){
				newsctp = scte_mgr.link2ptr(indx);
				if (lowload == -1 || newsctp->parms.svc_load < lowload)
					lowload = newsctp->parms.svc_load;
			}

			try {
				queue_pointer queue_mgr = SGT->get_queue(SGC->_SGC_proc.mid, qtp->paddr());
				size_t msgs = queue_mgr->get_num_msg();
				long wkqueued = lowload * msgs + sctep->parms.svc_load + SGC->netload(sctep->mid());
				if (wkqueued > qtp->local.wkqueued) {
					qtp->local.wkqueued = wkqueued;
					qtp->local.nqueued = msgs + 1;
				}
			} catch (exception& ex) {
				// skip this error so as to continue following actions
			}
		}
	}

	/*
	 * Set SGC->_SGC_svcindx because that is used in dequeue().  This
	 * is necessary because both a tmacall() or the end of a
	 * service may call dequeue() and tmacall() must use
	 * SGC->_SGC_svcindx.
	 */
	SGC->_SGC_svcindx = SGC->_SGC_asvcindx;

	/*
	 * don't subtract the load for service requested
	 * by remote client
	 */
	if (!remote_client) {
		if (!qte_mgr.dequeue(sctep, false)) { // updates service stats
			return retval;
		}
	} else { // update statistics
		bbp->bbmeters.wkcompleted += sctep->parms.svc_load;
		sctep->local.ncompleted++;
	}

	SGC->_SGC_sssq = (qtp->global.svrcnt == 1);
	retval = 1;
	return retval;
}

}
}

