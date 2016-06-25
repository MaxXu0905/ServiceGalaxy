#include "sg_internal.h"

namespace ai
{
namespace sg
{

sg_handle& sg_handle::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_handle *>(SGT->_SGT_mgr_array[HANDLE_MANAGER]);
}

/*
 * generates a message handle (a reply mtype) for a message
 * and stores it in the message header.
 */
int sg_handle::mkhandle(sg_message& msg, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	int i = -1;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	sg_msghdr_t& msghdr = msg;

	if (flags & SGNOREPLY) {
		if (SGC->_SGC_crte->rreqmade++ == std::numeric_limits<int>::max())
			SGC->_SGC_crte->rreqmade = 0;
		if (bbp->bbmeters.rreqmade++ == std::numeric_limits<int>::max())
			bbp->bbmeters.rreqmade = 0;
		msghdr.rplymtype = NULLMTYPE;
		retval = 0;
		return retval;
	}

	if (SGC->_SGC_hdllastfreed != -1) { // check last freed location first
		if (!(SGC->_SGC_handles[SGC->_SGC_hdllastfreed] & GHANDLE)) {
			// handle available
			i = SGC->_SGC_hdllastfreed;
		}
		SGC->_SGC_hdllastfreed = -1; // free cache
	}

	if (i == -1) {
		// need to check if all slots filled
		int count = startpoint;
		startpoint = (startpoint + 1) % MAXHANDLES;
		for (i = 0; i < MAXHANDLES; i++) {
			if (!(SGC->_SGC_handles[count] & GHANDLE)) {
				// handle available
				break;
			}
			count = (count + 1) % MAXHANDLES;
		}
		if (i == MAXHANDLES) {
			// no slots available
			SGT->_SGT_error_code = SGELIMIT;
			return retval;
		}
		i = count;
	}

	// don't allow an iteration value of 0, for the zeroith table index
	if (i == 0 && SGC->_SGC_handles[0] == 0)
		SGC->_SGC_handles[0]++;

	if (flags & SGCALL) {
		msghdr.rplymtype = CALLRPLYBASE + (SGC->_SGC_crte->hndl.gen << RPLYBITS) + i;
		SGC->_SGC_handleflags[i] = CALL_HANDLE;
	} else {
		msghdr.rplymtype = RPLYBASE + (SGC->_SGC_crte->hndl.gen << RPLYBITS) + i;
		SGC->_SGC_handleflags[i] = ACALL_HANDLE;
	}
	msghdr.rplyiter = SGC->_SGC_handles[i] & ITMASK;
	SGC->_SGC_handles[i] |= GHANDLE;

	SGC->_SGC_crte->hndl.numghandle++;

	/*
	 * See if we should bump counters.  Four cases.  1) Not internal call,
	 * 2) Internal call, level 1 with TMCALL flag, 3) Internal call, level 1
	 * with an internal service name and not a TMCALL and 4) Internal call,
	 * level 2 with an internal service name and a TMCALL.
	 */
	if (SGC->_SGC_crte->rreqmade++ == std::numeric_limits<int>::max())
		SGC->_SGC_crte->rreqmade = 0;
	if (bbp->bbmeters.rreqmade++ == std::numeric_limits<int>::max())
		bbp->bbmeters.rreqmade = 0;

	SGC->_SGC_crte->rstatus |= P_BUSY;
	retval = (i | ((SGC->_SGC_handles[i] & ITMASK) << RPLYBITS));
	return retval;
}

// validates the reply handle of a message.
bool sg_handle::chkhandle(sg_message& msg)
{
	int mtype;
	int index;
	int gen;
	sgc_ctx *SGC = SGT->SGC();
	sg_metahdr_t& metahdr = msg;
	sg_msghdr_t& msghdr = msg;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (msghdr.rplymtype != NULLMTYPE)
		mtype = msghdr.rplymtype;
	else
		mtype = metahdr.mtype;

	if (mtype & BBRQMTYP) { // a system call
		retval = true;
		return retval;
	}

	if (mtype >= CALLRPLYBASE && mtype < BBRQMTYP) {
		mtype -= CALLRPLYBASE;
	} else {
		if (!(mtype & RPLYBASE)) {
			SGT->_SGT_error_code = SGEINVAL;
			return retval;
		}

		mtype = mtype - RPLYBASE;
	}

	index = mtype & IDXMASK;
	gen = mtype >> RPLYBITS;
	/*
	 * check to see if appropriate good handle bit is on and then
	 * compare process generation and handle iteration.
	 */
	if (index > MAXHANDLES || !(SGC->_SGC_handles[index] & GHANDLE)
		|| SGC->_SGC_crte->hndl.gen != gen
		|| (SGC->_SGC_handles[index] & ITMASK) != msghdr.rplyiter) {
		SGT->_SGT_error_code = SGEBADDESC;
		return retval;
	}

	retval = true;
	return retval;
}

void sg_handle::drphandle(sg_message& msg)
{
	sgc_ctx *SGC = SGT->SGC();
	int mtype;
	int index;
	int gen;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!msg.is_rply())
		return;

	sg_metahdr_t& metahdr = msg;
	sg_msghdr_t& msghdr = msg;

	if (metahdr.mtype >= CALLRPLYBASE)
		mtype = metahdr.mtype - CALLRPLYBASE;
	else
		mtype = metahdr.mtype - RPLYBASE;

	index = mtype & IDXMASK;
	gen = mtype >> RPLYBITS;

	if ((SGC->_SGC_handles[index] & ITMASK) == msghdr.rplyiter
		&& (SGC->_SGC_handles[index] & GHANDLE)
		&& gen == SGC->_SGC_crte->hndl.gen) {
		// a good cd
		--SGC->_SGC_crte->hndl.numghandle;
		if (SGC->_SGC_crte->hndl.numghandle == 0)
			SGC->_SGC_crte->rstatus &= ~P_BUSY;
		if ((SGC->_SGC_handles[index] & ITMASK) == ITMASK)
			SGC->_SGC_handles[index] &= ~ITMASK;	// reset to 0
		++SGC->_SGC_handles[index];	// increments iteration
		SGC->_SGC_handles[index] &= ~(GHANDLE | THANDLE);
		SGC->_SGC_handleflags[index] = 0;
		SGC->_SGC_hdllastfreed = index;	// cache freed slot
	} else if (!(SGC->_SGC_handles[index] & GHANDLE)){	// a stale cd
		// only do this in case of handles[index] is NOT a good handle
		if (SGC->_SGC_crte->hndl.numshandle > 0)
			--SGC->_SGC_crte->hndl.numshandle;
		// if from a previous life, then no info in handle array to update
		if (gen == SGC->_SGC_crte->hndl.gen)
			SGC->_SGC_handles[index] -= (ITMASK + 1); // drop stale
	}
}

int sg_handle::mkstale(sg_message *msg)
{
	sgc_ctx *SGC = SGT->SGC();
	int i;
	int retval = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (msg == NULL) {
		// make all stale
		i = 0;
	} else if (chkhandle(*msg)){
		// good handle
		sg_msghdr_t& msghdr = *msg;
		i = msghdr.rplymtype & IDXMASK;
	} else {
		return retval;
	}

	for(; i < MAXHANDLES; i++) {
		if ((SGC->_SGC_handles[i] & GHANDLE) == 0) {
			// entry not used
			if (msg != NULL)
				break;	// only do specified entry
			else
				continue;	// check all
		}

		--SGC->_SGC_crte->hndl.numghandle;
		if (SGC->_SGC_crte->hndl.numghandle == 0)
			SGC->_SGC_crte->rstatus &= ~P_BUSY;
		++SGC->_SGC_crte->hndl.numshandle;
		// increment iteration and stale count for handle
		if ((SGC->_SGC_handles[i] & ITMASK) == ITMASK)
			SGC->_SGC_handles[i] &= ~ITMASK;	// reset to 0
		++SGC->_SGC_handles[i];
		SGC->_SGC_handles[i] += (ITMASK + 1);
		SGC->_SGC_handles[i] &= ~(GHANDLE | THANDLE);
		SGC->_SGC_handleflags[i] = 0;
		SGC->_SGC_hdllastfreed = i;	// cache freed slot
		retval++;	// free an entry

		if (msg != NULL)
			break;	// only do specified entry
	}

	return retval;
}


/*
 *  removes all stale message from a processes reply queue if
 *  there are any pending and if there is no good handle outstanding for that
 *  particular mtype.
 **/
void sg_handle::rcvstales()
{
	rcvstales_internal(100);
}

void sg_handle::rcvstales_internal(int count)
{
	int i;
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("count={1}") % count).str(SGLOCALE), NULL);
#endif

	if (++check4stales < count)
		return;

	check4stales = 0;
	if (SGC->_SGC_crte->hndl.numshandle <= 0) {
		SGC->_SGC_crte->hndl.numshandle = 0;
		return;
	}

	if (rcvstales_sysmsg == NULL) {
		rcvstales_sysmsg = sg_message::create(SGT);
		rcvstales_sysmsg->reserve(MAXRPC);
	}

	sg_msghdr_t *msghdr = *rcvstales_sysmsg;

	if (SGC->_SGC_crte->hndl.gen > 0) {
		/*
		 * restarted
		 * get messages from a previous generation
		 */
		/* Note: CALLRPLYBASE below was originally RPLYBASE.
		 *       We need CALLRPLYBASE to cover both
		 *       CALLRPLYBASE and RPLYBASE. We use the fact
		 *       that RPLYBASE < CALLRPLYBASE.
		 */
		msghdr->rplymtype = 1 - CALLRPLYBASE - (SGC->_SGC_crte->hndl.gen << RPLYBITS);
		msghdr->rcvr = SGC->_SGC_proc;
		while (ipc_mgr.receive(rcvstales_sysmsg, SGNOBLOCK | SGREPLY | SGSIGRSTRT)) {
			msghdr = *rcvstales_sysmsg;
			msghdr->rplymtype = 1 - CALLRPLYBASE - (SGC->_SGC_crte->hndl.gen << RPLYBITS);
			msghdr->rcvr = SGC->_SGC_proc;
			if (SGC->_SGC_crte->hndl.numshandle > 0)
				--SGC->_SGC_crte->hndl.numshandle;
		}

		SGT->_SGT_error_code = 0;
	}

	/*
	 * Try to get messages from current generation.  We can always throw
	 * these away because we won't try to receive any that has a
	 * conflict with a good handle outstanding.
	 */
	for (i = 0; i < MAXHANDLES; i++) {
		if (!(SGC->_SGC_handles[i] & GHANDLE) && (SGC->_SGC_handles[i] & STMASK)) {
			// stale pending with no good handle
			// Retrieve stales for mtypes around RPLYBASE
			msghdr->rplymtype = RPLYBASE;
			msghdr->rplymtype += (SGC->_SGC_crte->hndl.gen << RPLYBITS) + i;
			msghdr->rcvr = SGC->_SGC_proc;

			while (ipc_mgr.receive(rcvstales_sysmsg, SGNOBLOCK | SGREPLY | SGSIGRSTRT)) {
				msghdr = *rcvstales_sysmsg;
				msghdr->rplymtype = RPLYBASE;
				msghdr->rplymtype += (SGC->_SGC_crte->hndl.gen << RPLYBITS) + i;
				msghdr->rcvr = SGC->_SGC_proc;
				if (SGC->_SGC_crte->hndl.numshandle > 0)
					--SGC->_SGC_crte->hndl.numshandle;
				// decrement number of stales in handle array
				SGC->_SGC_handles[i] -= (ITMASK + 1);
			}

			// Retrieve stales for mtypes around CALLRPLYBASE
			SGT->_SGT_error_code = 0;

			msghdr->rplymtype = CALLRPLYBASE;
			msghdr->rplymtype += (SGC->_SGC_crte->hndl.gen << RPLYBITS) + i;
			msghdr->rcvr = SGC->_SGC_proc;

			while (ipc_mgr.receive(rcvstales_sysmsg, SGNOBLOCK | SGREPLY | SGSIGRSTRT)) {
				msghdr = *rcvstales_sysmsg;
				msghdr->rplymtype = RPLYBASE;
				msghdr->rplymtype += (SGC->_SGC_crte->hndl.gen << RPLYBITS) + i;
				msghdr->rcvr = SGC->_SGC_proc;
				if (SGC->_SGC_crte->hndl.numshandle > 0)
					--SGC->_SGC_crte->hndl.numshandle;
				// decrement number of stales in handle array
				SGC->_SGC_handles[i] -= (ITMASK + 1);
			}

			SGT->_SGT_error_code = 0;
		}
	}
}

/*
 * removes all stale message from a process reply queue if
 * there are any pending.
 */
void sg_handle::rcvcstales()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (SGC->_SGC_proc.rqaddr == SGC->_SGC_proc.rpaddr) {
		rcvstales();
		return;
	}

	if (++check4stales < 5)
		return;
	check4stales = 0;

	if (rcvcstales_sysmsg == NULL) {
		rcvcstales_sysmsg = sg_message::create(SGT);
		rcvcstales_sysmsg->reserve(MAXRPC);
	}

	sg_msghdr_t *msghdr = *rcvcstales_sysmsg;
	msghdr->rplymtype = NULLMTYPE;	// read in fifo order
	msghdr->rcvr = SGC->_SGC_proc;

	int save_error_code = SGT->_SGT_error_code;
	while (ipc_mgr.receive(rcvcstales_sysmsg, SGNOBLOCK | SGREPLY | SGSIGRSTRT)) {
		drphandle(*rcvcstales_sysmsg);

		msghdr = *rcvcstales_sysmsg;
		msghdr->rplymtype = NULLMTYPE;	// read in fifo order
		msghdr->rcvr = SGC->_SGC_proc;
	}

	/*
	 * Since there's no stale message in most of the times, so SGEBLOCK
	 * should be cleared.
	 */
	if (SGT->_SGT_error_code == SGEBLOCK)
		SGT->_SGT_error_code = save_error_code;
}

/*
 * calculates the send and reply priorities for a message and
 * stores it in the message header passed to it.
 */
int sg_handle::mkprios(sg_message& msg, sg_scte_t *scte, int flag)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(200, __PRETTY_FUNCTION__, (_("flag={1}") % flag).str(SGLOCALE), &retval);
#endif

	// generate the send priority
	sg_msghdr_t& msghdr = msg;

	if (SGT->_SGT_sndprio == BBRQMTYP) {
		msghdr.sendmtype = SGT->_SGT_sndprio;
	} else if (SGT->_SGT_sndprio >= SENDBASE && SGT->_SGT_sndprio <= SENDBASE + PRIORANGE - 1) {
		// valid send priority, use it
		msghdr.sendmtype = SGT->_SGT_sndprio;
	} else {
		msghdr.sendmtype = SENDBASE + scte->parms.priority;
		msghdr.sendmtype += SGT->_SGT_sndprio;
		if (msghdr.sendmtype < SENDBASE)
			msghdr.sendmtype = SENDBASE;
		if (msghdr.sendmtype > SENDBASE + PRIORANGE - 1)
			msghdr.sendmtype = SENDBASE + PRIORANGE - 1;
	}
	SGT->_SGT_sndprio = 0; // invalidate for next time

	// generate the reply priority (handle)
	retval = mkhandle(msg, flag);
	return retval;
}

// converts a message to a cd
int sg_handle::msg2cd(sg_message& msg)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (!msg.is_rply())
		return retval;

	int mtype;
	sg_metahdr_t& metahdr = msg;
	sg_msghdr_t& msghdr = msg;

	if (metahdr.mtype >= CALLRPLYBASE) {
		mtype = metahdr.mtype - CALLRPLYBASE;
	} else {
		mtype = metahdr.mtype - RPLYBASE;
	}

	retval = ((mtype & IDXMASK) | (msghdr.rplyiter << RPLYBITS));
	return retval;
}

sg_handle::sg_handle()
{
	startpoint = 0;
	check4stales = 0;
}

sg_handle::~sg_handle()
{
}

}
}

