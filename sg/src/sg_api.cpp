#include "sg_internal.h"
#include "metarep_struct.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

sg_api& sg_api::instance()
{
	sgt_ctx *SGT = sgt_ctx::instance();
	return *reinterpret_cast<sg_api *>(SGT->_SGT_mgr_array[API_MANAGER]);
}

sg_api& sg_api::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_api *>(SGT->_SGT_mgr_array[API_MANAGER]);
}

bool sg_api::init(const sg_init_t *init_info)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("init_info={1}") % reinterpret_cast<long>(init_info)).str(SGLOCALE), &retval);
#endif

	bi::scoped_lock<boost::recursive_mutex> lock(SGP->_SGP_ctx_mutex);
	int ctx_state = SGP->_SGP_ctx_state;

	if (init_info != NULL && (init_info->flags & SGMULTICONTEXTS)) {
		if (SGP->_SGP_ctx_state == CTX_STATE_UNINIT) {
			SGP->_SGP_ctx_state = CTX_STATE_MULTI;
			GPP->_GPP_do_threads = true;
		} else if (SGP->_SGP_ctx_state == CTX_STATE_SINGLE) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot join application in multi-context mode")).str(SGLOCALE));
			SGT->_SGT_error_code = SGEPROTO;
			return retval;
		}

		int context = SGP->create_ctx();
		if (!set_ctx(context)) {
			// error_code alreay set
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to set context to newly created context")).str(SGLOCALE));
			return retval;
		}
	} else {
		if (SGP->_SGP_ctx_state == CTX_STATE_UNINIT) {
			SGP->_SGP_ctx_state = CTX_STATE_SINGLE;
			GPP->_GPP_do_threads = false;
		} else if (SGP->_SGP_ctx_state == CTX_STATE_MULTI) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot join application in single-context mode")).str(SGLOCALE));
			SGT->_SGT_error_code = SGEPROTO;
			return retval;
		}
	}

	SGT->_SGT_context_invalid = false;

	sgc_ctx *SGC = SGT->SGC();
	if (!SGC->enter(SGT, 0))
		return retval;

	BOOST_SCOPE_EXIT((&SGT)(&SGC)) {
		SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	bool sysadm = false;
	if (!SGC->_SGC_amclient && init_info) {
		if (strcmp(init_info->cltname, "sgadmin") == 0) {
			sg_bbparms_t& bbparms = sg_config::instance(SGT).get_bbparms();
			if (geteuid() == bbparms.uid) {
				SGC->_SGC_slot = bbparms.maxaccsrs + AT_ADMIN;
				sysadm = true;
			}
		}
	}

	retval = sginit(PT_CLIENT, 0, init_info);
	if (!retval && SGT->_SGT_error_code == SGENOENT && sysadm) {
		SGC->_SGC_slot = -1;
		retval = sginit(PT_CLIENT, 0, init_info);
	}

	if (!retval) {
		SGP->_SGP_ctx_state = ctx_state;
		set_ctx(SGNULLCONTEXT);
		return retval;
	}

	return retval;
}

bool sg_api::fini()
{
	int context;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (!get_ctx(context))
		return retval;

	if (SGP->_SGP_amserver) {
		SGT->_SGT_error_code = SGEPROTO;
		return retval;
	}

	sgc_ctx *SGC = SGT->SGC();
	if (SGC->_SGC_sginit_savepid != getpid()) {
		// forked pid, don't term
		SGC->_SGC_sginit_savepid = 0;
		retval = true;
		return retval;
	}

	retval = sgfini(PT_CLIENT);
	if (retval) {
		SGC->_SGC_sginit_savepid = 0;

		bi::scoped_lock<boost::recursive_mutex> lock(SGP->_SGP_ctx_mutex);
		if (SGP->_SGP_ctx_state == CTX_STATE_MULTI) {
			int ctxs = SGP->destroy_ctx(context);
			if (ctxs >= 0) {
				if (ctxs == 0) // 允许Context状态切换
					SGP->_SGP_ctx_state = CTX_STATE_UNINIT;
				set_ctx(SGNULLCONTEXT);
				// 重置SGC
				SGC = SGT->SGC();
			}
		} else if (SGP->_SGP_ctx_state == CTX_STATE_SINGLE) {
			SGP->_SGP_ctx_state = CTX_STATE_UNINIT;
		}
	}

	SGT->_SGT_blktime = 0;
	return retval;
}

bool sg_api::set_ctx(int context)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("context={1}") % context).str(SGLOCALE), &retval);
#endif

	if (context < 0 && context != SGNULLCONTEXT) {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	if ((context == SGSINGLECONTEXT && SGP->_SGP_ctx_state != CTX_STATE_SINGLE)
		|| (context != SGSINGLECONTEXT && SGP->_SGP_ctx_state == CTX_STATE_SINGLE)) {
		SGT->_SGT_error_code = SGEPROTO;
		return retval;
	}

	if (!GPP->_GPP_do_threads && GPP->_GPP_thrid != SGT->_SGT_thrid) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: SGNOTHREADS set to yes but multiple threads detected")).str(SGLOCALE));
		SGT->_SGT_error_code = SGEPROTO;
		return retval;
	}

	if (!SGP->valid_ctx(context)) {
		SGT->_SGT_error_code = SGENOENT;
		return retval;
	}
	SGT->_SGT_context_invalid = false;

	if (context < 0)
		context = 0;

	SGT->_SGT_curctx = context;
	SGT->_SGT_SGC = SGP->_SGP_ctx_array[context];
	retval = true;
	return retval;
}

bool sg_api::get_ctx(int& context)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (SGT->_SGT_curctx == 0 && SGT->_SGT_context_invalid)
		context = SGINVALIDCONTEXT;
	else if (SGT->_SGT_curctx == 0 && SGP->_SGP_ctx_state != CTX_STATE_SINGLE)
		context = SGNULLCONTEXT;
	else
		context = SGT->_SGT_curctx;

	retval = true;
	return retval;
}

bool sg_api::set_blktime(int blktime, int flags)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("blktime={1}, flags={2}") % blktime % flags).str(SGLOCALE), &retval);
#endif

	if (blktime < 0) {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	if (flags == SGBLK_NEXT) {
		SGT->_SGT_blktime = blktime;
	} else {
		sgc_ctx *SGC = SGT->SGC();
		SGC->_SGC_blktime = blktime;
		if (SGC->_SGC_crte != NULL)
			SGC->_SGC_crte->rt_blktime = blktime;
	}

	retval = true;
	return retval;
}

int sg_api::get_blktime(int flags)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	if (flags == SGBLK_NEXT) {
		retval = SGT->_SGT_blktime;
		return retval;
	} else if (flags == SGBLK_ALL) {
		sgc_ctx *SGC = SGT->SGC();
		retval = SGC->_SGC_blktime;
		return retval;
	} else if (flags == 0) {
		sgc_ctx *SGC = SGT->SGC();
		sg_bboard_t *& bbp = SGC->_SGC_bbp;

		int blktime = SGT->_SGT_blktime > 0 ? SGT->_SGT_blktime : SGC->_SGC_blktime;
		if (blktime == 0) {
			if (SGC->_SGC_crte != NULL)
				blktime = bbp->bbparms.block_time * bbp->bbparms.scan_unit;
		}
		retval = blktime;
		return retval;
	} else {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}
}

bool sg_api::call(const char *service, message_pointer& send_msg, message_pointer& recv_msg, int flags)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("operation={1}, flags={2}") % service % flags).str(SGLOCALE), &retval);
#endif

	if (SGP->_SGP_amserver && !(SGP->_SGP_boot_flags & BF_REPLYQ)) {
		SGT->_SGT_error_code = SGEPROTO;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Process should set REPLYQ to invoke other operation.")).str(SGLOCALE));
		return retval;
	}

	retval = call_internal(service, send_msg, recv_msg, flags);
	clncallblktime();
	return retval;
}

bool sg_api::call_internal(const char *service, message_pointer& send_msg, message_pointer& recv_msg, int flags)
{
	int handle;
	int hidx;
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("operation={1}, flags={2}") % service % flags).str(SGLOCALE), &retval);
#endif

	if (!SGC->enter(SGT, 0))
		return retval;

	bool do_leave = true;
	BOOST_SCOPE_EXIT((&SGT)(&SGC)(&do_leave)) {
		if (do_leave)
			SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	if (SGC->_SGC_crte == NULL) {
		SGC->leave(SGT, 0);
		do_leave = false;
		// no bulletin board
		if (!init(NULL)) {
			initerr();
			return retval;
		}

		if (!SGC->enter(SGT, 0))
			return retval;
		do_leave = true;
	}

	if (!SGT->_SGT_intcall && (flags & ~(SGNOTRAN | SGNOBLOCK | SGNOTIME | SGSIGRSTRT))) {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	// Is client suspended outside of a transaction ?  If so, error now
	if (SGC->_SGC_crte->rflags & R_SUSP) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Suspended client attempting to generate new request")).str(SGLOCALE));
		return retval;
	}

	if (!SGT->_SGT_intcall && service != NULL && service[0] == '.' && service[1] == '.') {
		SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	SGT->_SGT_intcall++;
	BOOST_SCOPE_EXIT((&SGT)) {
		SGT->_SGT_intcall--;
	} BOOST_SCOPE_EXIT_END

	if ((handle = acall_internal(service, send_msg, flags | SGCALL)) < 0) {
		if (handle < 0 && SGT->_SGT_curctx == 0 && SGT->_SGT_context_invalid)
			do_leave = false;

		return retval;
	}
	hidx = (handle & IDXMASK);

	// turn off SGNOBLOCK for the receive
	flags &= ~SGNOBLOCK;

	bool rtn = getrply_internal(handle, recv_msg, flags | SGCALL);
	if (!rtn && SGT->_SGT_curctx == 0 && SGT->_SGT_context_invalid) {
		do_leave = false;
		return retval;
	}

	if (!rtn) {
		if (SGC->_SGC_handles[hidx] & GHANDLE) { // need to check for all case
			--SGC->_SGC_crte->hndl.numghandle;
			if (SGC->_SGC_crte->hndl.numghandle == 0)
				SGC->_SGC_crte->rstatus &= ~P_BUSY;
			++SGC->_SGC_crte->hndl.numshandle;
			// increment iteration and stale count for handle
			if ((SGC->_SGC_handles[hidx]&ITMASK) == ITMASK)
				SGC->_SGC_handles[hidx] &= ~ITMASK; // reset to 0
				++SGC->_SGC_handles[hidx];
				SGC->_SGC_handles[hidx] += (ITMASK + 1);
				SGC->_SGC_handles[hidx] &= ~(GHANDLE | THANDLE);
		}

		return retval;
	}

	retval = true;
	return retval;
}

int sg_api::acall(const char *service, message_pointer& send_msg, int flags)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("operation={1}, flags={2}") % service % flags).str(SGLOCALE), &retval);
#endif

	if (SGP->_SGP_amserver && !(SGP->_SGP_boot_flags & BF_REPLYQ)) {
		SGT->_SGT_error_code = SGEPROTO;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Process should set REPLYQ to invoke other operation.")).str(SGLOCALE));
		return retval;
	}

	retval = acall_internal(service, send_msg, flags);
	clncallblktime();
	return retval;
}

int sg_api::acall_internal(const char *service, message_pointer& send_msg, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	int handle = -1;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("operation={1}, flags={2}") % service % flags).str(SGLOCALE), &retval);
#endif

	if (!SGC->enter(SGT, 0))
		return retval;

	bool do_leave = true;
	BOOST_SCOPE_EXIT((&SGT)(&SGC)(&do_leave)) {
		if (do_leave)
			SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	if (SGC->_SGC_crte == NULL) { // no bulletin board
		SGC->leave(SGT, 0);
		do_leave = false;
		if (!init(NULL)) {
			initerr();
			return retval;
		}

		if (!SGC->enter(SGT, 0))
			return retval;
		do_leave = true;
	}

	// validate inputs
	if (!SGT->_SGT_intcall && (flags & ~(SGNOTRAN | SGNOREPLY | SGNOBLOCK | SGNOTIME | SGSIGRSTRT))) {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	// Is client suspended outside of a transaction ?  If so, error now
	if (SGC->_SGC_crte->rflags & R_SUSP) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Suspended client attempting to generate new request")).str(SGLOCALE));
		return retval;
	}

	if (send_msg == NULL) {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	SGC->inithdr(*send_msg);

	sg_metahdr_t& metahdr = *send_msg;
	sg_msghdr_t& msghdr = *send_msg;
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_handle& hdl_mgr = sg_handle::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_scte_t scte;

	if ((service == NULL) || (service[0] == '\0')) {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	} else {
		if (!SGT->_SGT_intcall && SGP->int_name(service)) {
			SGT->_SGT_error_code = SGENOENT;
			return retval;
		}

		strncpy(msghdr.svc_name, service, MAX_SVC_NAME);
		msghdr.svc_name[MAX_SVC_NAME] = '\0';
	}

	msghdr.flags = flags;

	if (!SGP->_SGP_amserver)
		hdl_mgr.rcvstales();

	/*
	 * Server with MSSQ and REPLYQ=N should not do tpcall or tpacall
	 * with a reply specified.  Allow tpforward to go through.
	 */
	if (SGP->_SGP_amserver && !(flags & SGFORWARD)) {
		sg_qte_t *qte = qte_mgr.link2ptr(SGP->_SGP_qte.hashlink.rlink);
		if (SGC->_SGC_proc.rqaddr == SGC->_SGC_proc.rpaddr
			&& !(flags & SGNOREPLY) && qte->global.svrcnt > 1) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot receive reply because MSSQ is specified but REPLYQ is not set")).str(SGLOCALE));
			SGT->_SGT_error_code = SGESYSTEM;
			return retval;
		}
	}

	if (!scte_mgr.getsvc(send_msg, scte, flags)) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	// set up header
	msghdr.rcvr = scte.svrproc();

	// Put the service table entry sgid in the header
	msghdr.alt_flds.svcsgid = scte.hashlink.sgid;
	msghdr.alt_flds.bbversion = SGC->_SGC_bbp->bbparms.bbversion;

	bool free_handle = true;
	BOOST_SCOPE_EXIT((&hdl_mgr)(&metahdr)(&msghdr)(&send_msg)(&handle)(&flags)(&free_handle)) {
		if (free_handle && handle > 0 && !(flags & SGFORWARD)) {
			metahdr.mtype = msghdr.rplymtype;
			hdl_mgr.drphandle(*send_msg);
		}
	} BOOST_SCOPE_EXIT_END

	if (flags & SGFORWARD) {
		msghdr.sender = SGT->_SGT_msghdr.sender;
		msghdr.rplyto = SGT->_SGT_msghdr.rplyto;
		// calculate send priority only
		handle = hdl_mgr.mkprios(*send_msg, &scte, flags | SGNOREPLY);
		if (handle != 0)
			return retval;

		// restore old handle information
		msghdr.rplymtype = SGT->_SGT_msghdr.rplymtype;
		msghdr.rplyiter = SGT->_SGT_msghdr.rplyiter;
	} else {
		msghdr.sender = SGC->_SGC_proc;
		msghdr.rplyto = SGC->_SGC_proc;
		// set message priorities
		handle = hdl_mgr.mkprios(*send_msg, &scte, flags);
		if (handle < 0)
			return retval;
	}

	settimeleft();
	if (SGT->_SGT_time_left == 0) {
		/* No Per-Context and Per-Call blocktime set, we need
		 * check the Per-Service blocktime,for WSH we need always check it.
		 */
		if (scte.parms.svc_name[0] != '.') // skip the "." service
			SGT->_SGT_time_left = scte.parms.svcblktime;
	}
	if ( !(flags & SGCALL) && handle != 0) {
		/* It is in acall(), we need to remember the service idx which
		 * will be used in getrply().
		 */
		SGC->_SGC_handlesvcindex[handle & IDXMASK] = scte.hashlink.rlink;
	}

	if (!qte_mgr.enqueue(&scte))
		return retval;

	bool enqueued = true;
	BOOST_SCOPE_EXIT((&qte_mgr)(&scte)(&enqueued)) {
		if (enqueued)
			qte_mgr.dequeue(&scte, true);
	} BOOST_SCOPE_EXIT_END

	if(!(flags& SGFORWARD))
		SGC->_SGC_crte->lastgrp = msghdr.rcvr.grpid;

	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	if (!ipc_mgr.send(send_msg, flags))
		return retval;

	if (msghdr.sendmtype != BBRQMTYP)
		SGT->_SGT_lastprio = msghdr.sendmtype;

	free_handle = false;
	enqueued = false;
	retval = handle;
	return retval;
}

// get reply from previous asynchronous service request.
bool sg_api::getrply(int& handle, message_pointer& recv_msg, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_scte_t *sctp;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("handle={1}, flags={2}") % handle % flags).str(SGLOCALE), &retval);
#endif

	settimeleft();
	if (SGT->_SGT_time_left && !(flags & SGGETANY) && SGC->_SGC_crte != NULL) {
		if ((sctp = scte_mgr.link2ptr(SGC->_SGC_handlesvcindex[handle & IDXMASK])) != NULL) {
			if (sctp->hashlink.sgid != BADSGID ) {
				if (sctp->parms.svc_name[0] != '.') // skip the "." service
					SGT->_SGT_time_left = sctp->parms.svcblktime;
			}
		}
	}

	retval = getrply_internal(handle, recv_msg, flags);
	clncallblktime();
	return retval;
}

bool sg_api::getrply_internal(int& handle, message_pointer& recv_msg, int flags)
{
	int setgetrply_count = 0;
	sgc_ctx *SGC = SGT->SGC();
	sg_handle& hdl_mgr = sg_handle::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("handle={1}, flags={2}") % handle % flags).str(SGLOCALE), &retval);
#endif

	if (!SGC->enter(SGT, 0))
		return retval;

	/*
	 * Special case, tpgetrply() processing below depends on tperrno being
	 * 0 as we enter here.  Specifically, the /trans stuff uses tperrno
	 * to communicate error conditions and not return codes.  So we need
	 * to reset tperrno here to 0 to handle the cases where getrply()
	 * is called not as the top ATMI level, e.g., call() --> getrply().
	 */
	SGT->_SGT_error_code = 0;

	bool do_leave = true;
	BOOST_SCOPE_EXIT((&SGT)(&SGC)(&do_leave)) {
		if (do_leave)
			SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	if (SGC->_SGC_crte == NULL) { // no bulletin board
		SGC->leave(SGT, 0);
		do_leave = false;
		if (!init(NULL)) {
			initerr();
			return retval;
		}

		if (!SGC->enter(SGT, 0))
			return retval;
		do_leave = true;
	}

	if (!SGT->_SGT_intcall && (flags & ~(SGGETANY | SGNOBLOCK | SGNOTIME | SGSIGRSTRT))) {
		SGT->_SGT_error_code = SGEINVAL;
		if (flags & SGGETANY)
			handle = 0;
		return retval;
	}

	if (SGC->_SGC_amclient)
		hdl_mgr.rcvstales();

	if (!(flags & SGCALL)) {
		if (flags & SGGETANY) {
			if (SGC->_SGC_getrply_count > 0) {
				SGT->_SGT_error_code = SGEPROTO;
				handle = 0;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: An application association may not issue getrply(SGGETANY) concurrently with getrply for a specific cd")).str(SGLOCALE));
				return retval;
			} else if (SGC->_SGC_getrply_count < 0) {
				SGT->_SGT_error_code = SGEPROTO;
				handle = 0;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: An application association may not issue multiple concurrent getrply(SGGETANY) calls")).str(SGLOCALE));
				return retval;
			}
			SGC->_SGC_getrply_count = -1;
			setgetrply_count = -1;
		} else { // Get specific handle
			if (SGC->_SGC_getrply_count < 0) {
				SGT->_SGT_error_code = SGEPROTO;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: An application association may not issue getrply for a specific cd concurrently with getrply(SGETANY)")).str(SGLOCALE));
				return retval;
			}
			SGC->_SGC_getrply_count++;
			setgetrply_count = 1;
		}
	}

	sg_msghdr_t& msghdr = *recv_msg;

	msghdr.rcvr = SGC->_SGC_proc;
	if (!(flags & SGGETANY)) {
		// generate mtype and reply iteration to receive on
		if (flags & SGCALL)
			msghdr.rplymtype = CALLRPLYBASE;
		else
			msghdr.rplymtype = RPLYBASE;
		msghdr.rplymtype += (SGC->_SGC_crte->hndl.gen << RPLYBITS) + (handle & IDXMASK);
		msghdr.rplyiter = (handle >> RPLYBITS);
	}
	if (flags & SGGETANY) // assume failure before receive
		SGT->_SGT_getrply_inthandle = 0;
	else
		SGT->_SGT_getrply_inthandle = handle; // assume error on cd given
	flags |= SGREPLY;

	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	if (!ipc_mgr.receive(recv_msg, flags)) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGESYSTEM;
		handle = SGT->_SGT_getrply_inthandle;	// restore handle

		SGC->_SGC_getrply_count -= setgetrply_count;
		return retval;
	}

	handle = SGT->_SGT_getrply_inthandle;
	SGC->_SGC_getrply_count -= setgetrply_count;
	if (SGT->_SGT_error_code != 0)
		return retval;

	if (SGT->_SGT_getrply_inthandle <= 0) {
		SGT->_SGT_error_code = SGESYSTEM;
		return retval;
	}

	retval = true;
	return retval;
}

bool sg_api::advertise(const string& svc_name, const string& class_name)
{
	sgid_t sgid;
	sg_scte_t scte;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("svc_name={1}, class_name={2}") % svc_name % class_name).str(SGLOCALE), &retval);
#endif

	if (!SGC->enter(SGT, 0))
		return retval;

	BOOST_SCOPE_EXIT((&SGT)(&SGC)) {
		SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	if (svc_name.empty() || class_name.empty()) {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	if (bbp == NULL || SGC->_SGC_amclient) {
		SGT->_SGT_error_code = SGEPROTO;
		return retval;
	}

	int index = -1;
	sg_svcdsp_t *svcdsp;
	for (svcdsp = SGP->_SGP_svcdsp; svcdsp->index != -1; ++svcdsp) {
		if (class_name == svcdsp->class_name) {
			index = svcdsp->index;
			break;
		}
	}

	if (index == -1) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not advertise {1}. Specify all classes at sgcompile time.") % svc_name).str(SGLOCALE));
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	/* check if the service is already advertised and
	 * not suspended, and using the same function
	 */
	strncpy(scte.parms.svc_name, svc_name.c_str(), sizeof(scte.parms.svc_name) - 1);
	scte.parms.svc_name[sizeof(scte.parms.svc_name) - 1] = '\0';
	scte.grpid() = SGC->_SGC_proc.grpid;
	scte.svrid() = SGC->_SGC_proc.svrid;

	{
		scoped_bblock bblock(SGT);

		if (scte_mgr.retrieve(S_GRPID, &scte, &scte, &sgid) > 0) {
			bool moved = (scte.mid() != SGC->_SGC_proc.mid
				|| scte.pid() != SGC->_SGC_proc.pid
				|| scte.rqaddr() != SGC->_SGC_proc.rqaddr);

			if (scte.parms.svc_index != index || moved) {
				/*
				 * allow svcraddr to be re-set if internal call -
				 * needed for sgclst hot-upgrade migration
				 */
				if (!moved && SGT->_SGT_intcall == 0) {
					SGT->_SGT_error_code = SGEMATCH;
					return retval;
				}
				scte.parms.svc_index = index;
				scte.svrproc() = SGC->_SGC_proc;
				strncpy(scte.parms.svc_cname, class_name.c_str(), sizeof(scte.parms.svc_cname) - 1);
				scte.parms.svc_cname[sizeof(scte.parms.svc_cname) - 1] = '\0';
				scte.global.status &= ~SUSPENDED;

				bblock.unlock();
				scte_mgr.update(&scte, 1, U_GLOBAL);
			} else if (scte.global.status & SUSPENDED) {
				bblock.unlock();
				sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
				if (!rpc_mgr.set_stat(sgid, ~SUSPENDED, STATUS_OP_AND))
					return retval;
			}

			retval = true;
			return retval;
		}

		SGT->_SGT_error_code = 0;

		/* the next test should be done by tmaddsvc() but it currently returns SGESYSTEM
		 * if there is no room in the service table. If that changes then this code
		 * should be deleted.
		 */
		if (bbp->bbmeters.csvcs == SGC->MAXSVCS()) {
			SGT->_SGT_error_code = SGELIMIT;
			return retval;
		}
	}

	if (scte_mgr.offer(svc_name, class_name) == BADSGID) {
		// no error if entry already exists
		if (SGT->_SGT_error_code == SGEDUPENT) {
			SGT->_SGT_error_code = 0;
			retval = true;
			return retval;
		}

		return retval;
	}

	retval = true;
	return retval;
}

bool sg_api::unadvertise(const string& svc_name)
{
	sg_scte_t scte;
	sgid_t sgid;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("svc_name={1}") % svc_name).str(SGLOCALE), &retval);
#endif

	if (!SGC->enter(SGT, 0))
		return retval;

	BOOST_SCOPE_EXIT((&SGT)(&SGC)) {
		SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	if (svc_name.empty()) {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	if (bbp == NULL || SGC->_SGC_amclient) {
		SGT->_SGT_error_code = SGEPROTO;
		return retval;
	}

	// retrieve the corresponding service table entry and sgid.
	strncpy(scte.parms.svc_name, svc_name.c_str(), sizeof(scte.parms.svc_name) - 1);
	scte.parms.svc_name[sizeof(scte.parms.svc_name) - 1] = '\0';
	scte.grpid() = SGC->_SGC_proc.grpid;
	scte.svrid() = SGC->_SGC_proc.svrid;

	if (scte_mgr.retrieve(S_GRPID, &scte, &scte, &sgid) < 0)
		return retval;

	if (unadvertise_internal(sgid, U_GLOBAL) <= 0)
		return retval;

	retval = true;
	return retval;
}

int sg_api::unadvertise_internal(sgid_t sgid, int flags)
{
	sg_scte_t scte;
	int cnt;
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("sgid={1}, flags={2}") % sgid % flags).str(SGLOCALE), &retval);
#endif

	if (sgid == BADSGID) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid operation information given")).str(SGLOCALE));
		}

		return retval;
	}

	scoped_bblock bblock(SGT);

	if (scte_mgr.retrieve(S_BYSGIDS, &scte, &scte, &sgid) < 0)
		return retval;

	// get count
	if ((cnt = scte_mgr.retrieve(S_QUEUE, &scte, NULL, NULL)) < 0)
		return retval;

	boost::shared_array<sgid_t> auto_sgids(new sgid_t[cnt + 1]);
	if (auto_sgids == NULL) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = USBRK;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure")).str(SGLOCALE));
		}

		return retval;
	}
	sgid_t *sgids = auto_sgids.get();

	if ((cnt = scte_mgr.retrieve(S_QUEUE, &scte, NULL, sgids)) < 0)
		return retval;

	bblock.unlock();

	if (flags & U_LOCAL)
		retval = scte_mgr.destroy(sgids, cnt);
	else
		retval = scte_mgr.gdestroy(sgids, cnt);

	return retval;
}

// takes a call descriptor and makes it stale
bool sg_api::cancel(int cd)
{
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("cd={1}") % cd).str(SGLOCALE), &retval);
#endif

	if (!SGC->enter(SGT, 0))
		return retval;

	bool do_leave = true;
	BOOST_SCOPE_EXIT((&SGT)(&SGC)(&do_leave)) {
		if (do_leave)
			SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	if (SGC->_SGC_crte == NULL) { // no bulletin board
		SGC->leave(SGT, 0);
		do_leave = false;
		if (!init(NULL)) {
			initerr();
			return retval;
		}

		if (!SGC->enter(SGT, 0))
			return retval;
		do_leave = true;
	}

	// make sure that the handle is valid
	int i = cd & IDXMASK;
	if (i < 0 || i >= MAXHANDLES
		|| (cd & ~(IDXMASK)) != ((SGC->_SGC_handles[i] & ITMASK) << RPLYBITS)
		|| (SGC->_SGC_handles[i] & GHANDLE) == 0) {
		SGT->_SGT_error_code = SGEBADDESC;
		return retval;
	}

	--SGC->_SGC_crte->hndl.numghandle;
	if (SGC->_SGC_crte->hndl.numghandle == 0)
		SGC->_SGC_crte->rstatus &= ~P_BUSY;
	++SGC->_SGC_crte->hndl.numshandle;
	// increment iteration and stale count for handle
	if ((SGC->_SGC_handles[i]&ITMASK) == ITMASK)
		SGC->_SGC_handles[i] &= ~ITMASK;	/* reset to 0 */
	++SGC->_SGC_handles[i];
	SGC->_SGC_handles[i] += (ITMASK + 1);
	SGC->_SGC_handles[i] &= ~(GHANDLE | THANDLE);
	SGC->_SGC_handleflags[i] = 0;
	SGC->_SGC_hdllastfreed = i;		/* cache freed slot */

	retval = true;
	return retval;
}

int sg_api::getprio()
{
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (!SGC->enter(SGT, 0))
		return retval;

	bool do_leave = true;
	BOOST_SCOPE_EXIT((&SGT)(&SGC)(&do_leave)) {
		if (do_leave)
			SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	if (SGC->_SGC_crte == NULL) { // no bulletin board
		SGC->leave(SGT, 0);
		do_leave = false;
		if (!init(NULL)) {
			initerr();
			return retval;
		}

		if (!SGC->enter(SGT, 0))
			return retval;
		do_leave = true;
	}

	if (!SGT->_SGT_lastprio) {
		SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	retval = (PRIORANGE - (SGT->_SGT_lastprio - SENDBASE));
	return retval;
}

// sets the priority of the next message to be sent.
bool sg_api::setprio(int prio, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("prio={1}, flags={2}") % prio % flags).str(SGLOCALE), &retval);
#endif

	if (!SGC->enter(SGT, 0))
		return retval;

	bool do_leave = true;
	BOOST_SCOPE_EXIT((&SGT)(&SGC)(&do_leave)) {
		if (do_leave)
			SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	if (SGC->_SGC_crte == NULL) { // no bulletin board
		SGC->leave(SGT, 0);
		do_leave = false;
		if (!init(NULL)) {
			initerr();
			return retval;
		}

		if (!SGC->enter(SGT, 0))
			return retval;
		do_leave = true;
	}

	if (!SGT->_SGT_intcall && (flags & ~SGABSOLUTE)) {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}
	if (flags & SGABSOLUTE) {
		if (prio == BBRQMTYP)
			SGT->_SGT_sndprio = prio;
		else if (prio < 1 || prio > PRIORANGE)
			SGT->_SGT_sndprio = 0;		/* use default */
		else
			SGT->_SGT_sndprio = SENDBASE + prio;
	} else {
		/*
		 * this first check is to guarantee that if the user gave a
		 * prio that is a valid send priority and did not pass the
		 * TPABSOLUTE flag, that the priority wasn't mistaken for the
		 * priority to be sent but was only added to the current prio.
		 * See comment above and _tmmkprios(_TCARG) if not clear.
		 */
		if (prio > PRIORANGE)
			SGT->_SGT_sndprio = PRIORANGE;
		else
			SGT->_SGT_sndprio = prio;
	}

	retval = true;
	return retval;
}

void sg_api::set_procname(const std::string& progname)
{
	GPP->set_procname(progname);
}

void sg_api::write_log(const char *filename, int linenum, const char *msg, int len)
{
	GPP->write_log(filename, linenum, msg, len);
}

void sg_api::write_log(const char *filename, int linenum, const string& msg)
{
	GPP->write_log(filename, linenum, msg);
}

void sg_api::write_log(const string& msg)
{
	GPP->write_log(msg);
}

void sg_api::write_log(const char *msg)
{
	GPP->write_log(msg);
}

sg_api::sg_api()
{
}

int sg_api::get_error_code() const
{
	return SGT->_SGT_error_code;
}

int sg_api::get_native_code() const
{
	return SGT->_SGT_native_code;
}

int sg_api::get_ur_code() const
{
	return SGT->_SGT_ur_code;
}

int sg_api::get_error_detail() const
{
	return SGT->_SGT_error_detail;
}

const char * sg_api::strerror() const
{
	return SGT->strerror();
}

const char * sg_api::strnative() const
{
	return uunix::strerror(SGT->_SGT_native_code);
}

bool sg_api::get_config(std::string& config, const std::string& key, int version)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("key={1}, version={2}") % key % version).str(SGLOCALE), &retval);
#endif

	size_t len = sizeof(int) + key.length();
	message_pointer msg = sg_message::create(SGT);
	msg->set_length(len);

	sg_metarep_t *data = reinterpret_cast<sg_metarep_t *>(msg->data());
	data->version = version;
	memcpy(data->key, key.c_str(), key.length());

	if (!call(METAREP_SVC_NAME, msg, msg, SGSIGRSTRT)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Call operation failed, {1} - {2}") % SGT->strerror() % SGT->strnative()).str(SGLOCALE));
		return retval;
	}

	config.assign(msg->data(), msg->length());
	retval = true;
	return retval;
}

sg_api::~sg_api()
{
}

bool sg_api::sginit(int proc_type, int slot, const sg_init_t *init_info)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_config& config_mgr = sg_config::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("proc_type={1}, slot={2}, init_info={3}") % proc_type % slot % reinterpret_cast<long>(init_info)).str(SGLOCALE), &retval);
#endif

	if (SGC->_SGC_sginit_pid) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Another thread is in init.")).str(SGLOCALE));
		return retval;
	}
	SGC->_SGC_sginit_pid = getpid();

	if ((SGC->_SGC_sginit_savepid && SGC->_SGC_sginit_savepid != SGC->_SGC_sginit_pid)
		|| SGP->_SGP_ctx_state == CTX_STATE_MULTI) {
		if (SGC->_SGC_bbp != NULL)
			SGC->detach();

		SGC->_SGC_amclient = false;
		SGC->_SGC_amadm = false;
		SGC->_SGC_gotdflts = false;
		SGC->_SGC_gotparms = 0;
		SGC->_SGC_here_gotten = false;
		SGC->_SGC_proc.pid = 0;
		SGC->_SGC_slot = -1;
		SGC->_SGC_crte = NULL;
		SGC->_SGC_bbp = NULL;
		SGC->_SGC_sginit_savepid = 0;
	}

	if (proc_type == PT_ADM) {
		if (SGC->_SGC_amadm) {
			if (SGC->_SGC_sginit_savepid <= 0)
				SGC->_SGC_sginit_savepid = SGC->_SGC_sginit_pid;
			SGC->_SGC_sginit_pid = 0;
			retval = true;
			return retval;
		}

		if (SGC->_SGC_slot != -1) {
			SGT->_SGT_error_code = SGEINVAL;
			SGC->_SGC_sginit_pid = 0;
			return retval;
		}

		if (slot < 0 || slot >= MAXADMIN) {
			SGT->_SGT_error_code = SGEINVAL;
			SGC->_SGC_sginit_pid = 0;
			return retval;
		}

		SGC->set_atype("ADMIN");
	} else if (proc_type & (PT_CLIENT | PT_SYSCLT | PT_ADMCLT)) {
		if (SGC->_SGC_amclient) {
			if (SGC->_SGC_sginit_savepid <= 0)
				SGC->_SGC_sginit_savepid = SGC->_SGC_sginit_pid;
			SGC->_SGC_sginit_pid = 0;
			retval = true;
			return retval;
		}

		if (SGP->_SGP_amserver || SGC->_SGC_amadm) {
			SGT->_SGT_error_code = SGEPROTO;
			return retval;
		}

		SGC->set_atype("CLIENT");
	} else {
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	if (!SGC->get_bbparms())
		return retval;

	sg_bbparms_t& bbparms = SGC->_SGC_getbbparms_curbbp;
	uid_t uid = geteuid();

	if (!(SGC->_SGC_uid == uid && (SGC->_SGC_perm & 0600) == 0600)
		&& !(SGC->_SGC_gid == getegid() && (SGC->_SGC_perm & 060) == 060)
		&& !((SGC->_SGC_perm & 06) == 06)) {
		SGT->_SGT_error_code = SGEPERM;
		SGC->_SGC_sginit_pid = 0;
		return retval;
	}

	if (bbparms.options & SGAA) {
		string encrypted_password;

		if (init_info == NULL || init_info->passwd[0] == '\0') {
			// 尝试从.sgpasswd中读取
			encrypted_password = bbparms.passwd;
	        if (!SGP->do_auth(bbparms.perm, bbparms.ipckey, encrypted_password, true)) {
				SGT->_SGT_error_code = SGEPERM;
				SGC->_SGC_sginit_pid = 0;
				return retval;
	        }
		} else {
			if (!SGP->encrypt(init_info->passwd, encrypted_password)
				|| encrypted_password != bbparms.passwd) {
				SGT->_SGT_error_code = SGEPERM;
				SGC->_SGC_sginit_pid = 0;
				return retval;
			}
		}
	}

	if (init_info != NULL) {
		if ((strlen(init_info->usrname) >= MAX_IDENT)
			|| (strlen(init_info->cltname) >= MAX_IDENT)
			|| (strlen(init_info->passwd) >= MAX_IDENT)
			|| (strlen(init_info->grpname) >= MAX_IDENT)) {
			SGT->_SGT_error_code = SGEINVAL;
			SGC->_SGC_sginit_pid = 0;
			return retval;
		}

		if (init_info->grpname[0] != '\0') {
			sg_sgent_t sgent;
			strcpy(sgent.sgname, init_info->grpname);
			if (!config_mgr.find(sgent)) {
				SGT->_SGT_error_code = SGEINVAL;
				SGC->_SGC_sginit_pid = 0;
				return retval;
			}

			SGP->_SGP_grpid = sgent.grpid;
		} else {
			SGP->_SGP_grpid = DFLT_PGID;
		}

		if (strcmp(init_info->cltname, "sysclient") == 0
			&& !(proc_type & (PT_SYSCLT | PT_ADMCLT))) {
			SGT->_SGT_error_code = SGEPERM;
			SGC->_SGC_sginit_pid = 0;
			return retval;
		}
	} else {
		SGP->_SGP_grpid = DFLT_PGID;
	}

	if (proc_type == PT_ADM)
		SGC->_SGC_slot = bbparms.maxaccsrs + slot;

	if (!SGC->attach()) {
		if (SGT->_SGT_error_code == SGEDUPENT) {
			SGT->_SGT_error_code = SGENOENT;
		} else {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Not attached to a Bulletin Board")).str(SGLOCALE));
		}

		SGC->_SGC_slot = -1;
		SGC->_SGC_sginit_pid = 0;
		return retval;
	}

	if (SGC->_SGC_crte != NULL) {
		if (init_info != NULL) {
			strcpy(SGC->_SGC_crte->usrname, init_info->usrname);
			strcpy(SGC->_SGC_crte->cltname, init_info->cltname);
			strcpy(SGC->_SGC_crte->grpname, init_info->grpname);
		}
		SGC->_SGC_crte->rt_grpid = SGP->_SGP_grpid;
	}

	sg_svrent_t svrent;
	memset(&svrent, '\0', sizeof(sg_svrent_t));
	SGP->setids(svrent.sparms.svrproc);
	if (!SGC->get_here(svrent)) {
		SGC->qcleanup(true);
		SGC->detach();
		SGC->_SGC_sginit_pid = 0;
		return retval;
	}

	if (SGC->_SGC_crte != NULL)
		SGC->_SGC_crte->qaddr = SGC->_SGC_proc.rqaddr;

	if (proc_type == PT_ADM)
		SGC->_SGC_amadm = true;
	else
		SGC->_SGC_amclient = true;

	if (SGC->_SGC_sginit_savepid <= 0)
		SGC->_SGC_sginit_savepid = SGC->_SGC_sginit_pid;
	SGC->_SGC_sginit_pid = 0;

	// 如果在sginit()之前_SGC_blktime已经设置
	if (SGC->_SGC_amclient) {
		if (SGC->_SGC_blktime > 0)
			set_blktime(SGC->_SGC_blktime, SGBLK_ALL);
	}

	SGT->_SGT_error_code = 0;
	retval = true;
	return retval;
}

bool sg_api::sgfini(int proc_type)
{
	sgc_ctx *SGC = SGT->SGC();
	bool undo_amadm = false;
	bool undo_amclient = false;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("proc_type={1}") % proc_type).str(SGLOCALE), &retval);
#endif

	if (proc_type == PT_ADM) {
		if (!SGC->_SGC_amadm) {
			retval = true;
			return retval;
		}
		undo_amadm = true;
	} else {
		if (!SGC->_SGC_amclient) {
			if (SGP->_SGP_amserver) {
				SGT->_SGT_error_code = SGEPROTO;
				return retval;
			}

			retval = true;
			return retval;
		}
		undo_amclient = true;
	}

	if (undo_amadm)
		SGC->_SGC_amadm = false;
	if (undo_amclient)
		SGC->_SGC_amclient = false;

	SGC->qcleanup(true);
	SGC->detach();

	SGC->clear();
	SGT->clear();
	SGP->clear();

	retval = true;
	return retval;
}

void sg_api::initerr()
{
	if (SGT->_SGT_error_code == SGEPERM) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot join application - permission denied")).str(SGLOCALE));
		SGT->_SGT_error_code = SGESYSTEM;
	} else if (SGT->_SGT_error_code == SGENOENT) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot join application - accesser limit exceeded")).str(SGLOCALE));
		SGT->_SGT_error_code = SGESYSTEM;
	}
}

void sg_api::settimeleft()
{
	if (SGT->_SGT_blktime > 0) {
		SGT->_SGT_time_left = SGT->_SGT_blktime;
	} else {
		sgc_ctx *SGC = SGT->SGC();
		SGT->_SGT_time_left = SGC->_SGC_blktime;
	}
}

void sg_api::clncallblktime()
{
	SGT->_SGT_time_left = 0;
	SGT->_SGT_blktime = 0;
}

}
}

