#include "sg_internal.h"

namespace bi = boost::interprocess;
namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

sg_qte& sg_qte::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_qte *>(SGT->_SGT_mgr_array[QTE_MANAGER]);
}

sgid_t sg_qte::create(sg_queparms_t& parms)
{
	return (this->*SGT->SGC()->_SGC_bbasw->cqte)(parms);
}

sgid_t sg_qte::smcreate(sg_queparms_t& parms)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	sg_bbmeters_t& bbmeter = bbp->bbmeters;
	sgid_t sgid = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, "", &sgid);
#endif
	scoped_bblock bblock(SGT);

	if (bbmap->qte_free == -1) {
		SGT->_SGT_error_code = SGELIMIT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No room to create a new QTE.")).str(SGLOCALE));
		return sgid;
	}

	sg_qte_t *entry = sg_entry_manage<sg_qte_t>::get(this, bbmap->qte_free, bbmeter.cques, bbmeter.maxques);
	(void)new (entry) sg_qte_t(parms);
	int bucket = hash_value(parms.saddr);
	sg_entry_manage<sg_qte_t>::insert_hash(this, entry, SGC->_SGC_qte_hash[bucket]);
	entry->hashlink.make_sgid(SGID_QUET, entry);
	if (sg_rpc::instance(SGT).ubbver() < 0)
		return sgid;

	sgid = entry->hashlink.sgid;
	return sgid;
}

sgid_t sg_qte::mbcreate(sg_queparms_t& parms)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sgid_t sgid = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, "", &sgid);
#endif

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_queparms_t)));
	if (msg == NULL) {
		// error_code already set
		return sgid;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_CQTE | VERSION;
	rqst->datalen = sizeof(sg_queparms_t);
	memcpy(rqst->data(), &parms, sizeof(sg_queparms_t));

	sgid = rpc_mgr.tsnd(msg);
	return sgid;
}

int sg_qte::destroy(sgid_t *sgids, int cnt)
{
	return (this->*SGT->SGC()->_SGC_bbasw->dqte)(sgids, cnt);
}

int sg_qte::gdestroy(sgid_t *sgids, int cnt)
{
	return (this->*SGT->SGC()->_SGC_bbasw->gdqte)(sgids, cnt);
}

int sg_qte::smdestroy(sgid_t *sgids, int cnt)
{
	int cidx;
	int ndeleted = 0;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	sg_bbmeters_t& bbmeter = bbp->bbmeters;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif
	scoped_bblock bblock(SGT);

	for (int i = 0; i < cnt; i++) {
		sgid_t sgid = sgids[i];
		int idx = sg_hashlink_t::get_index(sgid);
		if ((sgid & SGID_TMASK) != SGID_QUET || idx > SGC->MAXQUES())
			continue;

		sg_qte_t *qte = link2ptr(idx);
		if (qte->hashlink.sgid != sgid)
			continue;

		// Delete all servers on queue
		sg_ste& ste_mgr = sg_ste::instance(SGT);
		for (cidx = qte->global.clink; cidx != -1;) {
			sg_ste_t *ste = ste_mgr.link2ptr(cidx);

			cidx = ste->family.fslink;
			if (ste->hashlink.sgid == 0) {
				SGT->_SGT_error_code = SGENOENT;
				return retval;
			}

			ste_mgr.destroy(&ste->hashlink.sgid, 1);
		}

		int bucket = hash_value(qte->saddr());
		sg_entry_manage<sg_qte_t>::erase_hash(this, qte, SGC->_SGC_qte_hash[bucket]);
		sg_entry_manage<sg_qte_t>::put(this, qte, bbmap->qte_free, bbmeter.cques);

		ndeleted++;
	}

	if (ndeleted == 0) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	if (sg_rpc::instance(SGT).ubbver() < 0)
		return retval;

	retval = ndeleted;
	return retval;
}

int sg_qte::mbdestroy(sgid_t * sgids, int cnt)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif
	retval = sg_rpc::instance(SGT).dsgid(O_DQTE, sgids, cnt);
	return retval;
}

int sg_qte::update(sg_qte_t *qtes, int cnt, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->uqte)(qtes, cnt, flags);
}

int sg_qte::gupdate(sg_qte_t *qtes, int cnt, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->guqte)(qtes, cnt, flags);
}

int sg_qte::smupdate(sg_qte_t *qtes, int cnt, int flags)
{
	int idx;
	int nupdates = 0;
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}, flags={2}") % cnt % flags).str(SGLOCALE), &retval);
#endif

	if (cnt < 0 || qtes == NULL) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid queue information given")).str(SGLOCALE));
		}
		return retval;
	}

	if (flags & U_GLOBAL) {
		retval = gupdate(qtes, cnt, flags & ~U_GLOBAL);
		return retval;
	}

	scoped_bblock bblock(SGT);

	for (; cnt != 0; cnt--, qtes++) {
		if ((idx = qtes->hashlink.rlink) >= SGC->MAXQUES())
			continue;

		sg_qte_t *qte = link2ptr(idx);
		if (strcmp(qte->saddr(), qtes->saddr()) == 0) {
			if (flags & U_VIEW) {
				qtes->global.clink = qte->global.clink;
				qte->global = qtes->global;
			} else {
				qte->local = qtes->local;
				qte->global = qtes->global;
				qte->parms = qtes->parms;
			}
			nupdates++;
		}
	}

	if (nupdates == 0) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	if (sg_rpc::instance(SGT).ubbver() < 0)
		return retval;

	retval = nupdates;
	return retval;
}

int sg_qte::mbupdate(sg_qte_t *qtes, int cnt, int flags)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}, flags={2}") % cnt % flags).str(SGLOCALE), &retval);
#endif

	size_t datalen = cnt * sizeof(sg_qte_t);
	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(datalen));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_UQTE | VERSION;
	rqst->datalen = datalen;
	rqst->arg1.flag = flags;
	rqst->arg2.count = cnt;
	memcpy(rqst->data(), qtes, datalen);

	retval = rpc_mgr.asnd(msg);
	return retval;
}

int sg_qte::retrieve(int scope, const sg_qte_t *key, sg_qte_t *qtes, sgid_t *sgids)
{
	return (this->*SGT->SGC()->_SGC_bbasw->rqte)(scope, key, qtes, sgids);
}

int sg_qte::smretrieve(int scope, const sg_qte_t *key, sg_qte_t *qtes, sgid_t *sgids)
{
	int bucket = 0;
	int idx;
	sg_qte_t *qte;
	int nfound = 0;
	sgid_t sgid;
	const sg_qte_t *pkey;
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("scope={1}") % scope).str(SGLOCALE), &retval);
#endif
	sharable_bblock lock(SGT);

	switch (scope) {
	case S_BYSGIDS:
		sgid = sgids ? *sgids++ : BADSGID;
		if (sgid != BADSGID) {
			int idx = sg_hashlink_t::get_index(sgid);
			if (idx < SGC->MAXQUES()) {
				qte = link2ptr(idx);
				if (sgid == qte->hashlink.sgid) {
					*qtes++ = *qte;
					nfound++;
				}
			}
		}
		break;
	case S_BYRLINK:
		pkey = key ? key : qtes;
		if (!pkey || pkey->hashlink.rlink < 0 || pkey->hashlink.rlink >= SGC->MAXQUES()) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Queue parameter error in internal routine]")).str(SGLOCALE));
			return retval;
		}

		do {
			sg_qte_t *tqtep = link2ptr(pkey->hashlink.rlink);
			sgid = tqtep->hashlink.sgid;
			if (key)
				break;
		} while(sgid == BADSGID && (++pkey)->hashlink.rlink != -1);

		while (sgid != BADSGID) {
			int idx = sg_hashlink_t::get_index(sgid);
			if (idx < SGC->MAXQUES()) {
				qte = link2ptr(idx);
				if (sgid == qte->hashlink.sgid) {
					if (qtes != NULL)
						*qtes++ = *qte;
					if (sgids != NULL)
						*sgids++ = qte->hashlink.sgid;
					nfound++;
				}
			}

			if (key)
				break;

			pkey++;
			if (pkey->hashlink.rlink < 0 || pkey->hashlink.rlink >= SGC->MAXQUES())
				break;

			qte = link2ptr(pkey->hashlink.rlink);
			sgid = qte->hashlink.sgid;
		}
		break;
	case S_QUEUE:
		if (key == NULL) {
			if (SGT->_SGT_error_code == 0) {
				SGT->_SGT_error_code = SGESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Queue parameter error in internal routine]")).str(SGLOCALE));
			}
			return -1;
		}

		if (key->saddr()[0] != '\0') {
			bucket = hash_value(key->saddr());
			for (idx = SGC->_SGC_qte_hash[bucket]; idx != -1; idx = qte->hashlink.fhlink) {
				qte = link2ptr(idx);
				if (strcmp(key->saddr(), qte->saddr()) == 0) {
					if (qtes != NULL)
						*qtes++ = *qte;
					if (sgids != NULL)
						*sgids++ = qte->hashlink.sgid;
					nfound++;
					break;
				}
			}
		} else {
			for (bucket = 0; bucket < SGC->QUEBKTS(); bucket++) {
				for (idx = SGC->_SGC_qte_hash[bucket]; idx != -1; idx = qte->hashlink.fhlink) {
					qte = link2ptr(idx);
					if (qtes != NULL)
						*qtes++ = *qte;
					if (sgids != NULL)
						*sgids++ = qte->hashlink.sgid;
					nfound++;
				}
			}
		}
		break;
	default:
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Queue parameter error in internal routine]")).str(SGLOCALE));
		}
		return retval;
	}

	if (nfound == 0) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	retval = nfound;
	return retval;
}

int sg_qte::mbretrieve(int scope, const sg_qte_t *key, sg_qte_t *qtes, sgid_t *sgids)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("scope={1}") % scope).str(SGLOCALE), &retval);
#endif
	retval = sg_rpc::instance(SGT).rtbl(O_RQTE, scope, key, qtes, sgids);
	return retval;
}

bool sg_qte::enqueue(sg_scte_t *scte)
{
	return (this->*SGT->SGC()->_SGC_ldbsw->enqueue)(scte);
}

bool sg_qte::rtenq(sg_scte_t *scte)
{
	return commenq(scte, true);
}

bool sg_qte::rrenq(sg_scte_t *scte)
{
	return commenq(scte, false);
}

bool sg_qte::nullenq(sg_scte_t *scte)
{
	return true;
}

bool sg_qte::dequeue(sg_scte_t *scte, bool undo)
{
	return (this->*SGT->SGC()->_SGC_ldbsw->dequeue)(scte, undo);
}

bool sg_qte::rtdeq(sg_scte_t *scte, bool undo)
{
	return commdeq(scte, undo, true);
}

bool sg_qte::rrdeq(sg_scte_t *scte, bool undo)
{
	return commdeq(scte, undo, false);
}

bool sg_qte::nulldeq(sg_scte_t *scte, bool undo)
{
	return true;
}

int sg_qte::choose(sg_qte_t *curr, sg_qte_t *best, long curr_load, long best_load)
{
	return (this->*SGT->SGC()->_SGC_ldbsw->choose)(curr, best, curr_load, best_load);
}

/*
 *  queue comparison for both real-time (SHM) and round-robin (MP)
 *             load balancing.
 *
 *  Return values:
 * 	-1 "curr" is better than "best"
 *	 0 "curr" is better than all other possible choices
 *	+1 "best" is better than "curr"
 * 	+2 "curr" is equal to "best"
 */
int sg_qte::realchoose(sg_qte_t *curr, sg_qte_t *best, long curr_load, long best_load)
{
	long cwkqueued;				// Per-server work units in "curr" queue
	long bwkqueued;				// Per-server work units in "best" queue
	int indx;					// Index to server table
	sg_ste_t *stp = NULL;		// Server table entry pointer
	bool local = false;			// true if "curr" has a local idle server
	bool localbest = false;		// true if "best" is local server
	bool idlebest = false;		// true if "best" is local and idle
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("curr_load={1}, best_load={2}") % curr_load % best_load).str(SGLOCALE), &retval);
#endif

	// Determine if "curr" queue has an idle local server
	for (indx = curr->global.clink; indx != -1; indx = stp->family.fslink) {
		stp = ste_mgr.link2ptr(indx);
		if (SGC->remote(stp->mid())) // If any server is non-local, all are
			break;

		if (stp->svrproc().is_bbm()) {
			retval = 0;
			return retval;
		}

		if (stp->global.status == IDLE) {
			/*
			 *  This server considered "idle" if IDLE bit is
			 *  set but no others:  not BUSY, RESTARTING,
			 *  SUSPENDED, CLEANING, SHUTDOWN, etc.
			 */
			local = true;
			break;
		}
	}

	/*
	 *  An idle local server is always preferred over remote.
	 *  Continue to load balance across all local idle servers,
	 *  in case process scheduling causes one to become "stuck"
	 *  in idle state and not reading from queue. If using service
	 *  caching, the BB may not be locked, in which case we'll
	 *  make this check back in the service caching code rather
	 *  than here.
	 */
	indx = best->global.clink;
	if (indx != -1) {
		stp = ste_mgr.link2ptr(indx);
		if (!SGC->remote(stp->mid()))
			localbest = true;
	}

	if (local && !localbest) // "curr" queue wins comparison
		return retval;

	if (localbest) {
		//  Determine if "best" queue has an idle local server
		for (; indx != -1; indx = stp->family.fslink) {
			stp = ste_mgr.link2ptr(indx);
			if (stp->global.status == IDLE || stp->svrproc().is_bbm()) {
				idlebest = true;
				break;
			}
		}
		if (local && !idlebest) // "curr" queue wins comparison
			return retval;

		if (!local && idlebest) { // "best" queue wins comparison
			retval = 1;
			return retval;
		}
	}

	// Compute "curr" queue's per-server workload
	if (curr->global.svrcnt != 0) { // prevent divide by zero
		cwkqueued = curr->local.wkqueued / curr->global.svrcnt;
		cwkqueued += curr_load; // consider extra load if chosen
	} else {
		cwkqueued = std::numeric_limits<long>::max(); // should never happen
	}

	// Compute "best" queue's per-server workload
	if (best->global.svrcnt != 0) { // prevent divide by zero
		bwkqueued = best->local.wkqueued / best->global.svrcnt;
		bwkqueued += best_load; // consider extra load if chosen
	} else {
		bwkqueued = std::numeric_limits<long>::max(); // best's svrcnt may be 0 (see getsrsc)
	}

	if (cwkqueued < bwkqueued) // "curr" queue wins comparison
		return retval;

	if (cwkqueued == bwkqueued) { // "curr" and "best" are equal
		retval = 2;
		return retval;
	}

	retval = 1;
	return retval; // "best" queue wins comparison
}

int sg_qte::nullchoose(sg_qte_t *curr, sg_qte_t *best, long curr_load, long best_load)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("curr_load={1}, best_load={2}") % curr_load % best_load).str(SGLOCALE), &retval);
#endif
	retval = 0;
	return retval;
}

void sg_qte::syncq()
{
	(this->*SGT->SGC()->_SGC_ldbsw->syncq)();
}

/*
 *  Queue synchronization for "RR" (round-robin) load balancing.
 *  Reset the local.wkqueued element of every QTE to 0, tossing away all
 *  prior history by effectively restarting the algorithm.  This keeps newly
 *  booted servers from being clobbered with lots of load, and fixes potential imbalances
 *  caused by data-dependent-routing and local-idle-server-preference.
 */
void sg_qte::syncqrr()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_qte& qte_mgr = sg_qte::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", NULL);
#endif
	scoped_bblock bblock(SGT);

	int nbkts = SGC->QUEBKTS();
	for (int bucket = 0; bucket < nbkts; bucket++) {
		// For each hash bucket of the queue table
		sg_qte_t *qte;
		for (int qidx = SGC->_SGC_qte_hash[bucket]; qidx != -1; qidx = qte->hashlink.fhlink) {
			// For each queue
			qte = qte_mgr.link2ptr(qidx);
			qte->local.wkqueued = 0;
		}
	}
}

/*
 *  Queue synchronization for "RT" (real-time) load balancing.
 *  RT load balancing depends on exact values in wkqueued.
 *  If exact statistics are not turned on (and they're not
 *  by default), a race condition can prevent wkqueued from being
 *  decremented properly.  This routine checks for queues that are
 *  completely idle, and resets their status counters to zero.
 */
void sg_qte::syncqrt()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_qte_t *qte;
	sg_ste_t *ste;
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", NULL);
#endif
	scoped_bblock bblock(SGT);

	int nbkts = SGC->QUEBKTS();
	for (int bucket = 0; bucket < nbkts; bucket++) {
		// For each hash bucket of the queue table
		for (int qidx = SGC->_SGC_qte_hash[bucket]; qidx != -1; qidx = qte->hashlink.fhlink) {
			// For each queue
			qte = qte_mgr.link2ptr(qidx);

			bool allidle = true; // Assume all servers idle
			for (int sidx = qte->global.clink; sidx != -1; sidx = ste->family.fslink) {
				// For each server reading from queue
				ste = ste_mgr.link2ptr(sidx);
				if (ste->global.status != IDLE) {
					allidle = false; // At least 1 not idle
					break;
				}
			}

			if (allidle) {
				try {
					string qname = (boost::format("{1}.{2}") % SGC->midkey(SGC->_SGC_proc.mid) % qte->paddr()).str();
					bi::message_queue queue(bi::open_only, qname.c_str());
					size_t num_msgs = queue.get_num_msg();

					if (num_msgs != 0) // Do nothing is Q not really empty
						continue;

					// Otherwise, reset statistics
					qte->local.nqueued = 0;
					qte->local.wkqueued = 0;
				} catch (bi::interprocess_exception& ex) {
					// 忽略打开消息队列的错误
				}
			}
		}
	}
}

void sg_qte::syncqnull()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", NULL);
#endif
}

sg_qte_t * sg_qte::link2ptr(int link)
{
	sgc_ctx *SGC = SGT->SGC();
	return SGC->_SGC_qtes + link;
}

sg_qte_t * sg_qte::sgid2ptr(sgid_t sgid)
{
	sgc_ctx *SGC = SGT->SGC();
	return SGC->_SGC_qtes + sg_hashlink_t::get_index(sgid);
}

int sg_qte::ptr2link(const sg_qte_t *qte) const
{
	return qte->hashlink.rlink;
}

sgid_t sg_qte::ptr2sgid(const sg_qte_t * qte) const
{
	return qte->hashlink.sgid;
}

sg_qte::sg_qte()
{
}

sg_qte::~sg_qte()
{
}

int sg_qte::hash_value(const char *saddr) const
{
	sgc_ctx *SGC = SGT->SGC();
	size_t len = ::strlen(saddr);
	return static_cast<int>(boost::hash_range(saddr, saddr + len) % SGC->QUEBKTS());
}

bool sg_qte::commenq(sg_scte_t *scte, bool rt)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("rt={1}") % rt).str(SGLOCALE), &retval);
#endif

	sg_scte_t *sctep = scte_mgr.link2ptr(SGC->_SGC_svcindx);
	if (sctep->hashlink.sgid == BADSGID && strcmp(sctep->parms.svc_name, scte->parms.svc_name) != 0) {
		// shouldn't happen
		int indx = -1;
		if (SGC->_SGC_bbp->bblock_pid == SGC->_SGC_proc.pid) {
			const char *svc_name = scte->parms.svc_name;
			int bucket = scte_mgr.hash_value(svc_name);
			for (indx = SGC->_SGC_scte_hash[bucket]; indx != -1; indx = sctep->hashlink.fhlink) {
				sctep = scte_mgr.link2ptr(indx);
				if (scte->svrproc() != sctep->svrproc())
					continue;
				if (strcmp(svc_name, sctep->parms.svc_name))
					continue;
				SGC->_SGC_svcindx = indx;
				break;
			}
		}
		if (indx == -1) {
			SGT->_SGT_error_code = SGENOENT;
			return retval;
		}
	}

	sg_qte_t *qte = link2ptr(scte->queue.qlink);

	// 设置本地NON-MSSQ server为忙
	if (!SGC->remote(scte->mid()) && qte->global.svrcnt == 1) {
		sg_ste_t *ste = ste_mgr.link2ptr(qte->global.clink);
		ste->global.status |= BUSY;
	}

	long load = scte_mgr.get_load(*scte);
	if (rt) {
		qte->local.totwkqueued += qte->local.wkqueued;
		qte->local.totnqueued += qte->local.nqueued;
		qte->local.nqueued++;
		scte->local.nqueued++;
		bbp->bbmeters.currload += scte->parms.svc_load;
	}

	qte->local.wkqueued += load;
	bbp->bbmeters.wkinitiated += scte->parms.svc_load;

	if (qte->local.wkqueued < 0) {
		syncqrr();
		qte->local.wkqueued += load;
	}

	retval = true;
	return retval;
}

bool sg_qte::commdeq(sg_scte_t *scte, bool undo, bool rt)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("rt={1}") % rt).str(SGLOCALE), &retval);
#endif

	sg_scte_t *sctep = scte_mgr.link2ptr(SGC->_SGC_svcindx);
	if (sctep->hashlink.sgid == BADSGID && strcmp(sctep->parms.svc_name, scte->parms.svc_name) != 0) {
		// shouldn't happen
		int indx = -1;
		if (SGC->_SGC_bbp->bblock_pid == SGC->_SGC_proc.pid) {
    		const char *svc_name = scte->parms.svc_name;
    		int bucket = scte_mgr.hash_value(svc_name);
    		for (indx = SGC->_SGC_scte_hash[bucket]; indx != -1; indx = sctep->hashlink.fhlink) {
				sctep = scte_mgr.link2ptr(indx);
				if (scte->svrproc() != sctep->svrproc())
					continue;
				if (strcmp(svc_name, sctep->parms.svc_name))
					continue;
           		SGC->_SGC_svcindx = indx;
           		break;
    		}
		}
		if (indx == -1) {
			SGT->_SGT_error_code = SGENOENT;
			return retval;
		}
	}

	sg_qte_t *qte = link2ptr(scte->queue.qlink);
	long load = scte_mgr.get_load(*scte);

	if (undo || rt) {
		if (--qte->local.nqueued < 0)
			qte->local.nqueued = 0;
		if (--scte->local.nqueued < 0)
			scte->local.nqueued = 0;
		qte->local.wkqueued -= load;
		if (qte->local.wkqueued < 0)
			qte->local.wkqueued = 0;
		bbp->bbmeters.currload -= scte->parms.svc_load;
		if (bbp->bbmeters.currload < 0)
			bbp->bbmeters.currload = 0;

		// if unenqueue is set, this is an unenqueue request
		if (undo) {
			bbp->bbmeters.wkinitiated -= scte->parms.svc_load;
			if (bbp->bbmeters.wkinitiated < 0)
				bbp->bbmeters.wkinitiated = 0;
			qte->local.totwkqueued -= qte->local.wkqueued;
			if (qte->local.totwkqueued < 0)
				qte->local.totwkqueued = 0;
			qte->local.totnqueued -= qte->local.nqueued;
			if (qte->local.totnqueued < 0)
				qte->local.totnqueued = 0;
		}
	}

	if (!undo) {
		bbp->bbmeters.wkcompleted += scte->parms.svc_load;
		scte->local.ncompleted++;
	}

	retval = true;
	return retval;
}

}
}

