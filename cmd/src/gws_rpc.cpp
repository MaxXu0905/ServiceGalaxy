#include "gws_rpc.h"
#include "gws_connection.h"
#include "gws_server.h"

namespace ai
{
namespace sg
{

namespace bp = boost::posix_time;

gws_rpc& gws_rpc::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<gws_rpc *>(SGT->_SGT_mgr_array[GRPC_MANAGER]);
}

bool gws_rpc::process_rpc(gws_connection *conn, message_pointer& msg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	if ((rqst->opcode & VERSMASK) != VERSION) {
		nack_msg(conn, msg, SGEINVAL);
		return false;
	}

	message_pointer rply_msg = rpc_mgr.create_msg(sg_rpc_rply_t::need(MAXRPC));
	{
		// 拷贝消息头
		sg_metahdr_t& rqst_metahdr = *msg;
		sg_metahdr_t& rply_metahdr = *rply_msg;
		rply_metahdr = rqst_metahdr;

		sg_msghdr_t& rqst_msghdr = *msg;
		sg_msghdr_t& rply_msghdr = *rply_msg;
		rply_msghdr = rqst_msghdr;
	}

	int opcode = rqst->opcode & ~VERSMASK;
	switch (opcode) {
	case OB_EXIT:
		retval = do_exit(msg, rply_msg);
		break;
	case OB_FINDDBBM:
		retval = find_dbbm(msg, rply_msg);
		break;
	case OB_RKILL:
		retval = rkill(msg, rply_msg);
		break;
	case OB_SUSPEND:
		retval = suspend();
		break;
	case OB_UNSUSPEND:
		retval = unsuspend();
		break;
	case OB_QCTL:
		retval = qctl(msg, rply_msg);
		break;
	case OB_PCLEAN:
		retval = pclean(msg, rply_msg);
		break;
	case OB_STAT:
	default:
		nack_msg(conn, msg, SGEINVAL);
		return retval;
	}

	if (!retval) {
		nack_msg(conn, msg, SGESYSTEM);
		return retval;
	}

	if (rply_msg->rplywanted()) {
		/* for OB_FINDDBBM only. If the reply message is for
		 * sgmngr, then the rpc_rqust struct can not be used.
		 * it has to be changed to a rpc_rply structure.
		 */
		sg_msghdr_t *msghdr = *rply_msg;
		if (msghdr->sendmtype != GWMTYP && opcode == OB_FINDDBBM) {
			/*
			 * need to realloc message since the structure
			 * TMPROC will be copied to message dataloc.
			 */
			rply_msg->set_length(sg_rpc_rply_t::need(sizeof(sg_proc_t)));
			msghdr = *rply_msg;

			sg_rpc_rply_t *rply = rply_msg->rpc_rply();
			rply->datalen = sizeof(sg_proc_t);
			rply->opcode = OB_FINDDBBM | VERSION;
			rply->rtn = 0;
			rply->error_code = 0;
			rply->num = 0;

			bi::scoped_lock<boost::mutex> lock(GWP->_GWP_serial_mutex);
			*reinterpret_cast<sg_proc_t *>(rply->data()) = GWP->_GWP_dbbm_proc;
		}

		// send response to requester
		msghdr->rcvr = msghdr->rplyto;
		msghdr->sender = SGC->_SGC_proc;

		// reset metaheader
		sg_metahdr_t& metahdr = *rply_msg;
		metahdr.mid = msghdr->rcvr.mid;
		metahdr.qaddr = msghdr->rcvr.rpaddr;
		metahdr.mtype = msghdr->rplymtype;
		msghdr->flags |= SGREPLY;

		if (!SGC->remote(metahdr.mid)) {
			sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
			(void)ipc_mgr.send(rply_msg, SGNOBLOCK | SGSIGRSTRT | SGREPLY);
		} else {
			conn->write(SGT, rply_msg);
		}
	}

	retval = true;
	return retval;
}

bool gws_rpc::do_exit(message_pointer& rqst_msg, message_pointer& rply_msg)
{
	sg_msghdr_t& msghdr = *rqst_msg;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (SGP->_SGP_ambsgws) {
		if (!msghdr.sender.is_bbm() && !msghdr.sender.is_gws()) {
			GPP->write_log((_("INFO: Rejecting EXIT Message not sent from local sgmngr or sggws")).str(SGLOCALE));
			return retval;
		}

		/*
		 * We need to fake out tpsvrdone() for a sghgws being
		 * shutdown by the sgmngr. This only occurs in error situations
		 * and the sghgws needs to remove its queue in this case.
		 */
		if (msghdr.sender.is_bbm())
			SGP->_SGP_amgws = false;
	} else {
		if (!msghdr.sender.is_bbm()) {
			GPP->write_log((_("INFO: Rejecting EXIT Message not sent from local sgmngr")).str(SGLOCALE));
			return retval;
		}
	}

	// 必须先加锁，防止主线程丢失信号
	bi::scoped_lock<boost::mutex> lock(GWP->_GWP_exit_mutex);
	SGP->_SGP_shutdown++;
	GWP->_GWP_io_svc.stop();
	GWP->_GWP_exit_cond.notify_all();

	retval = true;
	return retval;
}

bool gws_rpc::find_dbbm(message_pointer& rqst_msg, message_pointer& rply_msg)
{
	sg_msghdr_t *msghdr = *rply_msg;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (SGP->_SGP_ambsgws) {
		sg_rpc_rqst_t *rqst = rqst_msg->rpc_rqst();
		sg_proc_t *proc = reinterpret_cast<sg_proc_t *>(rqst->data());
		if (proc->pid != 0) {
			bi::scoped_lock<boost::mutex> lock(GWP->_GWP_find_dbbm_mutex);
			GWP->_GWP_dbbm_proc = *proc;
			msghdr->rplymtype = NULLMTYPE;
			GWP->_GWP_find_dbbm_cond.notify_all();

			retval = true;
			return retval;
		}
	}

	// only reset mtype for sggws messages
	if (msghdr->sendmtype == GWMTYP)
		msghdr->rplymtype = GWMTYP;

	// reset message size
	rply_msg->set_length(sg_rpc_rply_t::need(sizeof(sg_proc_t)));

	// special case, reply message carry a request struct
	sg_metahdr_t *metahdr = *rply_msg;
	metahdr->flags |= SGRPTORQ;

	bi::scoped_lock<boost::mutex> lock(GWP->_GWP_serial_mutex);
	if (find_dbbm_internal() != NULL) {
		sg_rpc_rply_t *rply = rply_msg->rpc_rply();
		rply->opcode = OB_FINDDBBM | VERSION;
		*reinterpret_cast<sg_proc_t *>(rply->data()) = GWP->_GWP_dbbm_proc;
		retval = true;
		return retval;
	}

	return retval;
}

bool gws_rpc::qctl(message_pointer& rqst_msg, message_pointer& rply_msg)
{
	sg_rpc_rqst_t *rqst = rqst_msg->rpc_rqst();
	sg_rpc_rply_t *rply = rply_msg->rpc_rply();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	int& qaddr = rqst->arg1.addr;
	int& cmd = rqst->arg2.cmd;
	sgc_ctx *SGC = SGT->SGC();

	try {
		queue_pointer queue_mgr = SGT->get_queue(SGC->_SGC_proc.mid, qaddr);
		switch (cmd) {
		case SGSTATQ:
			*reinterpret_cast<int *>(rply->data()) = queue_mgr->get_num_msg();
			rply->rtn = 0;
			rply->datalen = sizeof(int);
			break;
		case SGREMOVEQ:
		case SGSETQ:
		case SGRESETQ:
		default:
			rply->rtn = -1;
			rply->error_code = SGEPROTO;
			rply->datalen = 0;
			break;
		}
	} catch (exception& ex) {
		rply->rtn = -1;
		rply->error_code = SGESYSTEM;
		rply->datalen = 0;
	}

	rply_msg->set_length(sg_rpc_rply_t::need(rply->datalen));

	retval = true;
	return retval;
}

bool gws_rpc::rkill(message_pointer& rqst_msg, message_pointer& rply_msg)
{
	sg_rpc_rqst_t *rqst = rqst_msg->rpc_rqst();
	sg_rpc_rply_t *rply = rply_msg->rpc_rply();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	int sig = rqst->arg1.sig;
	pid_t pid = reinterpret_cast<sg_proc_t *>(rqst->data())->pid;

	scoped_bblock bblock(SGT, bi::defer_lock);

	// see if "kill -9", and the "with BB lock" option is on
	if (sig == SIGKILL && rqst->arg3.flags & KILL_WITH_BBLOCK)
		bblock.lock();

	rply->rtn = kill(pid, sig);
	if (rply->rtn < 0)
		rply->error_code = errno;
	else
		rply->error_code = 0;
	rply->datalen = 0;
	rply_msg->set_length(sg_rpc_rply_t::need(0));

	retval = true;
	return retval;
}

bool gws_rpc::suspend()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	BOOST_FOREACH(gws_circuit_t& circuit, GWP->_GWP_circuits) {
		circuit.suspend(0);
	}

	retval = true;
	return retval;
}

bool gws_rpc::unsuspend()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	BOOST_FOREACH(gws_circuit_t& circuit, GWP->_GWP_circuits) {
		circuit.unsuspend();
	}

	retval = true;
	return retval;
}

bool gws_rpc::pclean(message_pointer& rqst_msg, message_pointer& rply_msg)
{
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	sg_rpc_rqst_t *rqst = rqst_msg->rpc_rqst();
	int pidx = SGC->midpidx(rqst->arg1.from);
	gws_circuit_t& circuit = GWP->_GWP_circuits[pidx];
	circuit.suspend(0);

	retval = true;
	return retval;
}

message_pointer gws_rpc::create_msg(int opcode, size_t datasize)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	message_pointer msg;
#if defined(DEBUG)
 	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("opcode={1}, datasize={2}") % opcode % datasize).str(SGLOCALE), NULL);
#endif

	if ((msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(datasize))) == NULL)
		return message_pointer();

	sg_metahdr_t& metahdr = *msg;
	sg_msghdr_t& msghdr = *msg;

	// set meta-header flag to METAHDR_FLAGS_GW, this is a sggws message
	metahdr.flags = METAHDR_FLAGS_GW;
	metahdr.mtype = GWMTYP;

	msghdr.sendmtype = GWMTYP;
	msghdr.rplymtype = NULLMTYPE;
	msghdr.sender = SGC->_SGC_proc;
	msghdr.rplyto = SGC->_SGC_proc;
	msghdr.flags = 0;
	strcpy(msghdr.svc_name, GWSVC);

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = opcode | VERSION;
	rqst->datalen = datasize;
	return msg;
}

gws_rpc::gws_rpc()
{
	GWP = gwp_ctx::instance();
}

gws_rpc::~gws_rpc()
{
}

sg_proc_t * gws_rpc::find_dbbm_internal()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int mid;
	int i;
	sg_wkinfo_t wkinfo;
	sg_proc_t *retval = NULL;
#if defined(DEBUG)
	scoped_debug<sg_proc_t *> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (GWP->_GWP_dbbm_set) {
		retval = &GWP->_GWP_dbbm_proc;
		return retval;
	}

	/*
	 * must check for sgclst on network first - if in loopback
	 * mode, the mid will be set wrong if a check is done
	 * for the sgclst locally first.
	 */
	if (SGP->_SGP_ambsgws) {
		message_pointer msg = create_msg(OB_FINDDBBM, sizeof(sg_proc_t));
		if (msg == NULL) {
			if (SGT->_SGT_error_code == 0) {
				SGT->_SGT_error_code = SGESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure")).str(SGLOCALE));
			}

			return retval;
		}

		sg_metahdr_t& metahdr = *msg;
		sg_msghdr_t& msghdr = *msg;

		msghdr.rcvr = SGC->_SGC_proc;
		metahdr.mid = msghdr.rcvr.mid;
		metahdr.qaddr = msghdr.rcvr.rqaddr;

		GWP->_GWP_dbbm_proc.clear();

		sg_rpc_rqst_t *rqst = msg->rpc_rqst();
		*reinterpret_cast<sg_proc_t *>(rqst->data()) = GWP->_GWP_dbbm_proc;

		{
			bi::scoped_lock<boost::mutex> lock(GWP->_GWP_find_dbbm_mutex);
			bp::ptime expire(bp::second_clock::universal_time() + bp::seconds(bbp->bbparms.block_time * bbp->bbparms.scan_unit * 1000));

			if (gws_connection::broadcast(SGT, msg) == 0)
				goto checklocal;

			while (GWP->_GWP_dbbm_proc.pid == 0)
				GWP->_GWP_find_dbbm_cond.timed_wait(GWP->_GWP_find_dbbm_mutex, expire);

			if (GWP->_GWP_dbbm_proc.pid == 0)
				goto checklocal;
		}

		GWP->_GWP_dbbm_set = true;
		retval = &GWP->_GWP_dbbm_proc;
		return retval;

checklocal:
		for (i = 0; i < MAX_MASTERS; i++) {
			if ((mid = SGC->lmid2mid(bbp->bbparms.master[i])) != BADMID && !SGC->remote(mid))
				break;
		}
		if (i == MAX_MASTERS)
			return retval;

		if (proc_mgr.get_wkinfo(SGC->_SGC_wka, wkinfo)) {
			GWP->_GWP_dbbm_proc.mid = SGC->_SGC_proc.mid;
			GWP->_GWP_dbbm_proc.rqaddr = SGC->_SGC_wka;
			GWP->_GWP_dbbm_proc.rpaddr = SGC->_SGC_wka;
			GWP->_GWP_dbbm_proc.pid = wkinfo.pid;
			GWP->_GWP_dbbm_proc.grpid = CLST_PGID;
			GWP->_GWP_dbbm_proc.svrid = MNGR_PRCID;
			GWP->_GWP_dbbm_set = true;
			retval = &GWP->_GWP_dbbm_proc;
			return retval;
		}

		return retval;
	} else {
		sg_ste_t dbbmste;

		dbbmste.svrid() = MNGR_PRCID;
		dbbmste.grpid() = CLST_PGID;
		if (retrieve(dbbmste) != 1)
			return retval;

		GWP->_GWP_dbbm_proc = dbbmste.svrproc();
	}

	retval = &GWP->_GWP_dbbm_proc;
	return retval;
}

/*
 * The following function is used in this file to look for server entries
 * without the BB locked.  We need to do this since the retrievals are being
 * triggered by local processes sending messages to this BRIDGE.  We'll
 * have to do a linear search since we're searching without a lock and hash
 * lists may be changing underneath us.  We'll be extra careful to be sure
 * we don't get a corrupt entry.
 */
int gws_rpc::retrieve(sg_ste_t& ste)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	int retval = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	int gi = ste.grpid();
	int si = ste.svrid();
	sg_ste_t *p = ste_mgr.link2ptr(0);
	for (int i = 0; i < SGC->MAXSVRS(); i++, p++) {
		if (p->mid() == BADMID)
			continue;

		if (gi == p->grpid() && si == p->svrid()) {
			sg_ste_t t = *p;
			// Recheck to make sure still ok
			if (t.grpid() == p->grpid() && t.svrid() == p->svrid()) {
				ste = t;
				retval = 1;
				return retval;
			}
		}
	}

	return 0;
}

void gws_rpc::nack_msg(gws_connection *conn, message_pointer& msg, int error_code)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);

	/*
	 * If no reply was expected to this message, then the best we can do is
	 * log a message with some details about the failure and go on.
	 */
	if (msg->rplywanted()) {
		message_pointer rply_msg = rpc_mgr.create_msg(sg_rpc_rply_t::need(0));
		if (rply_msg == NULL) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure")).str(SGLOCALE));
			return;
		}

		rply_msg->set_length(sg_rpc_rply_t::need(0));

		sg_metahdr_t& metahdr = *rply_msg;
		sg_msghdr_t& msghdr = *rply_msg;
		sg_rpc_rply_t *rply = rply_msg->rpc_rply();
		rply->opcode = -1;
		rply->datalen = 0;

		msghdr.sender = msghdr.rcvr;
		msghdr.rcvr = msghdr.rplyto;
		metahdr.mtype = msghdr.rplymtype;
		strcpy(msghdr.svc_name, "");
		msghdr.flags |= SGNACK;
		msghdr.error_code = error_code ? error_code : SGEOS;
		msghdr.native_code = UMSGSND;
		metahdr.mid = msghdr.rcvr.mid;
		metahdr.qaddr = msghdr.rcvr.rpaddr;

		if (!SGC->remote(metahdr.mid)) {
			sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
			(void)ipc_mgr.send(rply_msg, SGNOBLOCK | SGSIGRSTRT | SGREPLY);
		} else {
			conn->write(SGT, rply_msg);
		}
	}
}

}
}

