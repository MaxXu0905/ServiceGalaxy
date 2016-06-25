#include "sg_internal.h"

namespace ai
{
namespace sg
{

const char * sg_rpc_rqst_t::data() const
{
	return reinterpret_cast<const char *>(&placeholder);
}

char * sg_rpc_rqst_t::data()
{
	return reinterpret_cast<char *>(&placeholder);
}

const char * sg_rpc_rply_t::data() const
{
	return reinterpret_cast<const char *>(&placeholder);
}

char * sg_rpc_rply_t::data()
{
	return reinterpret_cast<char *>(&placeholder);
}

sg_rpc& sg_rpc::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_rpc *>(SGT->_SGT_mgr_array[RPC_MANAGER]);
}

message_pointer sg_rpc::create_msg(size_t data_size)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("data_size={1}") % data_size).str(SGLOCALE), NULL);
#endif

	message_pointer msg = sg_message::create(SGT);
	msg->set_length(data_size);
	init_rqst_hdr(msg);
	return msg;
}

void sg_rpc::init_rqst_hdr(message_pointer& msg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_metahdr_t& metahdr = *msg;
	metahdr.mtype = BBRQMTYP;
	metahdr.protocol = SGPROTOCOL;
	metahdr.flags = 0;

	sg_msghdr_t& msghdr = *msg;
	memset(&msghdr, '\0', sizeof(sg_msghdr_t));
	msghdr.sender = SGC->_SGC_proc;
	msghdr.rplyto = SGC->_SGC_proc;
	msghdr.sendmtype = BBRQMTYP;
	msghdr.rplymtype = sg_metahdr_t::bbrplymtype(SGC->_SGC_proc.pid);
	msghdr.rplyiter = ++SGC->_SGC_mkmsg_ritercount;
}

sgid_t sg_rpc::tsnd(message_pointer& msg)
{
	sgid_t sgid = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(200, __PRETTY_FUNCTION__, "", &sgid);
#endif

	// send request to sgclst, msg also points to reply
	if(!send_msg(msg, NULL, SGAWAITRPLY | SGSIGRSTRT)){
		// error_code already set
		return sgid;
	}

	// get tmid out of reply & return
	sg_rpc_rply_t * rply = reinterpret_cast<sg_rpc_rply_t *>(msg->data());
	if (rply->rtn < 0) {
		SGT->_SGT_error_code = rply->error_code;
		return sgid;
	}

	sgid = *reinterpret_cast<sgid_t *>(rply->data());
	return sgid;
}

int sg_rpc::asnd(message_pointer& msg)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	int flags = SGSIGRSTRT;
	if (!SGP->_SGP_amgws)
		flags |= SGAWAITRPLY;

	if (!send_msg(msg, NULL, flags)) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Message send/receive failure for remote procedure call")).str(SGLOCALE));
		}

		return retval;
	}

	if (flags & SGAWAITRPLY) {
		sg_rpc_rply_t *rply = msg->rpc_rply();
		SGT->_SGT_error_code = rply->error_code;
		retval = rply->rtn;
		return retval;
	}

	retval = 1;
	return retval;
}

bool sg_rpc::send_msg(message_pointer& msg, sg_proc_t *where, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_proc_t *proc;
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_switch& switch_mgr = sg_switch::instance();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("where={1}, flags={2}") % reinterpret_cast<long>(where) % flags).str(SGLOCALE), &retval);
#endif

	SGC->get_defaults();

	if (!where) {
		// 查找DBBM，通常情况下，在BB中查找
		if ((SGC->_SGC_sndmsg_wkid = proc_mgr.get_wkid()) < 0) {
			SGT->_SGT_error_code = SGENOCLST;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot find sgclst")).str(SGLOCALE));
			return retval;
		}

		proc = &SGC->_SGC_sndmsg_wkproc;
	} else {
		proc = where;
	}

	sg_metahdr_t& metahdr = *msg;
	sg_msghdr_t& msghdr = *msg;
	msghdr.flags = flags;
	msghdr.rcvr = *proc;

	if (proc->is_dbbm()) {
		if (proc->rqaddr == -1) {
			SGT->_SGT_error_code = SGESYSTEM;
			return retval;
		}

		strcpy(msghdr.svc_name, DBBM_SVC_NAME);
	} else {
		metahdr.mid = proc->mid;
		metahdr.qaddr = proc->rqaddr;
		metahdr.flags |= METAHDR_FLAGS_DBBMFWD;
		strcpy(msghdr.svc_name, BBM_SVC_NAME);
	}

	if (!(flags & SGAWAITRPLY))
		msghdr.rplymtype = NULLMTYPE;

	if (!ipc_mgr.send(msg, flags)) {
		if (!SGT->_SGT_error_code) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Send/receive error on remote procedure call")).str(SGLOCALE));
		}
		return retval;
	}

	if (!(flags &  SGAWAITRPLY) || switch_mgr.getack(SGT, msg, flags & ~SGNOBLOCK, SGP->_SGP_dbbm_resp_timeout)) {
		retval = true;
		return retval;
	}

	if (SGT->_SGT_error_code == SGGOTSIG) {
		if (!where)
			GPP->write_log(__FILE__, __LINE__, (_("WARN:sgclst busy or suspended.")).str(SGLOCALE));
		else
			GPP->write_log(__FILE__, __LINE__, (_("WARN:Cannot get reply message, timeout.")).str(SGLOCALE));
	}

	if (!SGT->_SGT_error_code) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Send/receive error on remote procedure call")).str(SGLOCALE));
	}

	return retval;
}

int sg_rpc::dsgid(int type, sgid_t *sgids, int cnt)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("type={1}, sgids={2}, cnt={3}") % type % reinterpret_cast<long>(sgids) % cnt).str(SGLOCALE), &retval);
#endif

	int rqsz = cnt * sizeof(sgid_t);
	message_pointer msg = create_msg(sg_rpc_rqst_t::need(rqsz));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = type | VERSION;
	rqst->arg3.sgids = cnt;
	rqst->datalen = rqsz;
	memcpy(rqst->data(), sgids, rqsz);

	retval = asnd(msg);
	return retval;
}

int sg_rpc::ustat(sgid_t *sgids, int cnt, int status, int flag)
{
	return (this->*SGT->SGC()->_SGC_bbasw->ustat)(sgids, cnt, status, flag);
}

int sg_rpc::smustat(sgid_t *sgids, int cnt, int status, int flag)
{
	int nupdated = 0;
	sgc_ctx *SGC = SGT->SGC();
	int *statusp;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("sgids={1}, cnt={2}, status={3}, flag={4}") % reinterpret_cast<long>(sgids) % cnt % status % flag).str(SGLOCALE), &retval);
#endif

	if (sgids == NULL) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid entry information given")).str(SGLOCALE));
			return retval;
		}
	}

	scoped_bblock bblock(SGT);

	for (; cnt != 0; cnt--, sgids++) {
		sgid_t sgid = *sgids;
		if (sgid == BADSGID)
			continue;

		int idx = sg_hashlink_t::get_index(sgid);

		switch (sgid & SGID_TMASK) {
		case SGID_SVRT:
			{
				if (idx >= SGC->MAXSVRS())
					continue;
				sg_ste_t *ste = sg_ste::instance(SGT).link2ptr(idx);
				if (ste->hashlink.sgid != sgid)
					continue;
				statusp = &ste->global.status;
			}
			break;
		case SGID_SVCT:
			{
				if (idx >= SGC->MAXSVCS())
					continue;
				sg_scte_t *scte = sg_scte::instance(SGT).link2ptr(idx);
				if (scte->hashlink.sgid != sgid)
					continue;
				statusp = &scte->global.status;
			}
			break;
		case SGID_QUET:
			{
				if (idx >= SGC->MAXQUES())
					continue;
				sg_qte_t *qte = sg_qte::instance(SGT).link2ptr(idx);
				if (qte->hashlink.sgid != sgid)
					continue;
				statusp = &qte->global.status;
			}
			break;
		default:
			continue;
		}

		if (flag & STATUS_OP_OR)
			*statusp |= status;
		else if (flag & STATUS_OP_AND)
			*statusp &= status;
		else
			*statusp = status;

		nupdated++;
	}

	if (nupdated == 0) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	if (ubbver() < 0)
		return retval;

	retval = nupdated;
	return retval;
}

int sg_rpc::mbustat(sgid_t *sgids, int cnt, int status, int flag)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("sgids={1}, cnt={2}, status={3}, flag={4}") % reinterpret_cast<long>(sgids) % cnt % status % flag).str(SGLOCALE), &retval);
#endif

	int rqsz = cnt * sizeof(sgid_t);
	message_pointer msg = create_msg(sg_rpc_rqst_t::need(rqsz));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_USTAT | VERSION;
	rqst->arg1.flag = flag;
	rqst->arg2.status = status;
	rqst->arg3.sgids = cnt;
	rqst->datalen = rqsz;
	memcpy(rqst->data(), sgids, rqsz);

	retval = asnd(msg);
	return retval;
}

int sg_rpc::ubbver()
{
	return (this->*SGT->SGC()->_SGC_bbasw->ubbver)();
}

int sg_rpc::smubbver()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (bbp == NULL)
		return retval;

	long new_ver = time(0);
	while (new_ver == bbp->bbparms.bbversion)
		new_ver++;

	bbp->bbparms.bbversion = new_ver;
	retval = 0;
	return retval;
}

int sg_rpc::nullubbver()
{
	int retval = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	return retval;
}

int sg_rpc::laninc()
{
	return (this->*SGT->SGC()->_SGC_bbasw->laninc)();
}

int sg_rpc::smlaninc()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (bbp == NULL) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOMNGR;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Not attached to a Bulletin Board")).str(SGLOCALE));
		}
		return retval;
	}

	bbp->bbmeters.lanversion++;
	retval = 0;
	return retval;
}

int sg_rpc::mblaninc()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	message_pointer msg = create_msg(sg_rpc_rqst_t::need(0));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_LANINC | VERSION;
	rqst->datalen = 0;

	if (!send_msg(msg, NULL, SGSIGRSTRT)) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Send/receive error for remote procedure call")).str(SGLOCALE));
		}
		return retval;
	}

	retval = 0;
	return retval;
}

bool sg_rpc::set_stat(sgid_t sgid, int status, int flags)
{
	int idx;
	int cnt = 0;
	boost::shared_array<sgid_t> auto_sgids;
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (sgid == BADSGID) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid table entry information given")).str(SGLOCALE));
		return retval;
	}

	if (status != SUSPENDED && status != ~SUSPENDED) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid status given")).str(SGLOCALE));
		return retval;
	}

	if ((idx = sg_hashlink_t::get_index(sgid)) == -1) {
		SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	{
		sg_ste& ste_mgr = sg_ste::instance(SGT);
		sg_qte& qte_mgr = sg_qte::instance(SGT);
		sg_scte& scte_mgr = sg_scte::instance(SGT);
		sg_ste_t ste;
		sg_qte_t qte;
		sg_scte_t scte;

		sharable_bblock lock(SGT);

		if ((sgid & SGID_TMASK) == SGID_SVRT) {
			if (idx >= SGC->MAXSVRS()) {
				SGT->_SGT_error_code = SGEINVAL;
				return retval;
			}

			if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, &sgid) < 0)
				return retval;

			qte.hashlink.rlink = ste.queue.qlink;
			if (qte_mgr.retrieve(S_BYRLINK, &qte, &qte, NULL) < 0)
				return retval;

			// 更改sgid，在后续步骤中更新queue
			if ((sgid = qte.hashlink.sgid) == BADSGID) {
				SGT->_SGT_error_code = SGENOENT;
				return retval;
			}

			if ((idx = sg_hashlink_t::get_index(sgid)) == -1) {
				SGT->_SGT_error_code = SGENOENT;
				return retval;
			}
		}

		if ((sgid & SGID_TMASK) == SGID_QUET) {
			if (idx >= SGC->MAXQUES()) {
				SGT->_SGT_error_code = SGEINVAL;
				return retval;
			}

			auto_sgids.reset(new sgid_t[SGC->MAXSVCS()]);

			scte.parms.svc_name[0] = '\0';
			scte.queue.qlink = idx;
			if ((cnt = scte_mgr.retrieve(S_QUEUE, &scte, NULL, auto_sgids.get())) < 0)
				return retval;
		}

		if ((sgid & SGID_TMASK) == SGID_SVCT) {
			if (idx >= SGC->MAXSVCS()) {
				SGT->_SGT_error_code = SGEINVAL;
				return retval;
			}

			if (scte_mgr.retrieve(S_BYSGIDS, &scte, &scte, &sgid) < 0)
				return retval;

			qte.hashlink.rlink = scte.queue.qlink;
			if (qte_mgr.retrieve(S_BYRLINK, &qte, &qte, NULL) < 0)
				return retval;

			if (qte.global.svrcnt > 1) {
				auto_sgids.reset(new sgid_t[qte.global.svrcnt]);
				if ((cnt = scte_mgr.retrieve(S_QUEUE, &scte, NULL, auto_sgids.get())) < 0)
					return retval;
			} else if (qte.global.svrcnt == 1) {
				cnt = 1;
			} else {
				SGT->_SGT_error_code = SGEINVAL;
				return retval;
			}
		}
	}

	if (auto_sgids == NULL) {
		if (ustat(&sgid, 1, status, flags) < 0)
			return retval;
	} else {
		if (ustat(auto_sgids.get(), cnt, status, flags) < 0)
			return retval;
	}

	retval = true;
	return retval;
}

void sg_rpc::setraddr(const sg_proc_t *up)
{
	sgc_ctx *SGC = SGT->SGC();

	if (up) {
		SGC->_SGC_mbrtbl_curraddr = *up;
		SGC->_SGC_mbrtbl_raddr = &SGC->_SGC_mbrtbl_curraddr;
	} else {
		SGC->_SGC_mbrtbl_raddr = NULL;
	}
}

void sg_rpc::setuaddr(const sg_proc_t *up)
{
	sgc_ctx *SGC = SGT->SGC();

	if (up) {
		SGC->_SGC_sndmsg_wkid = up->rqaddr;
		SGC->_SGC_sndmsg_wkproc = *up;
		SGC->_SGC_sndmsg_wkidset = true;
	} else {
		SGC->_SGC_sndmsg_wkid = -1;
		SGC->_SGC_sndmsg_wkproc.clear();
		SGC->_SGC_sndmsg_wkidset = false;
	}
}

int sg_rpc::cdbbm(int mid, int flags)
{
	sg_ste_t dbbmste;
	sg_ste_t bbmste;
	sg_ste_t bbm;
	sgid_t bbmsgids[MAXBBM + 1];
	int i;
	int bc;
	message_pointer msg;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("mid={1}, flags={2}") % mid % flags).str(SGLOCALE), &retval);
#endif

	// Retrieve the sgclst STE and see if it's still alive
	dbbmste.svrid() = MNGR_PRCID;
	dbbmste.grpid() = CLST_PGID;
	if (ste_mgr.retrieve(S_GRPID, &dbbmste, &dbbmste, NULL) != 1)
		return retval;

	/*
         * If the sgmngr on the current master node is in partitoned state, the
         * sgclst is definitely unreachable.
         */
	bbm.grpid() = SGC->mid2grp(SGC->lmid2mid(bbp->bbparms.master[bbp->bbparms.current]));
	bbm.svrid() = MNGR_PRCID;
	if (ste_mgr.retrieve(S_GRPID, &bbm, &bbm, NULL) == -1)
		return retval;

	if (!(bbm.global.status & PSUSPEND)) {
		int totry = findgws();
		for (i = 0; i < totry; i++) {
			// If it's already alive so it can't be created.
			if (proc_mgr.alive(dbbmste.svrproc()) && SGT->_SGT_error_code != SGETIME) {
				SGT->_SGT_error_code = SGEPROTO;
				return retval;
			}
		}
	}

	/*
	 * Otherwise retrieve all of the local sgmngr entries and send them the
	 * O_CDBBM message. We send to all local BBMs rather than just one
	 * so that the BBMs may retain there current logic on restarting the
	 * sgclst, i.e. only one of them performs the action and they vote to
	 * see which one. dbbmste already has the SRVID set to BBMID so
	 * just use that to retrieve tmids for all servers with that id.
	 */
	if ((bc = ste_mgr.retrieve(S_SVRID, &dbbmste, NULL, bbmsgids)) < 1)
		return retval;

	msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(0));
	if (msg == NULL)
		return retval;

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_CDBBM | VERSION;
	rqst->arg1.flag = flags;
	rqst->arg2.to = mid;

	bool sent = false;
	sg_msghdr_t *msghdr = *msg;
	strcpy(msghdr->svc_name, BBM_SVC_NAME);

	for (i = 0; i < bc; i++) {
		if (ste_mgr.retrieve(S_BYSGIDS, &bbmste, &bbmste, &bbmsgids[i]) != 1)
			continue;

		if (bbmste.grpid() == CLST_PGID || SGC->remote(bbmste.mid())
			|| !proc_mgr.alive(bbmste.svrproc()))
			continue;

		msghdr->rcvr = bbmste.svrproc();
		(void)ipc_mgr.send(msg, SGSIGRSTRT);
		sent = true;
	}

	if (!sent)
		return retval;

	msghdr = *msg;
	msghdr->rcvr = SGC->_SGC_proc;
	if (!ipc_mgr.receive(msg, SGNOTIME | SGSIGRSTRT | SGREPLY))
		return retval;

	retval = 0;
	return retval;
}

int sg_rpc::nwsuspend(int *onpes, int count)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("count={1}") % count).str(SGLOCALE), &retval);
#endif

	retval = tobridges(OB_SUSPEND, onpes, count);
	return retval;
}

// HA specific routine to unsuspend bridges on particular PEs
int sg_rpc::nwunsuspend(int *onpes, int count)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("count={1}") % count).str(SGLOCALE), &retval);
#endif

	retval = tobridges(OB_UNSUSPEND, onpes, count);
	return retval;
}

sg_rpc::sg_rpc()
{
}

sg_rpc::~sg_rpc()
{
}

int sg_rpc::tobridges(int opcode, int *onpes, int count)
{
	sg_ste_t ste;
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("opcode={1}, count={2}") % opcode % count).str(SGLOCALE), &retval);
#endif

	message_pointer msg = create_msg(sg_rpc_rqst_t::need(0));
	if (msg == NULL)
		return retval;

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = opcode | VERSION;
	rqst->datalen = 0;

	// set meta-header flag to TMBRIDGE, this is a BRIDGE message
	sg_metahdr_t& metahdr = *msg;
	sg_msghdr_t& msghdr = *msg;

	metahdr.flags |= METAHDR_FLAGS_GW;
	metahdr.mid = msghdr.rcvr.mid;
	metahdr.qaddr = msghdr.rcvr.rqaddr;

	msghdr.sendmtype = GWMTYP;
	metahdr.mtype = GWMTYP;
	msghdr.rplymtype = NULLMTYPE;

	for (int i = 0; i < count; i++) {
		ste.mid() = onpes[i];
		if (SGC->midpidx(onpes[i]) == SGC->midpidx(ALLMID)) {
			if (tohost(opcode, onpes[i]) < 0)
				return retval;
			continue;
		}

		scoped_bblock bblock(SGT);

		int rcnt = ste_mgr.retrieve(S_MACHINE, &ste, NULL, NULL);
		if (rcnt < 0)
			continue;

		boost::shared_array<sgid_t> auto_sgids(new sgid_t[rcnt]);
		sgid_t *sgids = auto_sgids.get();

		int tcnt = ste_mgr.retrieve(S_MACHINE, &ste, NULL, sgids);
		bblock.unlock();

		if (tcnt > rcnt) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Retrieved too many entries")).str(SGLOCALE));
			return retval;
		}

		for (int j = 0; j < tcnt; j++) {
			if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, &sgids[j]) != 1)
				continue;

			if (!ste.svrproc().is_gws())
				continue;

			if (!proc_mgr.alive(ste.svrproc()))
				continue;

			msghdr.rcvr = ste.svrproc();
			if (!ipc_mgr.send(msg, SGSIGRSTRT))
				return retval;
		}
	}

	retval = 0;
	return retval;
}

int sg_rpc::tohost(int opcode, int mid)
{
	int onpe;
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("opcode={1}, mid={2}") % opcode % mid).str(SGLOCALE), &retval);
#endif

	for (int px = SGC->_SGC_ntes[SGC->midnidx(mid)].link; px != -1; px = SGC->_SGC_ptes[px].link) {
		onpe = SGC->idx2mid(SGC->midnidx(mid), px);
		if (tobridges(opcode, &onpe, 1) < 0)
			return retval;
	}

	retval = 0;
	return retval;
}

int sg_rpc::findgws()
{
	sgid_t gwsgids[MAXBBM];
	sg_ste_t gwste;
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int retval = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	gwste.svrid() = GWS_PRCID;
	int brcount = 0;
	int count = ste_mgr.retrieve(S_SVRID, &gwste, NULL, gwsgids);
	for (int i = 0; i < count; i++) {
		if (ste_mgr.retrieve(S_BYSGIDS, &gwste, &gwste, &gwsgids[i]) != 1)
			continue;

		if (!gwste.svrproc().admin_grp())
			continue;

		if (SGC->remote(gwste.mid()))
			continue;

		if (gwste.global.status & BRSUSPEND)
			continue;

		if (!proc_mgr.alive(gwste.svrproc()))
			continue;

		brcount++;
	}
	if (!brcount)
		retval = 1;
	else
		retval = brcount;
	return retval;
}


}
}

