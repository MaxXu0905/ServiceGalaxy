#include "sg_internal.h"
#include "zlib.h"

namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

sg_ipc& sg_ipc::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_ipc *>(SGT->_SGT_mgr_array[IPC_MANAGER]);
}

bool sg_ipc::send(message_pointer& msg, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_metahdr_t& metahdr = *msg;
	sg_msghdr_t& msghdr = *msg;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("flags={1}, metahdr: flags={2}, mtype={3}, size={4}") % flags % metahdr.flags % metahdr.mtype % metahdr.size).str(SGLOCALE), &retval);
#endif

	if (flags & SGBYPASS) {
		flags &= ~SGBYPASS;
	} else {
		if ((metahdr.flags & METAHDR_FLAGS_DBBMFWD) && !(flags & SGREPLY)) {
			// 当前消息用来查找DBBM，被GWS调整过，需要修正回来
			msghdr.rcvr.rqaddr = metahdr.qaddr;
			msghdr.rcvr.rpaddr = metahdr.qaddr;
			msghdr.rcvr.mid = metahdr.mid;
			metahdr.mtype = msghdr.sendmtype;
		} else {
			metahdr.flags &= ~METAHDR_FLAGS_DBBMFWD;
			if (flags & SGREPLY) {
				metahdr.mtype = msghdr.rplymtype;
				msghdr.rplymtype = NULLMTYPE;
				metahdr.qaddr = msghdr.rcvr.rpaddr;
			} else {
				metahdr.mtype = msghdr.sendmtype;
				metahdr.qaddr = msghdr.rcvr.rqaddr;
			}
			metahdr.mid =  msghdr.rcvr.mid;
		}
	}

	metahdr.protocol = SGPROTOCOL;
	if (!SGP->_SGP_amserver) {
		if (SGC->_SGC_crte == NULL) {
			msghdr.alt_flds.origmid = BADMID;
			msghdr.alt_flds.origslot = -1;
			msghdr.alt_flds.origtmstmp = 0;
		} else {
			msghdr.alt_flds.origmid = SGC->_SGC_proc.mid;
			msghdr.alt_flds.origslot = SGC->_SGC_slot;
			msghdr.alt_flds.origtmstmp = SGC->_SGC_crte->rtimestamp;
		}
	} else {
		// I am a server
		msghdr.alt_flds.origmid = SGT->_SGT_msghdr.alt_flds.origmid;
		msghdr.alt_flds.origslot = SGT->_SGT_msghdr.alt_flds.origslot;
		msghdr.alt_flds.origtmstmp = SGT->_SGT_msghdr.alt_flds.origtmstmp;
	}

	retval = send_internal(msg, flags);
	return retval;
}

bool sg_ipc::receive(message_pointer& msg, int flags, int timeout)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_handle& hdl_mgr = sg_handle::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("flags={1}, timeout={2}") % flags % timeout).str(SGLOCALE), &retval);
#endif
	sg_msghdr_t& msghdr = *msg;
	sg_msghdr_t *sys_msghdr = *sys_rcv_msg;

	if (flags & SGREPLY) {
		if (!(flags & SGGETANY)) {
			if (!hdl_mgr.chkhandle(*msg))
				return retval;
		} else if (SGC->_SGC_crte->hndl.numghandle < 1) {
			// no outstanding replies
			SGT->_SGT_error_code = SGEPROTO;
			return retval;
		}
	}

	if (flags & SGGETANY)
		SGC->_SGC_crte->rflags |= GETANY;

	for (;;) {
		sys_msghdr->rcvr = msghdr.rcvr;
		sys_msghdr->rplymtype = msghdr.rplymtype;
		sys_msghdr->rplyiter = msghdr.rplyiter;

		if (!receive_internal(sys_rcv_msg, flags, timeout))
			return retval;

		sg_metahdr_t *sys_metahdr = *sys_rcv_msg;
		sys_msghdr = *sys_rcv_msg;

		if (!SGP->_SGP_amgws && sys_rcv_msg->is_rply()) { // a REPLY
			int	hidx;

			if (!hdl_mgr.chkhandle(*sys_rcv_msg)) {
				// stale message, throw away
				hdl_mgr.drphandle(*sys_rcv_msg);
				SGT->_SGT_error_code = 0;
				continue;
			}

			SGT->_SGT_getrply_inthandle = hdl_mgr.msg2cd(*sys_rcv_msg);
			hidx = (SGT->_SGT_getrply_inthandle & IDXMASK);
			hdl_mgr.drphandle(*sys_rcv_msg);
			/*
			 * Clear registry table info here after changing
			 * contexts and verifying that this is the message we
			 * were waiting for.  Otherwise keep the info here so
			 * that we will timeout later.
			 */
			if ((SGC->_SGC_crte->rflags & GETANY)
				|| (sys_metahdr->mtype == SGC->_SGC_crte->hndl.bbscan_mtype[hidx]
					&& sys_msghdr->rplyiter == SGC->_SGC_crte->hndl.bbscan_rplyiter[hidx])) {
				SGC->_SGC_crte->hndl.bbscan_mtype[hidx] = 0;
				SGC->_SGC_crte->hndl.bbscan_rplyiter[hidx] = 0;
				SGC->_SGC_crte->hndl.bbscan_timeleft[hidx] = 0;
			}
		}

		SGC->_SGC_crte->rflags &= ~GETANY;

		if (sys_msghdr->error_code) {
			SGT->_SGT_error_code = sys_msghdr->error_code;
			if (SGT->_SGT_error_code == SGEOS)
				SGT->_SGT_native_code = sys_msghdr->native_code;
			if (SGT->_SGT_error_code == SGESVCFAIL)
				SGT->_SGT_ur_code = sys_msghdr->ur_code;

			if (sys_msghdr->error_detail)
				SGT->_SGT_error_detail = sys_msghdr->error_detail;
		} else {
			SGT->_SGT_error_code = 0;
			SGT->_SGT_ur_code = sys_msghdr->ur_code;
		}

		break;
	}

	msg.swap(sys_rcv_msg);
	retval = true;
	return retval;
}

sg_ipc::sg_ipc()
{
	sys_rcv_msg = sg_message::create();
}

sg_ipc::~sg_ipc()
{
}

bool sg_ipc::send_internal(message_pointer& msg, int flags)
{
	sg_metahdr_t *metahdr = *msg;
	sgc_ctx *SGC = SGT->SGC();
	int mid;
	int qaddr;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	// Is this message a remote message ?
	bool remote = SGC->remote(metahdr->mid);
	if (remote) {
		if ((qaddr = findgw()) < 0) {
			SGT->_SGT_error_code = SGENOGWS;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: No sggws available for remote send")).str(SGLOCALE));
			return retval;
		}
		mid = SGC->_SGC_proc.mid;
	} else {
		mid = metahdr->mid;
		qaddr = metahdr->qaddr;
	}

	int mtype = metahdr->mtype;

	{
		// 挂起信号，必须在检查SIGTERM之前挂起，防止在信号挂起过程中
		// 有SIGTERM发生
		scoped_usignal sig;
		if (SGP->_SGP_shutdown_due_to_sigterm && !SGP->_SGP_shutdown_due_to_sigterm_called)
			shutdown_due_to_sigterm();

		retval = os_send(mid, qaddr, remote, msg, mtype, flags);
	}

	if (SGP->_SGP_shutdown_due_to_sigterm && !SGP->_SGP_shutdown_due_to_sigterm_called)
		shutdown_due_to_sigterm();

	return retval;
}

// 超过消息最大允许值，通过共享内存传递消息
bool sg_ipc::shm_transfer(message_pointer& msg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_shm_data_t shm_data;
	shm_data.size = msg->length();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", &retval);
#endif

	while (1) {
		string shm_name = (boost::format("msgq.%1%.%2%.%3%") % SGC->_SGC_proc.pid % SGC->_SGC_ctx_id % SGC->_SGC_rplyq_serial++).str();
		strcpy(shm_data.name, shm_name.c_str());

		try {
			bi::shared_memory_object mapping_mgr(bi::create_only,
				shm_data.name, bi::read_write, SGC->_SGC_perm);
			proc_mgr.chown(shm_name, SGC->_SGC_uid, SGC->_SGC_gid);
			mapping_mgr.truncate(shm_data.size);
			bi::mapped_region region(mapping_mgr, bi::read_write, 0, shm_data.size, NULL);
			// 拷贝数据到共享内存中
			void *addr = reinterpret_cast<char *>(region.get_address());
			::memcpy(addr, msg->data(), shm_data.size);

			// 设置消息头内容
			message_pointer shm_msg = sg_message::create(SGT);
			memcpy(shm_msg->raw_data(), msg->raw_data(), MSGHDR_LENGTH);
			shm_msg->assign(reinterpret_cast<const char *>(&shm_data), sizeof(sg_shm_data_t));

			// 设置消息头，通过共享内存传递消息
			sg_metahdr_t& metahdr = *shm_msg;
			metahdr.flags |= METAHDR_FLAGS_IN_SHM;
			SGT->_SGT_send_shmname = shm_name;

			// 重新设置消息
			msg = shm_msg;

			retval = true;
			return retval;
		} catch (bi::interprocess_exception& ex) {
			switch (ex.get_error_code()) {
			case bi::out_of_memory_error:
				boost::this_thread::yield();
				break;
			case bi::already_exists_error:
				break;
			default:
				SGT->_SGT_error_code = SGEOS;
				SGT->_SGT_native_code = ex.get_native_error();
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create share memory to send large message, error_code = {1}, native_code = {2}") % ex.get_error_code() % ex.get_native_error()).str(SGLOCALE));
				return retval;
			}
		}
	}
}

bool sg_ipc::receive_internal(message_pointer& msg, int flags, int timeout)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_msghdr_t& msghdr = *msg;
	int qaddr;
	int mtype;
	int hidx = -1;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	if (flags & SGREPLY)
		qaddr = msghdr.rcvr.rpaddr;
	else
		qaddr = msghdr.rcvr.rqaddr;

	if (flags & SGGETANY) {
		if (flags & SGREPLY) {
			/*
			 * If we are getting only replies then we only want
			 * to get a reply but we don't care which one.
			 */
			mtype = 1 - SENDBASE;        /* priority */
		} else {
			/*
			 * If we are getting requests, we can drain
			 * the queue fifo.  If we get a reply message, then they are stale
			 * and will be handled accordingly.
			 */
			mtype = 0;			/* fifo */
		}
	} else {
		mtype = msghdr.rplymtype;
	}

	if ((flags & (SGNOTIME | SGNOBLOCK)) == 0) {
		/*
		 * We need to store timeout information, and order is
		 * important.  Use the appropriate handle index.  If
		 * TPGETANY is set, use any handle index currently in use
		 * created by tpacall(), but not one created by tpcall().
		 * (tpgetrply(TPGETANY) cannot coexist with any
		 * other tpgetrply() in the same context.)
		 */
		if (flags & SGGETANY) {
			for (hidx = 0; hidx < MAXHANDLES; hidx++) {
				if ((SGC->_SGC_handles[hidx] & GHANDLE) && (SGC->_SGC_handleflags[hidx] & ACALL_HANDLE))
					break;
			}
		} else if (hidx < BBRQMTYP) {
			hidx = mtype;
			if (hidx >= CALLRPLYBASE) {
				hidx -= CALLRPLYBASE;
			} else if (hidx & RPLYBASE) {
				hidx -= RPLYBASE;
			}
			hidx &= IDXMASK;
		}

		if (hidx >= MAXHANDLES)
			hidx = -1;

		SGC->_SGC_crte->hndl.qaddr = qaddr;
		if (hidx != -1) { // RPC case
			/*
			 * Need to add 1 second so that it doesn't
			 * timeout one scan period too early since
			 * the sgmngr may be about ready to scan as
			 * this is being set.
			 */
			// 3-per blocktime:check the per-call/context/service bloctime first
			if (SGT->_SGT_time_left > 0) {
				SGC->_SGC_crte->hndl.bbscan_timeleft[hidx] = sg_roundup(SGT->_SGT_time_left, bbp->bbparms.scan_unit) + 1;
			} else {
				SGC->_SGC_crte->hndl.bbscan_timeleft[hidx] = bbp->bbparms.block_time * bbp->bbparms.scan_unit + 1;
			}
		}

		if (hidx != -1) {
			if (flags & SGGETANY) {
				// no mtype or reply iteration, make unique one
				SGC->_SGC_crte->hndl.bbscan_rplyiter[hidx] = SGC->_SGC_receive_getanycnt++;
				SGC->_SGC_crte->hndl.bbscan_mtype[hidx] = SGC->_SGC_crte->hndl.gen << RPLYBITS;
				/*
				 * Increment the mtype by 1 to ensure it is not
				 * zero.  (The BB looks for a non-zero mtype
				 * to see whether it should do timeout
				 * processing.)
				 */
				SGC->_SGC_crte->hndl.bbscan_mtype[hidx]++;
			} else {
				SGC->_SGC_crte->hndl.bbscan_rplyiter[hidx] = msghdr.rplyiter;
				SGC->_SGC_crte->hndl.bbscan_mtype[hidx] = msghdr.rplymtype;
			}
		}
	}

	{
		// 挂起信号，必须在检查SIGTERM之前挂起，防止在信号挂起过程中
		// 有SIGTERM发生
		scoped_usignal sig;

		if (SGP->_SGP_shutdown_due_to_sigterm && !SGP->_SGP_shutdown_due_to_sigterm_called)
			shutdown_due_to_sigterm();

		while (1) {
			retval = os_receive(qaddr, msg, mtype, flags, timeout);
			if (retval) {
				sg_metahdr_t& metahdr = *msg;
				if (metahdr.flags & METAHDR_FLAGS_ALARM) {
					if (hidx == -1) {
						// got a stale ALARM, try again
						continue;
					} else {
						if (metahdr.mtype != SGC->_SGC_crte->hndl.bbscan_mtype[hidx]) {
							// got a stale ALARM, try again
							continue;
						}

						sg_msghdr_t& msghdr = *msg;
						if (msghdr.rplyiter != SGC->_SGC_crte->hndl.bbscan_rplyiter[hidx]) {
							// got a stale ALARM, try again
							continue;
						}
					}

					SGT->_SGT_error_code = SGETIME;
					metahdr.flags &= ~METAHDR_FLAGS_ALARM;
					retval = false;
				}

				if (hidx != -1) {
					SGC->_SGC_crte->hndl.bbscan_mtype[hidx] = 0;
					SGC->_SGC_crte->hndl.bbscan_rplyiter[hidx] = 0;
					SGC->_SGC_crte->hndl.bbscan_timeleft[hidx] = 0;
				}
				SGC->_SGC_crte->rflags &= ~GETANY;
			}

			break;
		}
	}

	if (SGP->_SGP_shutdown_due_to_sigterm && !SGP->_SGP_shutdown_due_to_sigterm_called)
		shutdown_due_to_sigterm();

	return retval;
}

bool sg_ipc::os_receive(int qaddr, message_pointer& msg, int mtype, int flags, int timeout)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	size_t recvd_size;
	unsigned int prio;
	int ctx_level;
	int i;
	queue_pointer queue_mgr;
	size_t buffer_size;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(10, __PRETTY_FUNCTION__, (_("qaddr={1}, mtype={2}, flags={3}, timeout={4}, save_msgs={5}") % qaddr % mtype % flags % timeout % SGC->_SGC_rcved_msgs.size()).str(SGLOCALE), &retval);
#endif

	// 首先检查是否有缓存的消息
	if (!SGC->_SGC_rcved_msgs.empty()) {
		if (mtype == 0) { // the first message in the queue
			multimap<int, message_pointer>::reverse_iterator riter = SGC->_SGC_rcved_msgs.rbegin();
			msg = riter->second;
			SGC->_SGC_rcved_msgs.erase(riter->first);
			retval = true;
		} else if (mtype > 0) { // the first  message in the queue of type mtype
			multimap<int, message_pointer>::iterator iter = SGC->_SGC_rcved_msgs.find(mtype);
			if (iter != SGC->_SGC_rcved_msgs.end()) {
				msg = iter->second;
				SGC->_SGC_rcved_msgs.erase(iter);
				retval = true;
			}
		} else { // the first message in the queue with the lowest type less than or equal to the absolute value of mtype
			multimap<int, message_pointer>::iterator iter = SGC->_SGC_rcved_msgs.begin();
			if (iter->first <= -mtype) {
				msg = iter->second;
				SGC->_SGC_rcved_msgs.erase(iter);
				retval = true;
			}
		}

		if (retval) {
#if defined(MSG_DEBUG)
			{
				ostringstream fmt;
				fmt << (_("received cache message(")).str(SGLOCALE) << SGC->_SGC_proc.mid << ", " << qaddr << "): " << *msg;
				GPP->write_log(__FILE__, __LINE__, fmt.str());
			}
#endif

			return retval;
		}
	}

	try {
		queue_mgr = SGT->get_queue(SGC->_SGC_proc.mid, qaddr);
		buffer_size = queue_mgr->get_max_msg_size();
	} catch (bi::interprocess_exception& ex) {
		// 严重错误，队列不存在
		if (ex.get_native_error() != 0) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = ex.get_native_error();
		} else {
			SGT->_SGT_error_code = SGESYSTEM;
		}
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't get message queue for qaddr {1} - {2}") % qaddr % ex.what()).str(SGLOCALE));
		return retval;
	}

	while (1) {
		try {
			msg->reserve(buffer_size);
			char *buffer = msg->raw_data();

			if (GPP->_GPP_do_threads && !(flags & SGNOBLOCK))
				ctx_level = SGC->unlock(SGT);
			else
				ctx_level = 0;

			BOOST_SCOPE_EXIT((&ctx_level)(&SGT)(&SGC)(retval)) {
				if (ctx_level > 0) {
					if (SGC->lock(SGT, ctx_level) < 0) {
						/*
						 * Context has gone away on us.  userlog()
						 * was done in lock().
						 */
						SGT->_SGT_error_code = SGESYSTEM;
						SGT->_SGT_error_detail = SGED_INVALIDCONTEXT;
						retval = false;
					}
				}
			} BOOST_SCOPE_EXIT_END

			if (flags & SGNOBLOCK) {
				if (!queue_mgr->try_receive(buffer, buffer_size, recvd_size, prio)) {
					SGT->_SGT_error_code = SGEBLOCK;
					return retval;
				}
			} else {
				if (flags & SGNOTIME) {
					timeout = numeric_limits<int>::max();
				} else if (timeout == 0) {
					if (SGT->_SGT_time_left > 0)
						timeout = sg_roundup(SGT->_SGT_time_left, bbp->bbparms.scan_unit);
					else
						timeout = bbp->bbparms.block_time * bbp->bbparms.scan_unit;
				}

				bool found = false;
				if (SGC->_SGC_enable_alrm) {
					bp::ptime expire_time;
					time_t last_timestamp = 0;
					time_t end_time = bbp->bbmeters.timestamp + timeout;
					while (bbp->bbmeters.timestamp < end_time) {
						if (last_timestamp == bbp->bbmeters.timestamp) {
							expire_time += bp::seconds(1);
						} else {
							last_timestamp = bbp->bbmeters.timestamp;
							expire_time = bp::from_time_t(last_timestamp + 1);
						}

						if (queue_mgr->timed_receive(buffer, buffer_size, recvd_size, prio, expire_time)) {
							found = true;
							break;
						}

						if (usignal::get_pending() > 0 && !(flags & SGSIGRSTRT)) {
							SGT->_SGT_error_code = SGGOTSIG;
							return retval;
						}

						if (SGP->_SGP_shutdown) {
							SGT->_SGT_error_code = SGESYSTEM;
							return retval;
						}
					}
				} else {
					time_t current = time(0);
					for (i = 0; i < timeout; i++) {
						current++;
						if (queue_mgr->timed_receive(buffer, buffer_size, recvd_size, prio, bp::from_time_t(current))) {
							found = true;
							break;
						}

						if (usignal::get_pending() > 0 && !(flags & SGSIGRSTRT)) {
							SGT->_SGT_error_code = SGGOTSIG;
							return retval;
						}

						if (SGP->_SGP_shutdown) {
							SGT->_SGT_error_code = SGESYSTEM;
							return retval;
						}
					}
				}

				if (!found) {
					SGT->_SGT_error_code = SGETIME;
					return retval;
				}
			}
		} catch (bi::interprocess_exception& ex) {
			if (ex.get_native_error() != 0) {
				SGT->_SGT_error_code = SGEOS;
				SGT->_SGT_native_code = ex.get_native_error();
			} else {
				SGT->_SGT_error_code = SGESYSTEM;
			}
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't receive message from qaddr {1}, error_code = {2}, native_code = {3}") % qaddr % ex.get_error_code() % ex.get_native_error()).str(SGLOCALE));
			return retval;
		}

		sg_metahdr_t *metahdr = *msg;
		if (metahdr->flags & METAHDR_FLAGS_IN_SHM) {
			if (recvd_size != MSGHDR_LENGTH + sizeof(sg_shm_data_t)) {
				SGT->_SGT_error_code = SGEPROTO;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: wrong received size {1} given.") % recvd_size).str(SGLOCALE));
				return retval;
			}

			metahdr->flags &= ~METAHDR_FLAGS_IN_SHM;
			sg_shm_data_t *shm_data = reinterpret_cast<sg_shm_data_t *>(msg->data());

			// Save shm_name first since it will be changed
			string shm_name = shm_data->name;
			bi::remove_shared_memory_on_destroy shm_remove(shm_name.c_str());
			try {
				bi::shared_memory_object mapping_mgr(bi::open_only, shm_data->name, bi::read_only);
				bi::mapped_region region(mapping_mgr, bi::read_only, 0, shm_data->size, NULL);
				char *addr = reinterpret_cast<char *>(region.get_address());
				msg->assign(addr, shm_data->size);
				metahdr = *msg;
			} catch (bi::interprocess_exception& ex) {
				SGT->_SGT_error_code = SGEOS;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't read message from share memory {1}") % shm_data->name).str(SGLOCALE));
				return retval;
			}
		} else if (recvd_size < MSGHDR_LENGTH) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid length {1} of received message") % recvd_size).str(SGLOCALE));
			return retval;
		} else {
			// 设置消息头信息
			msg->decode();
		}

		// 如果是压缩的，则在以下条件成立时解压缩
		// 1. 没有标记SGKEEPCMPRS，表示无条件解压缩
		// 2. 标记SGKEEPCMPRS，但是该消息由本地的GWS处理
		if ((metahdr->flags & METAHDR_FLAGS_CMPRS)
			&& (!(flags & SGKEEPCMPRS) || ((metahdr->flags & METAHDR_FLAGS_GW) && !SGC->remote(metahdr->mid)))) {
			message_pointer uncmprs_msg = sg_message::create();
			Bytef *src = reinterpret_cast<Bytef *>(msg->data());
			uLong src_len = msg->length();

			size_t len = std::max(src_len * 5, static_cast<size_t>(64 * 1024));
			uncmprs_msg->reserve(len);
			while (1) {
				if (uncmprs_msg == NULL) {
					SGT->_SGT_error_code = SGEOS;
					SGT->_SGT_native_code = USBRK;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: out of memory")).str(SGLOCALE));
					return retval;
				}

				Bytef *dst = reinterpret_cast<Bytef *>(uncmprs_msg->data());
				uLongf dst_len = static_cast<uLongf>(len);
				switch (uncompress(dst, &dst_len, src, src_len)) {
				case Z_OK:
					break;
				case Z_BUF_ERROR:	// 输出缓冲区不足
					len *= 2;
					uncmprs_msg->reserve(len);
					continue;
				case Z_MEM_ERROR:
					SGT->_SGT_error_code = SGEOS;
					SGT->_SGT_native_code = USBRK;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: out of memory")).str(SGLOCALE));
					return retval;
				case Z_DATA_ERROR:
					SGT->_SGT_error_code = SGESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: message corrupted")).str(SGLOCALE));
					return retval;
				default:
					SGT->_SGT_error_code = SGESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: unknown return code")).str(SGLOCALE));
					return retval;
				}

				// 设置解压缩记录头
				sg_metahdr_t& uncmprs_metahdr = *uncmprs_msg;
				sg_msghdr_t& uncmprs_msghdr = *uncmprs_msg;
				uncmprs_metahdr = *metahdr;
				uncmprs_msghdr = *msg;

				uncmprs_msg->set_length(dst_len);
				uncmprs_metahdr.flags &= ~METAHDR_FLAGS_CMPRS;

				// 重新设置接收消息
				msg = uncmprs_msg;
				metahdr = &uncmprs_metahdr;
				break;
			}
		}

#if defined(MSG_DEBUG)
		{
			ostringstream fmt;
			fmt << (_("received message(")).str(SGLOCALE) << SGC->_SGC_proc.mid << ", " << qaddr << "): " << *msg;
			GPP->write_log(__FILE__, __LINE__, fmt.str());
		}
#endif

		if (mtype == 0) { // the first message in the queue
			break;
		} else if (mtype > 0) { // the first message in the queue of type mtype
			if (metahdr->mtype == mtype)
				break;
		} else { // the first message in the queue with the lowest type less than or equal to the absolute value of mtype
			if (metahdr->mtype <= -mtype)
				break;
		}

		// 当前消息不满足条件，保存到上下文数组中
		SGC->_SGC_rcved_msgs.insert(std::make_pair(metahdr->mtype, msg));
		msg = sg_message::create(SGT);
	}

	retval = true;
	return retval;
}

bool sg_ipc::os_send(int mid, int qaddr, bool remote, message_pointer msg, int mtype, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	int timeout;
	int ctx_level;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(10, __PRETTY_FUNCTION__, (_("mid={1}, qaddr={2}, remote={3}, mtype={4}, flags={5}") % mid % qaddr % remote % mtype % flags).str(SGLOCALE), &retval);
#endif

#if defined(MSG_DEBUG)
	{
		ostringstream fmt;
		fmt << (_("sent message(")).str(SGLOCALE) << mid << ", " << qaddr << "): " << *msg;
		GPP->write_log(__FILE__, __LINE__, fmt.str());
	}
#endif

	if (flags & SGNOTIME)
		timeout = numeric_limits<int>::max();
	else if (SGT->_SGT_time_left > 0)
		timeout = sg_roundup(SGT->_SGT_time_left, bbp->bbparms.scan_unit);
	else
		timeout = bbp->bbparms.block_time * bbp->bbparms.scan_unit;

	try {
		sg_metahdr_t *metahdr = *msg;
		queue_pointer queue_mgr = SGT->get_queue(mid, qaddr);

		size_t len = msg->length();
		if (len > SGC->_SGC_cmprs_limit && remote) {
			message_pointer cmprs_msg = sg_message::create(SGT);
			// 压缩缓冲区最大字节数
			int cmprs_len = static_cast<size_t>(len * 1.001 + 12);
			cmprs_msg->reserve(cmprs_len);

			Bytef *dst = reinterpret_cast<Bytef *>(cmprs_msg->data());
			uLongf dst_len = static_cast<uLongf>(cmprs_len);
			Bytef *src = reinterpret_cast<Bytef *>(msg->data());
			uLong src_len = static_cast<uLong>(len);
			if (compress(dst, &dst_len, src, src_len) == Z_OK && src_len > dst_len) {
				// 设置压缩记录头
				sg_metahdr_t& cmprs_metahdr = *cmprs_msg;
				sg_msghdr_t& cmprs_msghdr = *cmprs_msg;
				cmprs_metahdr = *metahdr;
				cmprs_msghdr = *msg;

				cmprs_metahdr.flags |= METAHDR_FLAGS_CMPRS;
				cmprs_msg->set_length(dst_len);

				// 重新设置发送消息
				msg = cmprs_msg;
				metahdr = &cmprs_metahdr;
			}
		}

		msg->encode();
		if (metahdr->size > queue_mgr->get_max_msg_size()) {
			if (!shm_transfer(msg)) {
				// error_code alreay set
				return retval;
			}

			// 重新设置消息头
			msg->encode();
			metahdr = *msg;
		}

#if defined(MSG_DEBUG)
		{
			ostringstream fmt;
			fmt << (_("timed sent message(")).str(SGLOCALE) << mid << ", " << qaddr << "): " << *msg;
			GPP->write_log(__FILE__, __LINE__, fmt.str());
		}
#endif

		if (GPP->_GPP_do_threads && ((flags & SGNOTIME) && !(flags & SGNOBLOCK)))
			ctx_level = SGC->unlock(SGT);
		else
			ctx_level = 0;

		BOOST_SCOPE_EXIT((&ctx_level)(&SGT)(&SGC)(&retval)) {
			if (ctx_level > 0) {
				if (SGC->lock(SGT, ctx_level) < 0) {
					/*
					 * Context has gone away on us.  userlog()
					 * was done in lock().
					 */
					SGT->_SGT_error_code = SGESYSTEM;
					SGT->_SGT_error_detail = SGED_INVALIDCONTEXT;
					retval = false;
				}
			}
		} BOOST_SCOPE_EXIT_END

		if (SGC->_SGC_enable_alrm) {
			bp::ptime expire_time;
			time_t last_timestamp = 0;
			time_t end_time = bbp->bbmeters.timestamp + timeout;
			while (bbp->bbmeters.timestamp < end_time) {
				if (last_timestamp == bbp->bbmeters.timestamp) {
					expire_time += bp::seconds(1);
				} else {
					last_timestamp = bbp->bbmeters.timestamp;
					expire_time = bp::from_time_t(last_timestamp + 1);
				}

				if (queue_mgr->timed_send(msg->raw_data(), metahdr->size, mtype, expire_time)) {
					if (SGT->_SGT_time_left > 0) {
						SGT->_SGT_time_left = end_time - bbp->bbmeters.timestamp;
						if (SGT->_SGT_time_left < 0)
							SGT->_SGT_time_left = 0;
					}
					retval = true;
					return retval;
				}

				if (usignal::get_pending() > 0 && !(flags & SGSIGRSTRT)) {
					SGT->_SGT_error_code = SGGOTSIG;
					return retval;
				}

				if (SGP->_SGP_shutdown) {
					SGT->_SGT_error_code = SGESYSTEM;
					return retval;
				}
			}
		} else {
			time_t current = time(0);
			for (int i = 0; i < timeout; i++) {
				current++;
				if (queue_mgr->timed_send(msg->raw_data(), metahdr->size, mtype, bp::from_time_t(current))) {
					if (SGT->_SGT_time_left > 0)
						SGT->_SGT_time_left -= i;
					retval = true;
					return retval;
				}

				if (usignal::get_pending() > 0 && !(flags & SGSIGRSTRT)) {
					SGT->_SGT_error_code = SGGOTSIG;
					return retval;
				}

				if (SGP->_SGP_shutdown) {
					SGT->_SGT_error_code = SGESYSTEM;
					return retval;
				}
			}
		}
	} catch (bi::interprocess_exception& ex) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't send message to qaddr {1}, error_code = {2}, native_code = {3}") % qaddr % ex.get_error_code() % ex.get_native_error()).str(SGLOCALE));
		return retval;
	}

	if (SGT->_SGT_time_left > 0)
		SGT->_SGT_time_left = 0;

	SGT->_SGT_error_code = SGETIME;
	return retval;
}

int sg_ipc::findgw()
{
	sg_ste_t gwste;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(30, __PRETTY_FUNCTION__, "", &retval);
#endif

	bool msgb = (bbp == NULL || (SGC->_SGC_bbasw->bbtype & BBT_MASK) == BBT_MSG);
	bool tried = false;

RESET_LABEL:
	if (SGC->_SGC_findgw_version != bbp->bbmeters.lanversion || msgb) {
		SGC->_SGC_findgw_version = bbp->bbmeters.lanversion;

		int cnt = 0;
		if (!msgb) {
			gwste.svrid() = GWS_PRCID;
			cnt = ste_mgr.retrieve(S_SVRID, &gwste, NULL, NULL);
			/*
			 * If there were none to retrieve, don't give up
			 * yet since there may be some bootstrap bridges
			 * around.
			 */
			if (cnt < 0)
				cnt = 0;
		}

		/*
		 * If there are no regular or bootstrap bridges, exit
		 * immediately
		 */
		if (cnt + addbootstrap(true) <= 0)
			return retval;

		/*
		 * Now add the STE entries for the sggws and the fake
		 * entries for any bootstrap sggws around.
		 */
		SGC->_SGC_findgw_cache.resize(cnt);
		if (!msgb) {
			if (ste_mgr.retrieve(S_SVRID, &gwste, &SGC->_SGC_findgw_cache[0], NULL) < 0)
				SGC->_SGC_findgw_cache.clear();
		}
		addbootstrap(false);

		for (vector<sg_ste_t>::iterator iter = SGC->_SGC_findgw_cache.begin(); iter != SGC->_SGC_findgw_cache.end();) {
			sg_ste_t& ste = *iter;
			sg_proc_t& proc = ste.svrproc();

			if (proc.admin_grp() && !SGC->remote(proc.mid)) {
				if (proc_mgr.alive(proc) && !(ste.global.status & BRSUSPEND)) {
					if ((ste.global.status & RESTARTING) || proc_mgr.getq(ste.mid(), ste.rqaddr())) {
						++iter;
						continue;
					}
				}
			}

			// This entry is for a remote or dead sggws, remove it
			iter = SGC->_SGC_findgw_cache.erase(iter);
		}

		SGC->_SGC_findgw_nextcache = 0;
	}

	// Check all SGC->_SGC_findgw_cache entries for the next "live" one
	if (!SGC->_SGC_findgw_cache.empty()) {
		// Wrap around the end if necessary
		if (++SGC->_SGC_findgw_nextcache >= SGC->_SGC_findgw_cache.size())
			SGC->_SGC_findgw_nextcache = 0;

		// If this one is dead now, then refresh the cache and try again
		if (!proc_mgr.alive(SGC->_SGC_findgw_cache[SGC->_SGC_findgw_nextcache].svrproc())) {
			SGC->_SGC_findgw_version--;	// reset the cache since a sggws died
			tried = true;
			goto RESET_LABEL;
		}

		retval = SGC->_SGC_findgw_cache[SGC->_SGC_findgw_nextcache].rqaddr();
	}

	if (retval < 0 && !tried) {
		SGC->_SGC_findgw_version--;	// reset the cache since a sggws died
		tried = true;
		goto RESET_LABEL;
	}

	return retval;
}

int sg_ipc::addbootstrap(bool count_only)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int count = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(30, __PRETTY_FUNCTION__, (_("count_only={1}") % count_only).str(SGLOCALE), &count);
#endif

	// look through nodepe tables for this node
	int nidx = SGC->midnidx(SGC->_SGC_proc.mid);

	// Loop through nodepe table looking for network entries to check
	for (int pidx = SGC->_SGC_ntes[nidx].link; pidx != -1; pidx = SGC->_SGC_ptes[pidx].link) {
		if (!(SGC->_SGC_ptes[pidx].flags & NP_NETWORK))
			continue;

		int mid = SGC->_SGC_ptes[pidx].mid;
		sg_wkinfo_t wkinfo;

		if (!proc_mgr.get_wkinfo(SGC->midkey(mid), wkinfo))
			continue;

		if (count_only) {
			count++;
			continue;
		}

		/*
		 * Check to see if there is an active bridge entry already
		 * in the cache for this queue id.
		 */
		bool found = false;
		BOOST_FOREACH(sg_ste_t& v, SGC->_SGC_findgw_cache) {
			sg_proc_t& proc = v.svrproc();

			if (!SGC->remote(proc.mid)) {
				if (v.rqaddr() == wkinfo.qaddr) {
					if (proc_mgr.alive(proc)) {
						found = true;
						break;
					}
				}
			}
		}

		/*
		 * if not found, then we know that there wasn't an entry in the
		 * cache already for a live process using this queue.
		 */
		if (!found) {
			sg_ste_t ste;
			ste.pid() = wkinfo.pid;
			ste.grpid() = SGC->mid2grp(mid);
			ste.svrid() = GWS_PRCID;
			ste.mid() = mid;
			ste.rqaddr() = wkinfo.qaddr;
			ste.rpaddr() = wkinfo.qaddr;
			ste.global.status = IDLE;
			if (proc_mgr.alive(ste.svrproc())) {
				SGC->_SGC_findgw_cache.push_back(ste);
				count++;
			}
		}
	}

	return count;
}

void sg_ipc::shutdown_due_to_sigterm()
{
	sg_svr& svr_mgr = sg_svr::instance(SGT);
#if defined(DEBUG)
	scoped_debug<int> debug(20, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!SGP->_SGP_shutdown_due_to_sigterm_called) {
		SGP->_SGP_shutdown_due_to_sigterm_called = true;
		svr_mgr.cleanup();
		exit(1);
	}
}

}
}

