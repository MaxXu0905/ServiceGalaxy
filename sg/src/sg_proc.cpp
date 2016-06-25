#include "sg_internal.h"

namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

sg_proc& sg_proc::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_proc *>(SGT->_SGT_mgr_array[PROC_MANAGER]);
}

bool sg_proc::create_wkid(bool amdbbm)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_bbparms_t& bbparms = config_mgr.get_bbparms();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("amdbbm={1}") % amdbbm).str(SGLOCALE), &retval);
#endif

	try {
		int wka;
		if (amdbbm) {
			wka = SGC->_SGC_wka;
		} else {
			int mid = SGC->getmid();
			wka = SGC->midkey(mid);
		}
		string shm_name = (boost::format("shm.wka.%1%") % wka).str();
		bi::shared_memory_object mapping_mgr(bi::open_or_create, shm_name.c_str(), bi::read_write, bbparms.perm);
		chown(shm_name, SGC->_SGC_uid, SGC->_SGC_gid);

		bi::offset_t total_size = sizeof(sg_wkinfo_t);
		mapping_mgr.truncate(total_size);
		bi::mapped_region region_mgr(mapping_mgr, bi::read_write, 0, total_size, NULL);

		sg_wkinfo_t *wkinfo = reinterpret_cast<sg_wkinfo_t *>(region_mgr.get_address());
		(void)new (wkinfo) sg_wkinfo_t();
		wkinfo->pid = getpid();
		wkinfo->qaddr = SGC->_SGC_proc.rqaddr;
		retval = true;
		return retval;
	} catch (bi::interprocess_exception& ex) {
		return retval;
	}
}

int sg_proc::get_wkid()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (SGC->_SGC_sndmsg_wkidset) {
		retval = SGC->_SGC_sndmsg_wkid;
		return retval;
	}

	// 首先在BB中查找DBBM节点是否存在
	if (bbp != NULL && !(SGC->_SGC_bbasw->bbtype & BBT_MSG)) {
		sg_ste_t dbbm_ste;
		dbbm_ste.svrid() = MNGR_PRCID;
		dbbm_ste.grpid() = CLST_PGID;

		if (ste_mgr.retrieve(S_GRPID, &dbbm_ste, &dbbm_ste, NULL) == 1) {
			SGC->_SGC_sndmsg_wkproc = dbbm_ste.svrproc();
			SGC->_SGC_sndmsg_wkid = dbbm_ste.rqaddr();
			retval = SGC->_SGC_sndmsg_wkid;
			return retval;
		}
	}

	// 如果没有找到，则检查local节点上是否存在
	sg_wkinfo_t wkinfo;
	if (get_wkinfo(SGC->_SGC_wka, wkinfo)) {
		int qaddr = wkinfo.qaddr;
		if (qaddr > 0 && getq(SGC->_SGC_proc.mid, qaddr)) {
			int i;
			int mid;

			for (i = 0; i < MAX_MASTERS; i++) {
				mid = SGC->lmid2mid(bbp->bbparms.master[i]);
				if (mid != BADMID && mid == SGC->_SGC_proc.mid)
					break;
			}

			if (i < MAX_MASTERS || SGC->_SGC_slot == SGC->MAXACCSRS() + AT_SHUTDOWN) {
				SGC->_SGC_sndmsg_wkproc.mid = (i < MAX_MASTERS) ? mid : SGC->_SGC_proc.mid;
				SGC->_SGC_sndmsg_wkproc.rqaddr = qaddr;
				SGC->_SGC_sndmsg_wkproc.rpaddr = qaddr;
				SGC->_SGC_sndmsg_wkproc.pid = wkinfo.pid;
				SGC->_SGC_sndmsg_wkproc.svrid = MNGR_PRCID;
				SGC->_SGC_sndmsg_wkproc.grpid = CLST_PGID;
				if (alive(SGC->_SGC_sndmsg_wkproc)) {
					retval = SGC->_SGC_sndmsg_wkproc.rqaddr;
					return retval;
				}
			}
		}
	}

	// 向GWS发起一个RPC请求，来查找DBBM的位置
	int mid = SGC->getmid();
	if (get_wkinfo(SGC->midkey(mid), wkinfo)) {
		int qaddr = wkinfo.qaddr;
		if (qaddr > 0 && getq(SGC->_SGC_proc.mid, qaddr)) {
			sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
			sg_ipc& ipc_mgr = sg_ipc::instance(SGT);

			message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_proc_t)));
			sg_metahdr_t& metahdr = *msg;
			sg_msghdr_t& msghdr = *msg;

			metahdr.flags |= METAHDR_FLAGS_GW;
			msghdr.rcvr.mid = SGC->_SGC_proc.mid;
			msghdr.rcvr.pid = SGC->_SGC_proc.pid;
			msghdr.rcvr.grpid = SGC->mid2grp(SGC->_SGC_proc.mid);
			msghdr.rcvr.svrid = GWS_PRCID;
			msghdr.rcvr.rqaddr = qaddr;
			msghdr.rcvr.rqaddr = qaddr;

			sg_rpc_rqst_t *rqst = msg->rpc_rqst();
			rqst->opcode = OB_FINDDBBM | VERSION;
			sg_proc_t *up = reinterpret_cast<sg_proc_t *>(rqst->data());
			up->pid = 0;
			rqst->datalen = sizeof(sg_proc_t);

			if (!alive(msghdr.rcvr))
				return retval;

			if (!ipc_mgr.send(msg, SGSIGRSTRT))
				return retval;

			sg_switch& switch_mgr = sg_switch::instance();
			if (!switch_mgr.getack(SGT, msg, SGSIGRSTRT, 0))
				return retval;

			sg_rpc_rply_t *rply = msg->rpc_rply();
			if (rply->rtn < 0)
				return retval;

			SGC->_SGC_sndmsg_wkproc = *reinterpret_cast<sg_proc_t *>(rply->data());
			retval = SGC->_SGC_sndmsg_wkproc.rqaddr;
			return retval;
		}
	}

	return retval;
}

int sg_proc::getq(int mid, int qaddr, size_t max_num_msg, size_t max_msg_size, int perm)
{
	sgc_ctx *SGC = SGT->SGC();
	boost::shared_ptr<bi::message_queue> msgq;
	string qname;
	int addr;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("mid={1}, qaddr={2}, max_num_msg={3}, max_msg_size={4}, perm={5}") % mid % qaddr % max_num_msg % max_msg_size % perm).str(SGLOCALE), &retval);
#endif

	int midkey = SGC->midkey(mid);

	if (qaddr > 0) { // 打开存在的队列
		try {
			addr = qaddr;
			qname = (boost::format("%1%.%2%") % midkey % addr).str();
			msgq.reset(new bi::message_queue(bi::open_only, qname.c_str()));
		} catch (bi::interprocess_exception& ex) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failure to open message queue {1}") % qname).str(SGLOCALE));
			SGT->_SGT_error_code = SGESYSTEM;
			return retval;
		}
	} else {
		try {
			sg_misc_t *misc = SGC->_SGC_misc;
			sg_qitem_t *items = misc->items;
			scoped_bblock bblock(SGT);

			if (misc->que_free == -1) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: No more queue is allowed")).str(SGLOCALE));
				SGT->_SGT_error_code = SGELIMIT;
				return retval;
			}

			sg_qitem_t& item = items[misc->que_free];
			int sequence = (item.sequence + 1) % (1 << QUEUE_VBITS);

			addr = (sequence << QUEUE_SHIFT) | item.rlink;
			if (SGP->_SGP_amdbbm)
				addr |= QUEUE_DBBM;
			else if (SGP->_SGP_ambsgws)
				addr |= QUEUE_GWS;
			else if (SGP->_SGP_amsgboot)
				addr |= QUEUE_BOOT;
			qname = (boost::format("%1%.%2%") % midkey % addr).str();

			if (!(SGP->_SGP_boot_flags & BF_RESTART)) {
				bi::message_queue::remove(qname.c_str());
				msgq.reset(new bi::message_queue(bi::create_only, qname.c_str(),
					max_num_msg, max_msg_size, perm));
			} else {
				msgq.reset(new bi::message_queue(bi::open_or_create, qname.c_str(),
					max_num_msg, max_msg_size, perm));
			}

			// 设置权限
			chown(qname, SGC->_SGC_uid, SGC->_SGC_gid);

			item.flags = 0;
			item.sequence = sequence;
			item.qaddr = addr;
			item.pid = getpid();

			// 从空闲队列中删除
			if (item.rlink == misc->que_tail)
				misc->que_tail = -1;

			misc->que_free = item.next;
			if (misc->que_free != -1)
				items[misc->que_free].prev = -1;

			// 加入到使用队列头
			item.prev = -1;
			item.next = misc->que_used;
			if (misc->que_used != -1)
				items[misc->que_used].prev = item.rlink;
			misc->que_used = item.rlink;
		} catch (bi::interprocess_exception& ex) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failure to create message queue {1}") % qname).str(SGLOCALE));
			SGT->_SGT_error_code = SGESYSTEM;
			return retval;
		}
	}

	retval = addr;
	return retval;
}

void sg_proc::insertq(int qaddr, pid_t pid)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
	sg_misc_t *misc = SGC->_SGC_misc;
	sg_qitem_t *items = misc->items;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("qaddr={1}, pid={2}") % qaddr % pid).str(SGLOCALE), NULL);
#endif

	int rlink = qaddr & QUEUE_MASK;
	if (rlink >= misc->maxques) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error.")).str(SGLOCALE));
		exit(1);
	}

	sg_qitem_t& item = items[rlink];
	if (item.pid != 0) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error.")).str(SGLOCALE));
		exit(1);
	}

	item.flags = 0;
	item.sequence = (qaddr >> QUEUE_SHIFT) & QUEUE_VMASK;
	item.qaddr = qaddr;
	item.pid = pid;

	// 从空闲队列中删除
	if (item.rlink == misc->que_tail)
		misc->que_tail = item.prev;

	if (item.prev == -1) {
		misc->que_free = item.next;
		if (misc->que_free != -1)
			items[misc->que_free].prev = -1;
	} else {
		items[item.prev].next = item.next;
		if (item.next != -1)
			items[item.next].prev = item.prev;
	}

	// 加入到使用队列头
	item.prev = -1;
	item.next = misc->que_used;
	if (misc->que_used != -1)
		items[misc->que_used].prev = rlink;
	misc->que_used = rlink;
}

bool sg_proc::updateq(const sg_ste_t& ste)
{
	scoped_bblock bblock(SGT);
	sgc_ctx *SGC = SGT->SGC();
	sg_misc_t *misc = SGC->_SGC_misc;
	sg_qitem_t *items = misc->items;
	int qaddr;
	int loop;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("rqaddr={1}, rpaddr={2}, pid={3}") % ste.rqaddr() % ste.rpaddr() % ste.pid()).str(SGLOCALE), &retval);
#endif

	if (ste.rqaddr() == ste.rpaddr())
		loop = 1;
	else
		loop = 2;

	for (int i = 0; i < loop; i++) {
		if (i == 0)
			qaddr = ste.rqaddr();
		else
			qaddr = ste.rpaddr();

		int rlink = qaddr & QUEUE_MASK;
		if (rlink >= misc->maxques) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid qaddr {1}.") % qaddr).str(SGLOCALE));
			return retval;
		}

		sg_qitem_t& item = items[rlink];
		if (item.rlink != rlink) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error, rlink={1}") % item.rlink).str(SGLOCALE));
			exit(1);
		}

		if (item.pid == 0)
			return retval;

		item.flags = 0;
		item.pid = ste.pid();
	}

	retval = true;
	return retval;
}

void sg_proc::removeq(int qaddr)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	if (bbp == NULL)
		return;

	sg_misc_t *misc = SGC->_SGC_misc;
	sg_qitem_t *items = misc->items;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("qaddr={1}") % qaddr).str(SGLOCALE), NULL);
#endif
	scoped_bblock bblock(SGT);

	int rlink = qaddr & QUEUE_MASK;
	if (rlink >= misc->maxques) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid qaddr {1}.") % qaddr).str(SGLOCALE));
		return;
	}

	sg_qitem_t& item = items[rlink];
	if (item.rlink != rlink) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error, rlink={1}") % item.rlink).str(SGLOCALE));
		exit(1);
	}

	if (item.pid == 0)
		return;

	string qname = (boost::format("%1%.%2%") % SGC->midkey(SGC->_SGC_proc.mid) % qaddr).str();
	bi::message_queue::remove(qname.c_str());

	item.flags = 0;
	item.pid = 0;

	// 从使用队列中删除
	if (item.prev == -1) {
		misc->que_used = item.next;
		if (misc->que_used != -1)
			items[misc->que_used].prev = -1;
	} else {
		items[item.prev].next = item.next;
		if (item.next != -1)
			items[item.next].prev = item.prev;
	}

	/*
	 * 创建队列时，应当优先使用刚释放的队列的新版本，
	 * 这样其它进程如果打开了该队列，即可检测到有版本
	 * 更新，从而关闭老版本的消息队列句柄。
	 * 但是，当序列刚好达到一个轮回时，就要冷却一段时
	 * 间再用，防止同一队列的版本更新过快，导致其它进
	 * 程还没有关闭老版本，而新队列的名称与老版本的名
	 * 称一致，从而，其它进程区分不了有版本更新。
	 */
	if (item.sequence != QUEUE_VMASK) {
		// 加入到空闲队列头
		item.prev = -1;
		item.next = misc->que_free;
		if (misc->que_free != -1)
			items[misc->que_free].prev = rlink;
		else
			misc->que_tail = rlink;
		misc->que_free = rlink;
	} else {
		// 加入到空闲队列尾
		item.prev = misc->que_tail;
		item.next = -1;
		if (misc->que_tail != -1)
			items[misc->que_tail].next = rlink;
		else
			misc->que_free = rlink;
		misc->que_tail = rlink;
	}
}

bool sg_proc::get_wkinfo(int wka, sg_wkinfo_t& wkinfo)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("wka={1}") % wka).str(SGLOCALE), &retval);
#endif

	try {
		string shm_name = (boost::format("shm.wka.%1%") % wka).str();
		bi::shared_memory_object mapping_mgr(bi::open_only, shm_name.c_str(), bi::read_write);

		bi::offset_t total_size = 0;
		mapping_mgr.get_size(total_size);
		if (total_size != sizeof(sg_wkinfo_t))
			return retval;

		bi::mapped_region region_mgr(mapping_mgr, bi::read_write, 0, total_size, NULL);

		sg_wkinfo_t *wkinfo_p = reinterpret_cast<sg_wkinfo_t *>(region_mgr.get_address());
		if (wkinfo_p->pid <= 0)
			return retval;

		if (kill(wkinfo_p->pid, 0) == -1)
			return retval;

		wkinfo = *wkinfo_p;
		retval = true;
		return retval;
	} catch (bi::interprocess_exception& ex) {
		return retval;
	}
}

bool sg_proc::alive(const sg_proc_t& proc)
{
	int timeout = 10;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	SGT->_SGT_error_code = 0;
	if (proc.pid <= 0)
		return retval;

	if (bbp != NULL)
		timeout = bbp->bbparms.scan_unit;

	if (nwkill(proc, 0, timeout)) {
		retval = true;
		return retval;
	}

	if (SGT->_SGT_error_code == SGEOS) {
		if (errno == ESRCH) {
			return retval;
		} else {
			SGT->_SGT_error_code = 0;
			retval = true;
			return retval;
		}
	}

	if (SGT->_SGT_error_code != SGETIME)
		return retval;

	retval = true;
	return retval;
}

bool sg_proc::nwkill(const sg_proc_t& proc, int signo, int timeout)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("signo={1}, timeout={2}") % signo % timeout).str(SGLOCALE), &retval);
#endif

	if (!SGC->remote(proc.mid)) {
		int ret = kill(proc.pid, signo);
		if (ret < 0) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = UKILL;
			return retval;
		}

		retval = true;
		return retval;
	}

	// GWS不能检查状态
	if (SGP->_SGP_amgws || proc.is_gws())
		return retval;

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_proc_t)));
	sg_metahdr_t *metahdr = *msg;
	sg_msghdr_t *msghdr = *msg;

	//  Set message fields that change per message
	msghdr->alt_flds.msgid = ++SGT->_SGT_nwkill_counter;

	// Set rplymtype so that replies to nwkills are not confused with other replies.
	msghdr->flags |= SG_NWKILLBIT;

	// set meta-header flag to METAHDR_FLAGS_GW, this is a sggws message
	msghdr->sendmtype = GWMTYP;
	metahdr->flags |= METAHDR_FLAGS_GW;
	msghdr->rcvr = proc;
	msghdr->rcvr.grpid = BBC_PGID;
	msghdr->rcvr.svrid = GWS_PRCID;

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = OB_RKILL | VERSION;
	rqst->datalen = sizeof(sg_proc_t);
	rqst->arg1.sig = signo;

	rqst->arg3.flags = 0;
	memcpy(rqst->data(), &proc, sizeof(sg_proc_t));

	// 发送消息到GWS
	if (!ipc_mgr.send(msg, SGSIGRSTRT)) {
		SGT->_SGT_error_code = SGENOGWS;
		return retval;
	}

	msghdr->rcvr = SGC->_SGC_proc;

	if (!ipc_mgr.receive(msg, SGREPLY, timeout)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Remote nwkill timed out {1} secs") % timeout).str(SGLOCALE));
		SGT->_SGT_error_code = SGETIME;
		return retval;
	}

	sg_rpc_rply_t *rply = msg->rpc_rply();
	if (rply->rtn < 0) {
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = UKILL;
		errno = rply->error_code;
		return retval;
	}

	retval = true;
	return retval;
}

bool sg_proc::nwqctl(const sg_qte_t& qte, int cmd, int *arg)
{
	sg_ste_t ste;
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("cmd={1}") % cmd).str(SGLOCALE), &retval);
#endif

	ste.hashlink.rlink = qte.global.clink;
	if (ste_mgr.retrieve(S_BYRLINK, &ste, &ste, NULL) != 1)
		return retval;

	if (!SGC->remote(ste.mid())) {
		try {
			/*
			 * When restarsvr is used for master migration, no previous message
			 * queue exists.  (Also, if the server had failed becuase its queue
			 * was manually removed, no queue would exist.)
			 * Therefore, it is not an error if msgctl() fails.
			 */
			string qname = (boost::format("%1%.%2%") % SGC->midkey(SGC->_SGC_proc.mid) % qte.paddr()).str();
			bi::message_queue msgq(bi::open_only, qname.c_str());
			if (arg)
				*arg = msgq.get_num_msg();
		} catch (exception& ex) {
			SGT->_SGT_error_code = SGESYSTEM;
			return retval;
		}

		retval = true;
		return retval;
	}

	// Must go remote, format a message for a remote sggws
	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(0));
	if (msg == NULL)
		return retval;

	// set meta-header flag to TMBRIDGE, this is a BRIDGE message
	sg_metahdr_t& metahdr = *msg;
	metahdr.flags |= METAHDR_FLAGS_GW;

	sg_msghdr_t *msghdr = *msg;
	msghdr->sendmtype = GWMTYP;

	// Set the receive to any sggws on the machine the qte is on
	msghdr->rcvr = ste.svrproc();
	msghdr->rcvr.grpid = BBC_PGID;
	msghdr->rcvr.svrid = GWS_PRCID;

	// Now stick the arguments in the message
	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = OB_QCTL | VERSION;
	rqst->datalen = 0;
	rqst->arg1.addr = qte.paddr();
	rqst->arg2.cmd = cmd;

	// Now send the message to the bridge local to the process
	if (!ipc_mgr.send(msg, SGSIGRSTRT)) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOGWS;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Send to sggws failed")).str(SGLOCALE));
		}
		return retval;
	}

	msghdr->rcvr = SGC->_SGC_proc;
	if (!ipc_mgr.receive(msg, SGSIGRSTRT | SGREPLY))
		return retval;

	// reset msghdr since msg may be realloced
	msghdr = *msg;

	if (msghdr->flags & SGNACK) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOGWS;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Send to sggws failed")).str(SGLOCALE));
		}
		return retval;
	}

	// Get the return data for the caller
	sg_rpc_rply_t *rply = msg->rpc_rply();
	if (rply->rtn < 0) {
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = msghdr->native_code;
		errno = rply->error_code;
	} else {
		if (arg)
			*arg = *reinterpret_cast<int *>(rply->data());
	}

	retval = (rply->rtn >= 0);
	return retval;
}

int sg_proc::sync_proc(char **argv, const sg_mchent_t& mchent, sg_ste_t *ste, int flags, pid_t& pid, bool (*ignintr)())
{
	int tochild[2];
	int toparent[2];	/* uni-directional pipes */
	int i;
	int ack;
	bool preserve = false;
	int bootfd = 0;
	pid_t oldpid;
	bool dosync = false;
	int max_fd_allowed;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	gpenv& env_mgr = gpenv::instance();
	string path_str;
	string ld_str;
	string str;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("flags={1}, pid={2}") % flags % pid).str(SGLOCALE), &retval);
#endif

	if (flags & SP_SYNC) {	/* Synchronize ? */
		if (pipe(tochild) < 0 || pipe(toparent) < 0)
			retval = BSYNC_PIPERR;
		else
			dosync = true;
	}
	if (argv[1][0] == '-' && argv[1][1] == 'f') {
		// reset the actual file descriptor used
		preserve = true;
		bootfd = toparent[1];
		sprintf(argv[2], "%d", bootfd);
	}

	switch (pid = fork()) {
	case -1:	/* Error forking */
		retval = BSYNC_FORKERR;
		return retval;
	case 0:	/* In child process */
		if (dosync) {
			/* Child will read from the [0] end of the
			 * tochild pipe, and may write to the [1] end of
			 * the toparent pipe, so close unused ends.
			 */
			close(tochild[1]);
			close(toparent[0]);

			if (ste != NULL) {
				ack = 0; /* initialize the ack character. */
				if ((read(tochild[0], &ack, 1) != 1) || (!strncmp(reinterpret_cast<char *>(&ack), PARENT_FAIL, 1))) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Restart sync error {1}") % errno).str(SGLOCALE));
					close(tochild[0]);
					goto syncerr;
				}
			}
			close(tochild[0]);

			if (!preserve) {
				// Set up stdin as pipe back to parent
				close(0);
				dup(toparent[1]);
				close(toparent[1]);
			}
		}

		usignal::signal(SIGINT, SIG_IGN);
		usignal::signal(SIGQUIT, SIG_IGN);
		usignal::signal(SIGCHLD, SIG_IGN);

		/*
		 * clean up left over open files. Leave bootfd open for process
		 * synchronization.
		 */
		if (!dosync && !preserve)
			close(0);

		if (mchent.envfile[0] != '\0') {
			ifstream ifs(mchent.envfile);
			if (!ifs) {
				SGT->_SGT_error_code = SGEOS;
				SGT->_SGT_native_code = UOPEN;
				GPP->write_log(__FILE__, __LINE__, (_("WARN: Unable to open NODE PROFILE {1} for reading") % mchent.envfile).str(SGLOCALE));
			}

			while (ifs.good()) {
				std::getline(ifs, str);
				if (str.empty())
					continue;

				if (strncmp(str.c_str(), "PATH=", 5) == 0)
					path_str = str.substr(5, str.length() - 5);
				else if (strncmp(str.c_str(), _TMLDPATH, sizeof(_TMLDPATH) - 1) == 0)
					ld_str = str.substr(sizeof(_TMLDPATH), str.length() - sizeof(_TMLDPATH));
			}
		}

		/*
		 * Set SGPROFILE and PATH environment variables for child.
		 */
		env_mgr.putenv((boost::format("SGPROFILE=%1%") % mchent.sgconfig).str().c_str());
		env_mgr.putenv((boost::format("APPROOT=%1%") % mchent.appdir).str().c_str());
		env_mgr.putenv((boost::format("SGROOT=%1%") % mchent.sgdir).str().c_str());

		str = (boost::format("PATH=%1%/bin:%2%/bin:/bin:/usr/bin") % mchent.appdir % mchent.sgdir).str();
		if (!path_str.empty()) {
			str += ":";
			str += path_str;
		}
		env_mgr.putenv(str.c_str());

		str = (boost::format("%1%=%2%/lib:%3%/lib:/lib:/usr/lib") % _TMLDPATH % mchent.appdir % mchent.sgdir).str();
		if (!ld_str.empty()) {
			str += ":";
			str += ld_str;
		}
		env_mgr.putenv(str.c_str());

		/*
		 * The hard coded limit on number of file descriptors (20) that a process
		 * may have is no longer valid on many platforms.  Therefore new
		 * system limit defined in limits.h or unistd.h will be used when
		 * possible.
		 */

#if defined(_SC_OPEN_MAX)
		max_fd_allowed = sysconf(_SC_OPEN_MAX);
#elif defined(OPEN_MAX)
		max_fd_allowed = OPEN_MAX;
#else
		max_fd_allowed = 20;
#endif

		for (i = 3; i < max_fd_allowed; i++) {
			if (i != bootfd)
				close(i);
		}

		if (argv[0][0] == '/')
			execv(argv[0], argv);
		else
			execvp(argv[0], argv);
		// shouldn't return

syncerr:
		if (dosync) {
			SGT->_SGT_error_code = BSYNC_EXECERR;
			write(bootfd, &SGT->_SGT_error_code, sizeof(int));
		}
		exit(1);
	default:	/* parent */
		break;
	}

	/*
	 * Only a parent that successfully forks a child makes it here.
	 */
 	if (dosync) {
		/* Parent will read from the [0] end of the
		 * toparent pipe, and write to the [1] end of
		 * the tochild pipe, so close unused ends.
		 */
		close(toparent[1]);
		close(tochild[0]);

		/*
		 * For restarting servers we need to update the pid before
		 * continuing. We also need to write to the servers input
		 * pipe to tell it to go ahead and exec.
		 */
		if (ste != NULL) {
			oldpid = ste->pid();
			ste->pid() = pid;
			if (ste_mgr.update(ste, 1, ste->svrproc().is_dbbm() ? U_LOCAL : U_GLOBAL) < 0) {
				ste->pid() = oldpid;
				write(tochild[1], PARENT_FAIL, 1);
				close(tochild[1]);
				retval = BSYNC_UNK;
				return retval;
			}

			// 更改misc
			if (!updateq(*ste)) {
				ste->pid() = oldpid;
				write(tochild[1], PARENT_FAIL, 1);
				close(tochild[1]);
				retval = BSYNC_UNK;
				return retval;
			}

			write(tochild[1], "1", 1);
		}
		close(tochild[1]);

retry:
		if ((i = read(toparent[0], &retval, sizeof(retval))) != sizeof(retval)) {
			if (i < 0 && errno == EINTR) {
				usignal::ensure_sigs();	// force gotintr to be set
				usignal::defer();		// reset signals
				if (ignintr != NULL && (*ignintr)())
					goto retry;
			}
			close(toparent[0]);
			retval = BSYNC_PIPERR;
			return retval;
		}
		close(toparent[0]);
	} else {
		/*
		 * If we're not syncing and we forked ok, then assume success.
		 */
		retval = BSYNC_OK;
		return retval;
	}

	/*
	 * Now translate the tperrno into a BSYNC error code for tmboot.
	 */
	switch (retval) {
	case SGEAPPINIT:
		pid = -1;
		retval = BSYNC_APPINIT;
		break;
	case SGEDUPENT:
		pid = -1;
		retval = BSYNC_DUPENT;
		break;
	case SGENOCLST:
		pid = -1;
		retval = BSYNC_NODBBM;
		break;
	case SGENOMNGR:
		pid = -1;
		retval = BSYNC_NOBBM;
		break;
	case SGENEEDPCLEAN:
		pid = -1;
		retval = BSYNC_NEEDPCLEAN;
		break;
	case 0:
		retval = BSYNC_OK;
		break;
	}

	/*
	 * In some odd cases, the global update of our pid above may have
	 * "succeeded", but not updated the local BB (specifically on
	 * BRIDGE restart where the sgclst times out talking to this site
	 * or gets a failure to get replies due to a partition.
	 * In those cases, make sure everything in restartsrv works ok
	 * by forcing a local update of the servers pid.
	 */
	if (retval != BSYNC_OK && ste != NULL) {
		if (ste_mgr.update(ste, 1, U_LOCAL) < 0)
			pid = ste->pid();
	}

	return retval;
}

/*
 * Start a remote server via a tagent interaction.
 */
int sg_proc::rsync_proc(char **argv, const sg_mchent_t& mchent, int flags, pid_t& pid)
{
	char *dap;
	char **ap;
	size_t need;
	sg_rpc_rply_t *rp;
	tasyncr_t *syncr;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("flags={1}, pid={2}") % flags % pid).str(SGLOCALE), &retval);
#endif

	pid = -1;	/* Initialize it just in case */

	/* Figure out how much data area we need */
	need = sizeof(tasync_t);
	/* add in the space for each argument string and its NULL. */
	for (ap = argv; *ap ;ap++) {
		need += strlen(*ap) + 1;
	}
	need += 1;	/* Plus one for the terminating NULL */

	/*
	 * Now see if we need to reallocate the data area.
	 */
	boost::shared_array<char> data_buffer(new char[need]);
	if (data_buffer == NULL) {
		retval = BSYNC_NSNDERR;
		return retval;
	}

	tasync_t *ta_sync = reinterpret_cast<tasync_t *>(data_buffer.get());
	ta_sync->flags = flags;
	ta_sync->mchent = mchent;
	dap = ta_sync->argvs;	/* pointer to data area argvs */
	for (ap = argv; *ap; ap++) {
		strcpy(dap, *ap);
		dap += strlen(dap) + 1;
	}
	*dap = '\0';

	if((rp = sg_agent::instance(SGT).send(mchent.mid, OT_SYNCPROC, ta_sync, need)) == NULL) {
		retval = BSYNC_UNK;
		return retval;
	}

	switch (rp->num) {
	case TGESND:
		retval = BSYNC_NSNDERR;
		return retval;
	case TGERCV:
		retval = BSYNC_NRCVERR;
		return retval;
    case 0:
		syncr = reinterpret_cast<tasyncr_t *>(rp->data());
		pid = syncr->pid;
		retval = syncr->spret;
		return retval;
	}

	retval = BSYNC_UNK;
	return retval;
}

bool sg_proc::stat(const string& name, struct stat& buf)
{
	string full_path = _SHMPATH;
	full_path += name;
	return (::stat(full_path.c_str(), &buf) == 0);
}

bool sg_proc::chown(const string& name, uid_t uid, gid_t gid)
{
	string full_path = _SHMPATH;
	full_path += name;
	return (::chown(full_path.c_str(), uid, gid) == 0);
}

bool sg_proc::chmod(const string& name, mode_t mode)
{
	string full_path = _SHMPATH;
	full_path += name;
	return (::chmod(full_path.c_str(), mode) == 0);
}

sg_proc::sg_proc()
{
}

sg_proc::~sg_proc()
{
}

}
}

