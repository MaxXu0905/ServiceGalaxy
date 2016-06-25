#include "sg_internal.h"

namespace ai
{
namespace sg
{

sg_meta& sg_meta::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_meta *>(SGT->_SGT_mgr_array[META_MANAGER]);
}

bool sg_meta::metasnd(message_pointer& msg, sg_proc_t *where, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_switch& switch_mgr = sg_switch::instance();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("where={1}, flags={2}") % where % flags).str(SGLOCALE), &retval);
#endif

	sg_msghdr_t& msghdr = *msg;
	msghdr.flags = flags;
	msghdr.rcvr = *where;
	msghdr.sender = SGC->_SGC_proc;
	msghdr.rplyto = SGC->_SGC_proc;
	strcpy(msghdr.svc_name, "sysmeta");

	if (!ipc_mgr.send(msg, flags)) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Send/receive error on remote procedure call")).str(SGLOCALE));
		}
		return retval;
	}

	if (!(flags & SGAWAITRPLY)) {
		retval = true;
		return retval;
	}

	if (!switch_mgr.getack(SGT, msg, flags, 0))
		return retval;

	retval = true;
	return retval;
}

bool sg_meta::metarcv(message_pointer& msg)
{
	int spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	sg_metahdr_t& metahdr = *msg;
	sg_msghdr_t& msghdr = *msg;

	switch (msghdr.flags & SGMETAMSG){
	case SGADVMETA:
		retval = doadv(msg);
		return retval;
 	case SGSHUTQUEUE:
		retval = doshut(msg);
		return retval;
	case SGSHUTSERVER:
		{
			sg_ste_t rcvr;
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_viable& viable_mgr = sg_viable::instance(SGT);
			sg_proc& proc_mgr = sg_proc::instance(SGT);

			// did the intended server dequeue meta-msg?
			if (SGC->_SGC_proc.pid == msghdr.rcvr.pid) {
				retval = doshut(msg);
				return retval;
			}

			/*
			 * We are NOT the server that's supposed
			 * to shutdown, so if the intended recipient
			 * does exist, re-queue this message at the
			 * recipient's "queue top" (i.e. at its maxmtype
			 * value), and lower our queue top below that
			 * so we don't read the message again.
			 */

			rcvr.svrproc() = msghdr.rcvr;
			if ((ste_mgr.retrieve(S_GRPID, &rcvr, &rcvr, NULL) < 0)
				|| (viable_mgr.inviable(&rcvr, spawn_flags) && SGT->_SGT_error_code != SGESVRSTAT)
				|| !proc_mgr.alive(msghdr.rcvr)) {
				/*
				 * Can't seem to find the intended server,
				 * or it is inviable; drop this message and
				 * tell tmshutdown the problem.
				 */
				retval = RETURN(msg, SGT->_SGT_error_code);
				return retval;
			}

			/*
			 * Lower our "queue top" (if necessary) while we
			 * still have control of the shutdown message.
			 */

			if (SGP->_SGP_ste.global.maxmtype >= rcvr.global.maxmtype) {
				if (ste_mgr.retrieve(S_BYRLINK, &SGP->_SGP_ste, &SGP->_SGP_ste, NULL) < 0) {
					retval = failure(msg, (_("ERROR: Cannot find own BB entry")).str(SGLOCALE));
					return retval;
				}

				/*
				 * Only one shutdown request pending per
				 * queue, so we can lower our queue top here.
				 * New in 4.0, we must turn off the TM_RPLYBASE bit
				 * here always so that the message is not mistaken for
				 * a reply by the "real" recipient.
				 */
				SGP->_SGP_ste.global.maxmtype = rcvr.global.maxmtype - 1;
				SGP->_SGP_ste.global.maxmtype &= ~RPLYBASE;
				if (ste_mgr.update(&SGP->_SGP_ste, 1, U_GLOBAL) < 0) {
					retval = failure(msg, (_("ERROR: [Cannot update own queue]")).str(SGLOCALE));
					return retval;
				}
			}

			/*
			 * Note that a server's "queue top" is only adjusted
			 * (lowered) when processing meta-messages. However,
			 * when the SHUTDOWN bit is set in the server's status
			 * word, those meta-messages which normally adjust the
			 * "queue top" should somehow defer the update, until
			 * either the SHUTDOWN status is cleared, or the server
			 * terminates. This is necessary to avoid races between
			 * putting messages at the "top" of a server's queue,
			 * and the server lowering the top of its queue before
			 * reading the queued message.
			 *
			 * This is not a problem yet, since SHUTDOWN is the
			 * only meta-message implemented, but in the future,
			 * BEWARE.
			 */

			metahdr.mtype = rcvr.global.maxmtype;
			msghdr.sendmtype = rcvr.global.maxmtype;
			if (!ipc_mgr.send(msg, SGSIGRSTRT | SGNOTIME | SGNOTRAN)) {
				retval = failure(msg, (_("ERROR: Cannot re-queue msg")).str(SGLOCALE));
				return retval;
			}

			retval = true;
			return retval;
		}
	default:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Operation receive: unknown meta-msg type: {1}]") % metahdr.flags).str(SGLOCALE));
	}

	retval = RETURN(msg, SGESYSTEM);
	return retval;
}

sg_meta::sg_meta()
{
}

sg_meta::~sg_meta()
{
}

// code to advertise a new service for this server.
bool sg_meta::doadv(message_pointer& msg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	char *svc_name = msg->data() + sizeof(int);
	char *class_name = svc_name + strlen(svc_name) + 1;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	sg_rpc_rply_t *rply = msg->rpc_rply();
	memset(rply, '\0', sizeof(rply) - sizeof(long));

	sg_msghdr_t& msghdr = *msg;
	msghdr.flags &= ~SGMETAMSG;
	msghdr.sender = SGC->_SGC_proc;
	msghdr.rcvr = msghdr.rplyto;
	msghdr.error_code = 0;
	strcpy(msghdr.svc_name, "");
	msg->set_length(sg_rpc_rply_t::need(0));

	sg_scte& scte_mgr = sg_scte::instance(SGT);
	if ((rply->rtn = scte_mgr.offer(svc_name, class_name)) == BADSGID) {
		rply->error_code = SGT->_SGT_error_code;
		msghdr.error_code = SGT->_SGT_error_code;
	}

	if (msg->rplywanted() && !ipc_mgr.send(msg, SGSIGRSTRT | SGNOTIME | SGNOTRAN | SGREPLY))
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Operation receive: cannot send meta-msg reply]")).str(SGLOCALE));

	retval = true;
	return retval;
}

/*
 * Common code for shutting down all servers on a queue
 * or shutting down a particular server on a queue. In
 * the latter case, the caller disambiguates servers.
 */
bool sg_meta::doshut(message_pointer& msg)
{
	bool canshut;
	int nsiblings;
	sg_ste_t ste;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	sg_msghdr_t save_msghdr = *msg;

	/*
	 * First, enforce system constraints: don't abandon
	 * a queue without attempting to drain it. And second,
	 * allow the user the last word as to whether the
	 * shutdown operation should be permitted or not.
	 */
	if ((nsiblings = getqcnt()) < 0) {
		retval = failure(msg, (_("ERROR: [Cannot find own BB entry]")).str(SGLOCALE));
		return retval;
	}

	/*
	 * Special for HA, if the flag SD_IGNOREC is set on the shutdown meta-
	 * message for a sgmngr process, then the sgmngr should ignore all accessors
	 * and shutdown unconditionally. In that case, we don't even call
	 * svrstop here so we don't have to pass a flag down or anything.
	 *
	 * Even if SD_IGNOREC is set, don't shut down sgmngr if SERVERS are
	 * running. The user only wanted to ignore CLIENTS and not SERVERS.
	 */
	int *l = reinterpret_cast<int *>(msg->data());
	if ((*l & SD_IGNOREC) && SGP->_SGP_ambbm && SGC->_SGC_proc.is_bbm()) {
		ste.mid() = SGC->_SGC_proc.mid;
		canshut = false;
		switch (ste_mgr.retrieve(S_MACHINE, &ste, NULL, NULL)) {
		case 1:
			break;
		case 2:
			canshut = true;
			break;
		case 3:
			if (SGC->lmid2mid(bbp->bbparms.master[bbp->bbparms.current]) == SGC->_SGC_proc.mid) {
				canshut = true;
			} else {
				/*
				 * Special case: If the 3rd server is
				 * the temporary sgclst used in a
				 * partitioned shutdown, let it pass.
				 */
				ste.grpid() = CLST_PGID;
				ste.svrid() = MNGR_PRCID;
 				if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) == 1){
					if (ste.mid() == SGC->_SGC_proc.mid)
						canshut = true;
				}
			}
			break;
		}
	} else {
		sg_switch& switch_mgr = sg_switch::instance();
		canshut = switch_mgr.svrstop(SGT);
	}

	sg_msghdr_t& msghdr = *msg;
	msghdr = save_msghdr;

	if (nsiblings > 1 && canshut) {
		/*
		 * More readers and user OKs,
		 * this process can stop now.
		 */
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGESYSTEM;

		retval = RETURN(msg, -1);
		return retval;
	} else if (canshut) { // last server on queue
		if (!SGP->_SGP_shutdown) {
			// put msg back on queue as sentinel
			if(!ipc_mgr.send(msg, SGSIGRSTRT | SGNOTIME | SGNOTRAN)) {
				retval = failure(msg, (_("ERROR: Cannot re-queue msg")).str(SGLOCALE));
				return retval;
			}

			/*
			 * WINDOW open (sgclst only):
			 *
			 *	Make queue read-only: we're closed for today.
			 */
			if (SGP->_SGP_amdbbm) {
				string qname = (boost::format("%1%.%2%") % SGC->midkey(msghdr.rcvr.mid) % msghdr.rcvr.rqaddr).str();
				struct stat buf;
				proc_mgr.stat(qname, buf);
				proc_mgr.chmod(qname, buf.st_mode & ~0222);
			}

			SGP->_SGP_shutdown++;
			retval = true;
			return retval;
		}

		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGESYSTEM;

		retval = RETURN(msg, -1);
		return retval;
	} else {
		// user rejected shutdown request
		if (SGC->_SGC_proc.admin_grp()) {
			if (SGP->_SGP_ambbm)
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to shutdown, clients/processes still attached")).str(SGLOCALE));
			else
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot perform shutdown at this time")).str(SGLOCALE));
		} else {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Application rejects shutdown request")).str(SGLOCALE));
		}

		retval = RETURN(msg, SGESYSTEM);
		return retval;
	}
}

int sg_meta::getqcnt()
{
	sg_qte& qte_mgr = sg_qte::instance(SGT);

	if (qte_mgr.retrieve(S_BYRLINK, &SGP->_SGP_qte, &SGP->_SGP_qte, NULL) < 0)
		return -1;

	return SGP->_SGP_qte.global.svrcnt;
}

// record an error message and return an indication of the error
bool sg_meta::failure(message_pointer& msg, const string& problem)
{
	GPP->write_log((_("meta-message: {1}") % problem).str(SGLOCALE));
	return RETURN(msg, SGT->_SGT_error_code);
}

bool sg_meta::RETURN(message_pointer& msg, int status)
{
	sg_msghdr_t save_msghdr;
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_switch& switch_mgr = sg_switch::instance();
	sg_svr& svr_mgr = sg_svr::instance(SGT);

	sg_msghdr_t *msghdr = *msg;
	msghdr->flags &= ~SGMETAMSG;

	msg->set_length(sg_rpc_rply_t::need(0));

	/*
	 * Remember the ipckey and its associated ids used to
	 * remove ipc resources, since the caller may or may not
	 * be detached from the BB by the time we need them.
	 */

	sg_bbparms_t bbparms = SGC->_SGC_bbp->bbparms;

	if (status == -1) {
		/*
		 * _tmstopserv(_TCARG) calls _tmgetack(), which,
		 * for BBMs, uses the service request
		 * buffer. Since "msg" *is* the service request
		 * buffer, we have to save the message header
		 * before calling _tmstopserv(_TCARG).
		 */

		save_msghdr = *msghdr;

		// clear the reply queue so we can get the ack message
		if (SGC->_SGC_proc.rqaddr != SGC->_SGC_proc.rpaddr) {
			msghdr->rplymtype = NULLMTYPE;	// read in fifo order
			msghdr->rcvr = SGC->_SGC_proc;
			while (ipc_mgr.receive(msg, SGNOBLOCK | SGREPLY | SGSIGRSTRT)) {
				msghdr = *msg;
				msghdr->rplymtype = NULLMTYPE;	// read in fifo order
				msghdr->rcvr = SGC->_SGC_proc;
			}
		}

		/*
		 * At this point, since svrstop() succeeded, it is ok
		 * not to check stopserv(): we're going away.
		 */
		svr_mgr.stop_serv();
		SGP->_SGP_shutdown++;	// turn it on now, its safe even for mssq
		*msghdr = save_msghdr;
	}

	switch_mgr.shutrply(SGT, msg, status, &bbparms);

	msghdr->sender = SGC->_SGC_proc;
	msghdr->rcvr = msghdr->rplyto;
	strcpy(msghdr->svc_name, "");

	if (msg->rplywanted() && !ipc_mgr.send(msg, SGSIGRSTRT | SGNOTIME | SGNOTRAN | SGREPLY))
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation receive: cannot send meta-msg reply")).str(SGLOCALE));

	return (status == 0);
}

}
}

