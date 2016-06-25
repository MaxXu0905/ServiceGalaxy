#include "sg_internal.h"

namespace ai
{
namespace sg
{

sg_ste& sg_ste::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_ste *>(SGT->_SGT_mgr_array[STE_MANAGER]);
}

sgid_t sg_ste::create(sgid_t qsgid, sg_svrparms_t& parms)
{
	return (this->*SGT->SGC()->_SGC_bbasw->cste)(qsgid, parms);
}

sgid_t sg_ste::smcreate(sgid_t qsgid, sg_svrparms_t& parms)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sgid_t sgid = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, (_("qsgid={1}") % qsgid).str(SGLOCALE), &sgid);
#endif

	if (qsgid == BADSGID) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Process parameter error in internal routine]")).str(SGLOCALE));
		return sgid;
	}

	int qidx = sg_hashlink_t::get_index(qsgid);
	if (qidx >= SGC->MAXQUES()) {
		SGT->_SGT_error_code = SGENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Queue parameter qsqid {1} error in internal routine.") % qsgid).str(SGLOCALE));
		return sgid;
	}

	if (SGC->_SGC_proc.pid == 0)
		SGC->_SGC_proc.pid = getpid();

	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	sg_bbmeters_t& bbmeter = bbp->bbmeters;
	scoped_bblock bblock(SGT);

	if (bbmap->ste_free == -1) {
		SGT->_SGT_error_code = SGELIMIT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No room to create a new STE.")).str(SGLOCALE));
		return sgid;
	}

	sg_proc_t& svrproc = parms.svrproc;
	sg_ste_t *old_ste = NULL;
	sg_ste_t pkey;

	// 首先检查该节点是否已经创建
	int bucket = hash_value(svrproc.svrid);
	bool found = false;

	for (int idx = SGC->_SGC_ste_hash[bucket]; idx != -1; idx = old_ste->hashlink.fhlink) {
		old_ste = link2ptr(idx);
		if (old_ste->svrproc() == svrproc) {
			found = true;
			break;
		}
	}

	if (found) {
		// 重用原节点
		if ((old_ste->global.status & RESTARTING)
			&& old_ste->pid() == svrproc.pid) {
			if (old_ste->global.status & MIGRATING) {
				// 使用新的qaddr
				old_ste->rqaddr() = svrproc.rqaddr;
				old_ste->rpaddr() = svrproc.rpaddr;

				pkey.grpid() = (svrproc.svrid == MNGR_PRCID) ? CLST_PGID : SGC->mid2grp(svrproc.mid);
				pkey.svrid() = MNGR_PRCID;

				if (retrieve(S_GRPID, &pkey, &pkey, NULL) == 1) {
					if (!(pkey.global.status & IDLE)) {
						SGT->_SGT_error_code = SGESYSTEM;
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Process not available, doing work.")).str(SGLOCALE));
						return sgid;
					}
				}

				old_ste->family.plink = pkey.hashlink.rlink;
			} else {
				svrproc.rqaddr = old_ste->rqaddr();
				svrproc.rpaddr = old_ste->rqaddr();
			}

			if (old_ste->global.status & MIGRATING)
				old_ste->global.gen = 1;
			else
				old_ste->global.gen++;

			old_ste->global.status &= SVCTIMEDOUT;
			old_ste->global.status |= IDLE;
			old_ste->global.mtime = time(0);
			if (rpc_mgr.ubbver() < 0)
				return sgid;
			return old_ste->hashlink.sgid;
		}

		int spawn_flag = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
		sg_switch::instance().bbclean(SGT, old_ste->mid(), SVRTAB, spawn_flag);
		SGT->_SGT_error_code = SGEDUPENT;
		return sgid;
	}

	// 全新的创建，需要保证QTE已经存在
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_qte_t *qte = qte_mgr.link2ptr(qidx);
	if (qsgid != qte->hashlink.sgid) {
		SGT->_SGT_error_code = SGENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No space for another QTE, qsgid {1}") % qsgid).str(SGLOCALE));
		return sgid;
	}

	pkey.grpid() = (svrproc.svrid == MNGR_PRCID) ? CLST_PGID : SGC->mid2grp(svrproc.mid);
	pkey.svrid() = MNGR_PRCID;
	pkey.hashlink.rlink = -1;

	if (retrieve(S_GRPID, &pkey, &pkey, NULL) == 1) {
		if (!(pkey.global.status & IDLE)) {
			SGT->_SGT_error_code = SGESVRSTAT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr unstable or dead")).str(SGLOCALE));
			return sgid;
		}
	}

	sg_ste_t *entry = sg_entry_manage<sg_ste_t>::get(this, bbmap->ste_free, bbmeter.csvrs, bbmeter.maxsvrs);
	(void)new (entry) sg_ste_t(parms);
	sg_entry_manage<sg_ste_t>::insert_hash(this, entry, SGC->_SGC_ste_hash[bucket]);
	entry->svrproc() = svrproc;
	entry->hashlink.make_sgid(SGID_SVRT, entry);
	// 设置当前节点的QTE索引
	entry->queue.qlink = qidx;

	// 父节点指向BBM
	entry->family.plink = pkey.hashlink.rlink;
	// 子节点为空，表示没有SERVICE节点
	entry->family.clink = -1;
	// QTE节点中保存了使用同一QTE的SERVER的链表头，
	// 当前节点加入到该链表头中
	entry->family.fslink = qte->global.clink;
	entry->family.bslink = -1;
	qte->global.clink = entry->hashlink.rlink;
	qte->global.svrcnt++;

	// 插入到兄弟链表
	if (entry->family.fslink != -1) {
		sg_ste_t *next = link2ptr(entry->family.fslink);
		next->family.bslink = entry->hashlink.rlink;
	}

	if (rpc_mgr.ubbver() < 0)
		return sgid;

	sgid = entry->hashlink.sgid;
	return sgid;
}

sgid_t sg_ste::mbcreate(sgid_t qsgid, sg_svrparms_t& parms)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sgid_t sgid = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, (_("qsgid={1}") % qsgid).str(SGLOCALE), &sgid);
#endif

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_svrparms_t)));
	if (msg == NULL) {
		// error_code already set
		return sgid;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_CSTE | VERSION;
	rqst->datalen = sizeof(sg_svrparms_t);
	rqst->arg1.sgid = qsgid;
	memcpy(rqst->data(), &parms, sizeof(sg_svrparms_t));

	sgid = rpc_mgr.tsnd(msg);
	return sgid;
}

int sg_ste::destroy(sgid_t *sgids, int cnt)
{
	return (this->*SGT->SGC()->_SGC_bbasw->dste)(sgids, cnt);
}

int sg_ste::gdestroy(sgid_t *sgids, int cnt)
{
	return (this->*SGT->SGC()->_SGC_bbasw->gdste)(sgids, cnt);
}

int sg_ste::smdestroy(sgid_t *sgids, int cnt)
{
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
		if ((sgid & SGID_TMASK) != SGID_SVRT || idx > SGC->MAXSVRS())
			continue;

		sg_ste_t *ste = link2ptr(idx);
		if (ste->hashlink.sgid != sgid)
			continue;

		// 删除服务
		sg_scte& scte_mgr = sg_scte::instance(SGT);
		for (int cidx = ste->family.clink; cidx != -1;) {
			sg_scte_t *scte = scte_mgr.link2ptr(cidx);
			if (scte->hashlink.sgid == BADSGID) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Inconsistent operation/process associations in bulletin board]")).str(SGLOCALE));
				SGT->_SGT_error_code = SGESYSTEM;
				return retval;
			}

			cidx = scte->family.fslink;
			scte_mgr.destroy(&scte->hashlink.sgid, 1);
		}

		// 调整消息队列
		sg_qte& qte_mgr = sg_qte::instance(SGT);
		sg_qte_t *qte = qte_mgr.link2ptr(ste->queue.qlink);

		if (ste->family.bslink == -1) {
			// at the head of the list of services... adjust
			qte->global.clink = ste->family.fslink;
		}

		if (qte->global.svrcnt > 0) {
			if (qte->local.nqueued == 0)
				qte->local.wkqueued -= qte->local.wkqueued / qte->global.svrcnt;
			qte->global.svrcnt--;
		}

		int bucket = hash_value(ste->svrid());
		sg_entry_manage<sg_ste_t>::erase_hash(this, ste, SGC->_SGC_ste_hash[bucket]);
		sg_entry_manage<sg_ste_t>::put(this, ste, bbmap->ste_free, bbmeter.csvrs);
		ste->svrproc().clear();

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

int sg_ste::mbdestroy(sgid_t * sgids, int cnt)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif
	retval = sg_rpc::instance(SGT).dsgid(O_DSTE, sgids, cnt);
	return retval;
}

int sg_ste::update(sg_ste_t *stes, int cnt, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->uste)(stes, cnt, flags);
}

int sg_ste::gupdate(sg_ste_t *stes, int cnt, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->guste)(stes, cnt, flags);
}

int sg_ste::smupdate(sg_ste_t *stes, int cnt, int flags)
{
	int idx;
	int nupdates = 0;
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}, flags={2}") % cnt % flags).str(SGLOCALE), &retval);
#endif

	if (cnt < 0 || stes == NULL) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid process information given")).str(SGLOCALE));
		}
		return retval;
	}

	if (flags & U_GLOBAL) {
		retval = gupdate(stes, cnt, flags & ~U_GLOBAL);
		return retval;
	}

	scoped_bblock bblock(SGT);

	for (; cnt != 0; cnt--, stes++) {
		if ((idx = stes->hashlink.rlink) >= SGC->MAXSVRS())
			continue;

		sg_ste_t *ste = link2ptr(idx);
		if (ste->svrproc() == stes->svrproc()) {
			if (flags & U_VIEW) {
				ste->global = stes->global;
			} else {
				if (!(ste->global.status & RESTARTING)) {
					*ste = *stes;
				} else {
					ste->local = stes->local;
					ste->global = stes->global;
					ste->queue = stes->queue;
				}
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

int sg_ste::mbupdate(sg_ste_t *stes, int cnt, int flags)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}, flags={2}") % cnt % flags).str(SGLOCALE), &retval);
#endif

	size_t datalen = cnt * sizeof(sg_ste_t);
	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(datalen));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_USTE | VERSION;
	rqst->datalen = datalen;
	rqst->arg1.flag = flags;
	rqst->arg2.count = cnt;
	memcpy(rqst->data(), stes, datalen);

	retval = rpc_mgr.asnd(msg);
	return retval;
}

int sg_ste::retrieve(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids)
{
	return (this->*SGT->SGC()->_SGC_bbasw->rste)(scope, key, stes, sgids);
}

int sg_ste::smretrieve(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids)
{
	int bucket = 0;
	int idx;
	sg_ste_t *ste;
	int nfound = 0;
	sgid_t sgid;
	const sg_ste_t *pkey;
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
			if (idx < SGC->MAXSVRS()) {
				ste = link2ptr(idx);
				if (sgid == ste->hashlink.sgid) {
					*stes++ = *ste;
					nfound++;
				}
			}
		}
		break;
	case S_BYRLINK:
		pkey = key ? key : stes;
		if (!pkey || pkey->hashlink.rlink < 0 || pkey->hashlink.rlink >= SGC->MAXSVRS()) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Process parameter error in internal routine]")).str(SGLOCALE));
			return retval;
		}

		do {
			sg_ste_t *tstep = link2ptr(pkey->hashlink.rlink);
			sgid = tstep->hashlink.sgid;
			if (key)
				break;
		} while(sgid == BADSGID && (++pkey)->hashlink.rlink != -1);

		while (sgid != BADSGID) {
			int idx = sg_hashlink_t::get_index(sgid);
			if (idx < SGC->MAXSVRS()) {
				ste = link2ptr(idx);
				if (sgid == ste->hashlink.sgid) {
					if (stes != NULL)
						*stes++ = *ste;
					if (sgids != NULL)
						*sgids++ = ste->hashlink.sgid;
					nfound++;
				}
			}

			if (key)
				break;

			pkey++;
			if (pkey->hashlink.rlink < 0 || pkey->hashlink.rlink >= SGC->MAXSVRS())
				break;
			ste = link2ptr(pkey->hashlink.rlink);
			sgid = ste->hashlink.sgid;
		}
		break;
	case S_GRPID:
		bucket = hash_value(key->svrid());
		for (idx = SGC->_SGC_ste_hash[bucket]; idx != -1; idx = ste->hashlink.fhlink) {
			ste = link2ptr(idx);
			if (key->svrid() == ste->svrid()
				&& key->grpid() == ste->grpid()) {
				if (stes != NULL)
					*stes++ = *ste;
				if (sgids != NULL)
					*sgids++ = ste->hashlink.sgid;
				nfound++;
				break;
			}
		}
		break;
	case S_SVRID:
		bucket = hash_value(key->svrid());
		for (idx = SGC->_SGC_ste_hash[bucket]; idx != -1; idx = ste->hashlink.fhlink) {
			ste = link2ptr(idx);
			if (key->svrid() == ste->svrid()) {
				if (stes != NULL)
					*stes++ = *ste;
				if (sgids != NULL)
					*sgids++ = ste->hashlink.sgid;
				nfound++;
			}
		}
		break;
	case S_GROUP:
		for (bucket = 0; bucket < SGC->SVRBKTS(); bucket++) {
			for (idx = SGC->_SGC_ste_hash[bucket]; idx != -1; idx = ste->hashlink.fhlink) {
				ste = link2ptr(idx);
				if (key->grpid() == ste->grpid()) {
					if (stes != NULL)
						*stes++ = *ste;
					if (sgids != NULL)
						*sgids++ = ste->hashlink.sgid;
					nfound++;
				}
			}
		}
		break;
	case S_MACHINE:
		for (bucket = 0; bucket < SGC->SVRBKTS(); bucket++) {
			for (idx = SGC->_SGC_ste_hash[bucket]; idx != -1; idx = ste->hashlink.fhlink) {
				ste = link2ptr(idx);
				if (key->mid() == ALLMID || key->mid() == ste->mid()) {
					if (stes != NULL)
						*stes++ = *ste;
					if (sgids != NULL)
						*sgids++ = ste->hashlink.sgid;
					nfound++;
				}
			}
		}
		break;
	case S_QUEUE:
		for (bucket = 0; bucket < SGC->SVRBKTS(); bucket++) {
			for (idx = SGC->_SGC_ste_hash[bucket]; idx != -1; idx = ste->hashlink.fhlink) {
				ste = link2ptr(idx);
				if (key->queue.qlink == ste->queue.qlink) {
					if (stes != NULL)
						*stes++ = *ste;
					if (sgids != NULL)
						*sgids++ = ste->hashlink.sgid;
					nfound++;
				}
			}
		}
		break;
	default:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Parameter error in internal routine]")).str(SGLOCALE));
		SGT->_SGT_error_code = SGESYSTEM;
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

int sg_ste::mbretrieve(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("scope={1}") % scope).str(SGLOCALE), &retval);
#endif
	retval = sg_rpc::instance(SGT).rtbl(O_RSTE, scope, key, stes, sgids);
	return retval;
}

sgid_t sg_ste::offer(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt)
{
	return (this->*SGT->SGC()->_SGC_bbasw->ogsvc)(svrparms, svcparms, cnt);
}

// Atomically create a QTE, STE, and all the service table entries for a server.
sgid_t sg_ste::smoffer(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt)
{
	sgid_t retval = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif

	if (cnt < 0 || (cnt && !svcparms)) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Invalid operation information passed to internal routine]")).str(SGLOCALE));
		return retval;
	}

	int exists;
	sg_svcparms_t *svcp;
	boost::shared_array<sg_svcparms_t> nsvcp;
	bool madeq = false;
	sgid_t qsgid;
	sgid_t ssgid;
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_qte_t qte;
	scoped_bblock bblock(SGT);

	strcpy(qte.parms.saddr, svrparms.rqparms.saddr);
	if ((exists = qte_mgr.retrieve(S_QUEUE, &qte, &qte, &qsgid)) == 1) {
		switch (conform(qte, svrparms, nsvcp, cnt)) {
		case 0:
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation does not match existing operations offered")).str(SGLOCALE));
			// FALLTHRU
		case -1:
			return retval;
		default:
			break;
		}

		if (svrparms.svrproc.rqaddr == svrparms.svrproc.rpaddr)
			svrparms.svrproc.rpaddr = qte.parms.paddr;
		svrparms.svrproc.rqaddr = qte.parms.paddr;
	}

	// 如果Q还没有创建
	if (exists < 0) {
		SGT->_SGT_error_code = 0;

		qsgid = qte_mgr.create(svrparms.rqparms);
		if (qsgid == BADSGID)
			return retval; // error_code already set

		madeq = true;
	}

	sg_ste_t ste;
	int csvcflags = 0;
	ste.grpid() = svrparms.svrproc.grpid;
	ste.svrid() = svrparms.svrproc.svrid;
	if (retrieve(S_GRPID, &ste, &ste, NULL) == 1) {
		if (ste.global.status & RESTARTING)
			csvcflags = RESTARTING;
	}

	// 现在我们有了QTE，开始创建STE
	if ((ssgid = create(qsgid, svrparms)) == BADSGID) {
		if (madeq)
			qte_mgr.destroy(&qsgid, 1);
		return retval; // error_code already set
	}

	// 如果创建了Q，检查当前STE是否为restarting server
	// 如果是，则需要删除我们创建的Q
	if (madeq && (svrparms.rqparms.options & RESTARTABLE)) {
		if (retrieve(S_BYSGIDS, &ste, &ste, &ssgid) != 1) {
			qte_mgr.destroy(&qsgid, 1);
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find process entry.")).str(SGLOCALE));
			SGT->_SGT_error_code = SGENOENT;
			return retval;
		}

		qte.hashlink.rlink = ste.queue.qlink;
		if (qte_mgr.retrieve(S_BYRLINK, &qte, &qte, NULL) != 1) {
			qte_mgr.destroy(&qsgid, 1);
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find process entry.")).str(SGLOCALE));
			SGT->_SGT_error_code = SGENOENT;
			return retval;
		}

		sgid_t nsgid = qte.hashlink.sgid;
		if (nsgid != qsgid) {
			if (qte_mgr.destroy(&qsgid, 1) != 1)
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Restarting process cannot delete queue")).str(SGLOCALE));

			qsgid = nsgid;
			madeq = (qte.global.svrcnt == 1);
		}
	}

	// 创建服务
	if (nsvcp != NULL)
		svcp = nsvcp.get();
	else
		svcp = svcparms;

	int i;
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	for (i = 0; i < cnt; i++, svcp++) {
		if (scte_mgr.create(ssgid, *svcp, csvcflags) == BADSGID)
			break;
	}

	if (i < cnt) {
		if (madeq)
			qte_mgr.destroy(&qsgid, 1);	// 同时删除svr/svc
		else
			destroy(&ssgid, 1);	// 同时删除svc

		return retval; // error_code already set
	}

	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	if (rpc_mgr.ubbver() < 0)
		return retval;

	if (SGP->_SGP_boot_flags & BF_SUSPENDED) {
		if (!rpc_mgr.set_stat(ssgid, SUSPENDED, 0)) {
			if (madeq)
				qte_mgr.destroy(&qsgid, 1);	// 同时删除svr/svc
			else
				destroy(&ssgid, 1);	// 同时删除svc

			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failure to set process stat.")).str(SGLOCALE));
			return retval;
		}
	}

	if (SGP->_SGP_boot_flags & BF_SUSPAVAIL) {
		if (cnt != 0 && !svcp->admin_name()) {
			if (!rpc_mgr.set_stat(ssgid, SUSPENDED, 0))
				GPP->write_log(__FILE__, __LINE__, (_("WARN: Unable to suspend process {1}") % ssgid).str(SGLOCALE));
		}
	}

	retval = ssgid;
	return retval;
}

sgid_t sg_ste::mboffer(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt)
{
	sg_ste_t ste;
	sg_scte_t scte;
	sgid_t ssgid;
	int	locked_count;	/* locked count of services */
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sgid_t retval = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif

	int datalen = sizeof(sg_svrparms_t) + (cnt * sizeof(sg_svcparms_t));
	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(datalen));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_OGSVCS | VERSION;
	rqst->arg1.count = cnt;
	rqst->datalen = datalen;
	memcpy(rqst->data(), &svrparms, sizeof(sg_svrparms_t));
	memcpy(rqst->data() + sizeof(sg_svrparms_t), svcparms, cnt * sizeof(sg_svcparms_t));

	if((ssgid = rpc_mgr.tsnd(msg)) == BADSGID)
		return retval;

	if (cnt == 0) {
		retval = ssgid;
		return retval;
	}

	/*
	 * Phase 2 of advertise...
	 *
	 * Make sure that malloc is called OUTSIDE of the bblock.
	 * The count may change from underneath us in the following
	 * scenario:
	 * 1.	This server is part of an MSSQ set
	 * 2.	While this server is restarting, one of the other servers
	 *	in the MSSQ set advertises a service
	 *
	 * In order to guard against the above contingency, we first malloc
	 * the expected size, get the lock and then get the real count of
	 * services.  If the real count of services is greater than the
	 * size we allocated, we release the lock, and try again with the
	 * larger size.
	 *
	 * There is a known problem here.  If the list of services (or
	 * state of services) has changed between the time of the
	 * message sent and the call to tmustat, then we may leave
	 * the bulletin board in an unknown state. See MR ut96-33701
	 */
	int curr_count = cnt;
	boost::shared_array<sgid_t> auto_sgids;
	sgid_t *sgids;

	while (1) {
		auto_sgids.reset(new sgid_t[curr_count]);
		sgids = auto_sgids.get();
		if (sgids == NULL) {
			if (SGT->_SGT_error_code == 0) {
				SGT->_SGT_error_code = SGEOS;
				SGT->_SGT_native_code = USBRK;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure")).str(SGLOCALE));
			}

			return retval;
		}

		scoped_bblock bblock(SGT);

		if (retrieve(S_BYSGIDS, &ste, &ste, &ssgid) < 0)
			return retval;

		scte.svrproc() = ste.svrproc();
		scte.parms.svc_name[0] = '\0';
		if ((locked_count = scte_mgr.retrieve(S_GRPID, &scte, NULL, NULL)) < 0) {
			// If no entries, just return ssgid
			if (SGT->_SGT_error_code == SGENOENT) {
				retval = ssgid;
				return retval;
			}

			return retval;
		}

		if (locked_count != curr_count) {
			curr_count = locked_count;
		} else {
			if (scte_mgr.retrieve(S_GRPID, &scte, NULL, sgids) < 0)
				return retval;
			break;
		}
	}

	/* We don't want to suspend any services in non-application servers
	 * when BS_SUSPAVAIL or BS_SUSPENDED flag is on.
	 * Most of system servers do not advertise services,
	 * Some of them provide .. services, TMS provides TMS service
	 * and ..TMS service, TMQUEUE provides TMQUEUE service and
	 * DMADM provides service DMADMIN
	 */
	if ((SGP->_SGP_boot_flags & (BS_SUSPAVAIL | BS_SUSPENDED))
		&& (cnt != 0) && !SGP->admin_name(svcparms->svc_name)) {
		if (rpc_mgr.ustat(sgids, curr_count, SUSPENDED, 0) < 0) {
			GPP->write_log((_("WARN: Unable to suspend process {1}") % ssgid).str(SGLOCALE));
			return retval;
		}
	} else if (rpc_mgr.ustat(sgids, curr_count, ~SUSPENDED, STATUS_OP_AND) < 0) {
		return retval;
	}

	retval = ssgid;
	return retval;
}

int sg_ste::remove(sgid_t sgid)
{
	return (this->*SGT->SGC()->_SGC_bbasw->rmste)(sgid);
}

// 从BB中删除SERVER，如果是最后一个Q上的SERVER
// 则同时删除QUEUE，返回在Q上的SERVER个数
int sg_ste::smremove(sgid_t sgid)
{
	sg_ste_t ste;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("sgid={1}") % sgid).str(SGLOCALE), &retval);
#endif
	scoped_bblock bblock(SGT);

	if (retrieve(S_BYSGIDS, &ste, &ste, &sgid) != 1) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot find desired process to remove")).str(SGLOCALE));
		}
		return retval;
	}

	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_qte_t qte;
	qte.hashlink.rlink = ste.queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &qte, &qte, NULL) != 1) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot find queue entry for process that is to be removed")).str(SGLOCALE));
		}
		return retval;
	}

	if (qte.global.svrcnt == 1) {
		// 最后一个在Q上的进程
		sgid_t qsgid = qte.hashlink.sgid;
		if (qte_mgr.destroy(&qsgid, 1) != 1)
			return retval;
	} else {
		// 其它进程还在Q上
		if (destroy(&sgid, 1) != 1)
			return retval;
	}

	retval = qte.global.svrcnt;
	return retval;
}

int sg_ste::mbremove(sgid_t sgid)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("sgid={1}") % sgid).str(SGLOCALE), &retval);
#endif
	retval = sg_rpc::instance(SGT).dsgid(O_RMSTE, &sgid, 1);
	return retval;
}

sg_ste_t * sg_ste::link2ptr(int link)
{
	sgc_ctx *SGC = SGT->SGC();
	return SGC->_SGC_stes + link;
}

sg_ste_t * sg_ste::sgid2ptr(sgid_t sgid)
{
	sgc_ctx *SGC = SGT->SGC();
	return SGC->_SGC_stes + sg_hashlink_t::get_index(sgid);
}

int sg_ste::ptr2link(const sg_ste_t *ste) const
{
	return ste->hashlink.rlink;
}

sgid_t sg_ste::ptr2sgid(const sg_ste_t *ste) const
{
	return ste->hashlink.sgid;
}

int sg_ste::hash_value(int svrid) const
{
	sgc_ctx *SGC = SGT->SGC();
	return static_cast<int>(boost::hash_value(svrid) % SGC->SVRBKTS());
}

sg_ste::sg_ste()
{
}

sg_ste::~sg_ste()
{
}

// 检查给定的QTE和svrparms是否已经设置标准，返回
// 1: 满足标准
// 0: 不满足标准
// -1: 有错误
int sg_ste::conform(sg_qte_t& qte, sg_svrparms_t& svrparms, boost::shared_array<sg_svcparms_t>& nsvcp, int& cnt)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif

	// 如果没有SERVER在QTE上
	nsvcp.reset(NULL);
	if (qte.global.svrcnt == 0) {
		retval = 1;
		return retval;
	}

	sg_ste_t ste;
	ste.grpid() = svrparms.svrproc.grpid;
	ste.svrid() = svrparms.svrproc.svrid;
	if (retrieve(S_GRPID, &ste, &ste, NULL) == 1) {
		if (ste.global.status & RESTARTING) {
			// 可能在做migration
			if (strcmp(qte.parms.filename, svrparms.rqparms.filename) != 0) {
				strcpy(qte.parms.filename, svrparms.rqparms.filename);
				sg_qte& qte_mgr = sg_qte::instance(SGT);
				qte_mgr.update(&qte, 1, 0);
			}
		}
	}

	SGT->_SGT_error_code = 0;

	// 检查Q上的第一个服务是否正确
	sg_ste_t *step = link2ptr(qte.global.clink);
	if (step->grpid() != svrparms.svrproc.grpid) {
		retval = 0;
		return retval;
	}

	if (strcmp(qte.parms.filename, svrparms.rqparms.filename) != 0) {
		// 不是同一个文件，检查是否为link
		struct stat x;
		struct stat y;
		if ((stat(qte.parms.filename, &x) < 0) || (stat(svrparms.rqparms.filename, &y) < 0)) {
			SGT->_SGT_error_code = SGEOS;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot get status for process files {1} and {2}")
				% svrparms.rqparms.filename % qte.parms.filename).str(SGLOCALE));
			return retval;
		}

		if (x.st_ino != y.st_ino || x.st_dev != y.st_dev) {
			retval = 0;
			return retval;
		}
	}

	int real_cnt = 0;
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_scte_t *sctep;
	for (int idx = step->family.clink; idx != -1; idx = sctep->family.fslink) {
		sctep = scte_mgr.link2ptr(idx);
		real_cnt++;
	}

	// 所有服务都没有发布
	if (real_cnt == 0) {
		retval = 1;
		return retval;
	}

	nsvcp.reset(new sg_svcparms_t[real_cnt]);
	sg_svcparms_t *obsvcs = nsvcp.get();
	cnt = real_cnt;

	for (int idx = step->family.clink; idx != -1; idx = sctep->family.fslink) {
		sctep = scte_mgr.link2ptr(idx);
		*obsvcs++ = sctep->parms;
	}

	retval = 1;
	return retval;
}

}
}

