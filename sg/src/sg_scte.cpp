#include "sg_internal.h"

using namespace std;

namespace ai
{
namespace sg
{

sg_scte& sg_scte::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_scte *>(SGT->_SGT_mgr_array[SCTE_MANAGER]);
}

sgid_t sg_scte::create(sgid_t ssgid, sg_svcparms_t& parms, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->cscte)(ssgid, parms, flags);
}

sgid_t sg_scte::smcreate(sgid_t ssgid, sg_svcparms_t& parms, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sgid_t retval = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, (_("ssgid={1}, flags={2}") % ssgid % flags).str(SGLOCALE), &retval);
#endif

	if (ssgid == BADSGID) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation information invalid")).str(SGLOCALE));
		return retval;
	}

	int sidx = sg_hashlink_t::get_index(ssgid);
	if (sidx >= SGC->MAXSVCS()) {
		SGT->_SGT_error_code = SGENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation information invalid, does not exist")).str(SGLOCALE));
		return retval;
	}

	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	sg_bbmeters_t& bbmeter = bbp->bbmeters;
	scoped_bblock bblock(SGT);

	if (bbmap->scte_free == -1) {
		SGT->_SGT_error_code = SGELIMIT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No room to create a new scte.")).str(SGLOCALE));
		return retval;
	}

	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_ste_t *parent = ste_mgr.link2ptr(sidx);

	if (ssgid != parent->hashlink.sgid) {
		SGT->_SGT_error_code = SGENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation information invalid, does not exist.")).str(SGLOCALE));
		return retval;
	}

	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_qte_t qte;
	qte.hashlink.rlink = parent->queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &qte, &qte, NULL) < 0)
		return retval;

	int bucket = hash_value(parms.svc_name);

	// 首先检查该节点是否已经创建
	sg_scte_t *scte;
	sg_ste_t *step;
	bool found = false;

	for (int idx = SGC->_SGC_scte_hash[bucket]; idx != -1; idx = scte->hashlink.fhlink) {
		scte = link2ptr(idx);
		step = ste_mgr.link2ptr(scte->family.plink);
		if (strcmp(scte->parms.svc_name, parms.svc_name) == 0) {
			if (*step == *parent) {
				found = true;
				break;
			}
		}
	}

	if (found) {
		// 重用原节点
		if ((flags & RESTARTING)
			&& parent->hashlink.rlink == scte->family.plink) {
			// restarting server
			scte->svrproc() = parent->svrproc();
			// update svc index just in case MIGRATING
			scte->parms.svc_index = parms.svc_index;

			if (rpc_mgr.ubbver() < 0)
				return retval;

			retval = scte->hashlink.sgid;
			return retval;
		}

		/*
		 * Incompatible but sane: return
		 * SGEDUPENT and schedule a cleanup
		 * if the owner of this entry is dead.
		 */
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGEDUPENT;

		int spawn_flag = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
		sg_switch::instance().bbclean(SGT, scte->mid(), SVCTAB, spawn_flag);
		return retval;
	}

	sg_scte_t *entry = sg_entry_manage<sg_scte_t>::get(this, bbmap->scte_free, bbmeter.csvcs, bbmeter.maxsvcs);
	(void)new (entry) sg_scte_t(parms);
  	sg_entry_manage<sg_scte_t>::insert_hash(this, entry, SGC->_SGC_scte_hash[bucket]);
	entry->hashlink.make_sgid(SGID_SVCT, entry);
	entry->queue = parent->queue;

	// 加到链表头
	entry->family.plink = sidx;
	entry->family.clink = -1;
	entry->family.fslink = parent->family.clink;
	entry->family.bslink = -1;
	parent->family.clink = entry->hashlink.rlink;

	// 设置节点的全局字段
	sg_gservice_t& global = entry->global;
	global.svrproc = parent->svrproc();

	if (parent->svrid() == MNGR_PRCID)
		global.status = IDLE;
	else
		global.status = IDLE | SUSPENDED;

	if (rpc_mgr.ubbver() < 0)
		return retval;

	retval = entry->hashlink.sgid;
	return retval;
}

sgid_t sg_scte::mbcreate(sgid_t ssgid, sg_svcparms_t& parms, int flags)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sgid_t retval = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, (_("ssgid={1}, flags={2}") % ssgid % flags).str(SGLOCALE), &retval);
#endif

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_svcparms_t)));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	if (flags)
		rqst->opcode = O_ASCTE | VERSION;
	else
		rqst->opcode = O_CSCTE | VERSION;
	rqst->datalen = sizeof(sg_svcparms_t);
	rqst->arg1.sgid = ssgid;
	memcpy(rqst->data(), &parms, sizeof(sg_svcparms_t));

	retval = rpc_mgr.tsnd(msg);
	return retval;
}

int sg_scte::destroy(sgid_t *sgids, int cnt)
{
	return (this->*SGT->SGC()->_SGC_bbasw->dscte)(sgids, cnt);
}

int sg_scte::gdestroy(sgid_t *sgids, int cnt)
{
	return (this->*SGT->SGC()->_SGC_bbasw->gdscte)(sgids, cnt);
}

int sg_scte::smdestroy(sgid_t *sgids, int cnt)
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
		sgid_t& sgid = sgids[i];
		int idx = sg_hashlink_t::get_index(sgid);
		if ((sgid & SGID_TMASK) != SGID_SVCT || idx > SGC->MAXSVCS())
			continue;

		sg_scte_t *scte = link2ptr(idx);
		if (scte->hashlink.sgid != sgid)
			continue;

		if (scte->family.bslink == -1) {
			// at the head of the list of services... adjust
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_ste_t *ste = ste_mgr.link2ptr(scte->family.plink);
			ste->family.clink = scte->family.fslink;
		}

		int bucket = hash_value(scte->parms.svc_name);
		sg_entry_manage<sg_scte_t>::erase_hash(this, scte, SGC->_SGC_scte_hash[bucket]);
		sg_entry_manage<sg_scte_t>::put(this, scte, bbmap->scte_free, bbmeter.csvcs);

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

int sg_scte::mbdestroy(sgid_t *sgids, int cnt)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif
	retval = sg_rpc::instance(SGT).dsgid(O_DSCTE, sgids, cnt);
	return retval;
}

int sg_scte::update(sg_scte_t *sctes, int cnt, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->uscte)(sctes, cnt, flags);
}

int sg_scte::gupdate(sg_scte_t *sctes, int cnt, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->guscte)(sctes, cnt, flags);
}

int sg_scte::smupdate(sg_scte_t *sctes, int cnt, int flags)
{
	int idx;
	int nupdates = 0;
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}, flags={2}") % cnt % flags).str(SGLOCALE), &retval);
#endif
	scoped_bblock bblock(SGT);

	for (; cnt != 0; cnt--, sctes++) {
		if ((idx = sctes->hashlink.rlink) >= SGC->MAXSVCS())
			continue;

		sg_scte_t *scte = link2ptr(idx);
		if (strcmp(scte->parms.svc_name, sctes->parms.svc_name) == 0) {
			if (flags & U_VIEW) {
				scte->global = sctes->global;
				scte->parms = sctes->parms;
			} else {
				scte->local = sctes->local;
				scte->global = sctes->global;
				scte->parms = sctes->parms;
				scte->queue = sctes->queue;
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

int sg_scte::mbupdate(sg_scte_t *sctes, int cnt, int flags)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}, flags={2}") % cnt % flags).str(SGLOCALE), &retval);
#endif

	size_t datalen = cnt * sizeof(sg_scte_t);
	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(datalen));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_USCTE | VERSION;
	rqst->datalen = datalen;
	rqst->arg1.flag = flags;
	rqst->arg2.count = cnt;
	memcpy(rqst->data(), sctes, datalen);

	retval = rpc_mgr.asnd(msg);
	return retval;
}

int sg_scte::retrieve(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids)
{
	return (this->*SGT->SGC()->_SGC_bbasw->rscte)(scope, key, sctes, sgids);
}

int sg_scte::smretrieve(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids)
{
	int bucket = 0;
	int idx;
	sg_scte_t *scte;
	int nfound = 0;
	sgid_t sgid;
	const sg_scte_t *pkey;
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("scope={1}") % scope).str(SGLOCALE), &retval);
#endif
	sharable_bblock lock(SGT);

	if (scope != S_BYSGIDS && scope != S_BYRLINK
		&& key != NULL && key->parms.svc_name[0] != '\0') {
		/*
		 * If the last referenced entry is still valid,
		 * and is the same as the one we are now after,
		 * no search is necessary.
		 */
		scte = link2ptr(SGC->_SGC_svcindx);
		sgid = scte->hashlink.sgid;
		if (sgid != BADSGID
			&& strcmp(key->parms.svc_name, scte->parms.svc_name) == 0
			&& scope == S_GRPID) {
			// cache hit
			if (sctes)
				*sctes = *scte;
			if (sgids)
				*sgids = sgid;
			return 1;
		}

		// Search the hash chain for the specified service.
		bucket = hash_value(key->parms.svc_name);
		for (idx = SGC->_SGC_scte_hash[bucket]; idx != -1; idx = scte->hashlink.fhlink) {
			scte = link2ptr(idx);
			switch (scope) {
			case S_GRPID:
				if (key->grpid() != scte->grpid() || key->svrid() != scte->svrid())
					continue;
				break;
			case S_GROUP:
				if (key->grpid() != scte->grpid())
					continue;
				break;
			case S_MACHINE:
				if (key->mid() != ALLMID && key->mid() != scte->mid())
					continue;
				break;
			case S_QUEUE:
				if (key->queue.qlink != scte->queue.qlink)
					continue;
				break;
			case S_SVRID:
				if (key->svrid() != scte->svrid())
					continue;
				break;
			case S_ANY:
				break;
			default:
				if (SGT->_SGT_error_code == 0) {
					SGT->_SGT_error_code = SGESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Parameter error in internal routine]")).str(SGLOCALE));
				}
				return -1;
			}

			if (strcmp(key->parms.svc_name, scte->parms.svc_name) != 0)
				continue;

			if (sctes != NULL)
				*sctes++ = *scte;
			if (sgids != NULL)
				*sgids++ = scte->hashlink.sgid;
			nfound++;

			if (scope == S_GRPID) {
				SGC->_SGC_svcindx = idx;
				break;
			}
		}
	} else {
		switch (scope) {
		case S_BYSGIDS:
			sgid = sgids ? *sgids++ : BADSGID;
			if (sgid != BADSGID) {
				int idx = sg_hashlink_t::get_index(sgid);
				if (idx < SGC->MAXSVCS()) {
					scte = link2ptr(idx);
					if (sgid == scte->hashlink.sgid) {
						*sctes++ = *scte;
						nfound++;
					}
				}
			}
			break;
		case S_BYRLINK:
			pkey = key ? key : sctes;
			if (!pkey || pkey->hashlink.rlink < 0 || pkey->hashlink.rlink >= SGC->MAXSVCS()) {
				SGT->_SGT_error_code = SGESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Operation parameter error in internal routine]")).str(SGLOCALE));
				return retval;
			}

			do {
				sg_scte_t *tstep = link2ptr(pkey->hashlink.rlink);
				sgid = tstep->hashlink.sgid;
				if (key)
					break;
			} while(sgid == BADSGID && (++pkey)->hashlink.rlink != -1);

			while (sgid != BADSGID) {
				int idx = sg_hashlink_t::get_index(sgid);
				if (idx < SGC->MAXSVCS()) {
					scte = link2ptr(idx);
					if (sgid == scte->hashlink.sgid) {
						if (sctes != NULL)
							*sctes++ = *scte;
						if (sgids != NULL)
							*sgids++ = scte->hashlink.sgid;
						nfound++;
					}
				}

				if (key)
					break;

				pkey++;
				if (pkey->hashlink.rlink < 0 || pkey->hashlink.rlink >= SGC->MAXSVCS())
					break;

				scte = link2ptr(pkey->hashlink.rlink);
				sgid = scte->hashlink.sgid;
			}
			break;
		case S_GRPID:
			{
				sg_ste_t skey;
				skey.svrproc() = key->svrproc();

				if (sg_ste::instance(SGT).retrieve(scope, &skey, &skey, NULL) < 0) {
					SGT->_SGT_error_code = SGENOENT;
					return -1;
				}

				for (idx = skey.family.clink; idx != -1; idx = scte->family.fslink) {
					scte = link2ptr(idx);
					if (sctes != NULL)
						*sctes++ = *scte;
					if (sgids != NULL)
						*sgids++ = scte->hashlink.sgid;
					nfound++;
				}
			}
			break;
		case S_GROUP:
			for (bucket = 0; bucket < SGC->SVCBKTS(); bucket++) {
				for (idx = SGC->_SGC_scte_hash[bucket]; idx != -1; idx = scte->hashlink.fhlink) {
					scte = link2ptr(idx);
					if (key->grpid() == scte->grpid()) {
						if (sctes != NULL)
							*sctes++ = *scte;
						if (sgids != NULL)
							*sgids++ = scte->hashlink.sgid;
						nfound++;
					}
				}
			}
			break;
		case S_MACHINE:
			for (bucket = 0; bucket < SGC->SVCBKTS(); bucket++) {
				for (idx = SGC->_SGC_scte_hash[bucket]; idx != -1; idx = scte->hashlink.fhlink) {
					scte = link2ptr(idx);
					if (key->mid() == ALLMID || key->mid() == scte->mid()) {
						if (sctes != NULL)
							*sctes++ = *scte;
						if (sgids != NULL)
							*sgids++ = scte->hashlink.sgid;
						nfound++;
					}
				}
			}
			break;
		case S_SVRID:
			for (bucket = 0; bucket < SGC->SVCBKTS(); bucket++) {
				for (idx = SGC->_SGC_scte_hash[bucket]; idx != -1; idx = scte->hashlink.fhlink) {
					scte = link2ptr(idx);
					if (key->svrid() == scte->svrid()) {
						if (sctes != NULL)
							*sctes++ = *scte;
						if (sgids != NULL)
							*sgids++ = scte->hashlink.sgid;
						nfound++;
					}
				}
			}
			break;
		case S_QUEUE:
			{
				sg_qte& qte_mgr = sg_qte::instance(SGT);
				sg_qte_t *qte = qte_mgr.link2ptr(key->queue.qlink);

				if (key->queue.qlink >= SGC->MAXQUES() || qte->hashlink.sgid == BADSGID) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Invalid operation information passed to internal routine]")).str(SGLOCALE));
					SGT->_SGT_error_code = SGESYSTEM;
					return -1;
				}

				sg_ste& ste_mgr = sg_ste::instance(SGT);
				sg_ste_t *ste;

				for (idx = qte->global.clink; idx != -1; idx = ste->family.fslink) {
					ste = ste_mgr.link2ptr(idx);
					for (idx = ste->family.clink; idx != -1; idx = scte->family.fslink) {
						scte = link2ptr(idx);
						if (sctes != NULL)
							*sctes++ = *scte;
						if (sgids != NULL)
							*sgids++ = scte->hashlink.sgid;
						nfound++;
					}
				}
			}
			break;
		case S_ANY:
			for (bucket = 0; bucket < SGC->SVCBKTS(); bucket++) {
				for (idx = SGC->_SGC_scte_hash[bucket]; idx != -1; idx = scte->hashlink.fhlink) {
					scte = link2ptr(idx);
					if (sctes != NULL)
						*sctes++ = *scte;
					if (sgids != NULL)
						*sgids++ = scte->hashlink.sgid;
					nfound++;
				}
			}
			break;
		default:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Parameter error in internal routine]")).str(SGLOCALE));
			SGT->_SGT_error_code = SGESYSTEM;
			return retval;
		}
	}

	if (nfound == 0) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	retval = nfound;
	return retval;
}

int sg_scte::mbretrieve(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("scope={1}") % scope).str(SGLOCALE), &retval);
#endif
	retval = sg_rpc::instance(SGT).rtbl(O_RSCTE, scope, key, sctes, sgids);
	return retval;
}

sgid_t sg_scte::add(sgid_t ssgid, sg_svcparms_t& parms)
{
	return (this->*SGT->SGC()->_SGC_bbasw->ascte)(ssgid, parms);
}

sgid_t sg_scte::smadd(sgid_t ssgid, sg_svcparms_t& parms)
{
	sgc_ctx *SGC = SGT->SGC();
	sgid_t retval = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, (_("ssgid={1}") % ssgid).str(SGLOCALE), &retval);
#endif

	if (ssgid == BADSGID) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation information invalid")).str(SGLOCALE));
		return retval;
	}

	int sidx = sg_hashlink_t::get_index(ssgid);
	if (sidx >= SGC->MAXSVCS()) {
		SGT->_SGT_error_code = SGENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation information invalid, does not exist")).str(SGLOCALE));
		return retval;
	}

	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	scoped_bblock bblock(SGT);

	if (bbmap->scte_free == -1) {
		SGT->_SGT_error_code = SGELIMIT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No room to create a new scte.")).str(SGLOCALE));
		return retval;
	}

	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_ste_t *parent = ste_mgr.link2ptr(sidx);

	if (ssgid != parent->hashlink.sgid) {
		SGT->_SGT_error_code = SGENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation information invalid, does not exist.")).str(SGLOCALE));
		return retval;
	}

	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_qte_t *qte = qte_mgr.link2ptr(parent->queue.qlink);
	boost::shared_array<sgid_t> auto_sgids(new sgid_t[qte->global.svrcnt]);
	sgid_t *tsgidp = auto_sgids.get();

	// 增加服务到QUEUE上的每个SERVER
	sg_ste_t *ste;
	sgid_t svrsgid;
	int idx;
	int cnt;
	for (idx = qte->global.clink, cnt = 0; idx != -1; idx = ste->family.fslink, cnt++, tsgidp++) {
		ste = ste_mgr.link2ptr(idx);
		svrsgid = ste->hashlink.sgid;

		if ((*tsgidp = create(svrsgid, parms, 0)) == BADSGID)
			break;

		// 记录下给定进程中创建的服务
		if (svrsgid == ssgid)
			retval = *tsgidp;
	}

	if (cnt != qte->global.svrcnt) {
		// 创建失败
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGESYSTEM;

		destroy(auto_sgids.get(), cnt);
		retval = BADSGID;
	}

	if (sg_rpc::instance(SGT).ubbver() < 0)
		return retval;

	return retval;
}

sgid_t sg_scte::mbadd(sgid_t ssgid, sg_svcparms_t& parms)
{
	return mbcreate(ssgid, parms, 1);
}

sgid_t sg_scte::offer(const string& svc_name, const string& class_name)
{
	sg_svcparms_t svcparms;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sgid_t retval = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, (_("svc_name={1}, class_name={2}") % svc_name % class_name).str(SGLOCALE), &retval);
#endif

	if (bbp == NULL) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: No bulletin board")).str(SGLOCALE));
		}

		return retval;
	}

	if (svc_name.empty()) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid operation information given, NULL name")).str(SGLOCALE));
		}

		return retval;
	}

	if (!SGP->_SGP_let_svc && SGP->admin_name(svc_name.c_str())) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGEINVAL;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot advertise operation names beginning with '.'")).str(SGLOCALE));
		}

		return retval;
	}

	sg_svcdsp_t *svcdsp;
	for (svcdsp = SGP->_SGP_svcdsp; svcdsp->index != -1; svcdsp++) {
		if (class_name == svcdsp->class_name)
			break;
	}

	if (svcdsp->index == -1) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid operation information given, bad class name")).str(SGLOCALE));
		}

		return retval;
	}

	strncpy(svcparms.svc_name, svc_name.c_str(), sizeof(svcparms.svc_name));
	svcparms.svc_name[sizeof(svcparms.svc_name) - 1] = '\0';
	strncpy(svcparms.svc_cname, class_name.c_str(), sizeof(svcparms.svc_cname));
	svcparms.svc_cname[sizeof(svcparms.svc_cname) - 1] = '\0';
	svcparms.svc_index = svcdsp->index;

	sg_proc_t svrproc;
	SGP->setids(svrproc);

	sg_config& config_mgr = sg_config::instance(SGT);
	if (!config_mgr.find(svcparms, svrproc.grpid))
		return retval;

	/*
	 * Add the service to all servers on the queue, or to none of them.
	 */

	if (SGP->_SGP_ambbm) {
		/*
		 * the switch entry for the sgmngr is set up for in-coming
		 * messages so it uses _tmsmaddsvc.  This doesn't work
		 * if the sgmngr itself is trying to advertize.
		 */
		retval = mbadd(SGP->_SGP_ssgid, svcparms);
	} else {
		retval = add(SGP->_SGP_ssgid, svcparms);
	}

	return retval;
}

bool sg_scte::getsvc(message_pointer& msg, sg_scte_t& scte, int flags)
{
	sg_msghdr_t& msghdr = *msg;
	char *svc_name = msghdr.svc_name;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	cache_iterator iter;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	if (svc_name[0] == '\0')
		return retval;

	// 检查版本是否最新
	bool cache_ready = true;
	if (bbversion != bbp->bbparms.bbversion) {
		cache_map.clear();
		bbversion = bbp->bbparms.bbversion;
		cache_ready = false;
	} else {
		iter = cache_map.find(svc_name);
		if (iter == cache_map.end())
			cache_ready = false;
	}

	sg_cache_t *cache;
	sg_scte_t *sctep;
	if (!cache_ready) {
		sharable_bblock lock(SGT);

		iter = cache_map.insert(std::make_pair(svc_name, sg_cache_t())).first;
		cache = &iter->second;

		int bucket = hash_value(svc_name);
		for (int idx = SGC->_SGC_scte_hash[bucket]; idx != -1; idx = sctep->hashlink.fhlink) {
			sctep = link2ptr(idx);
			if (strcmp(sctep->parms.svc_name, svc_name) == 0) {
				if (!SGC->remote(sctep->mid())) {
					if (kill(sctep->pid(), 0) == -1)
						continue;

					cache->local_sctes.push_back(sctep);
				} else {
					cache->remote_sctes.push_back(sctep);
				}
			}
		}

		if (!cache->local_sctes.empty()) {
			sctep = cache->local_sctes[0];
			cache->svc_policy = sctep->parms.svc_policy;
			cache->load_limit = sctep->parms.load_limit;
		} else if (!cache->remote_sctes.empty()) {
			sctep = cache->remote_sctes[0];
			cache->svc_policy = sctep->parms.svc_policy;
			cache->load_limit = sctep->parms.load_limit;
		} else {
			cache->svc_policy = DFLT_POLICY;
			cache->load_limit = DFLT_COSTLIMIT;
		}
		cache->local_index = 0;
		cache->remote_index = 0;
	} else {
		cache = &iter->second;
	}

	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);

	while (1) {
		// 首先查找本地是否有空闲服务
		int best_index = -1;
		long best_wkqueued = std::numeric_limits<long>::max();
		int index;

		// 从上次满足条件的下一个节点位置开始查找
		for (index = cache->local_index; index < cache->local_sctes.size(); index++) {
			sctep = cache->local_sctes[index];
			if (SGC->_SGC_special_mid != BADMID && SGC->_SGC_special_mid != sctep->mid())
				continue;

			sg_qte_t *qte = qte_mgr.link2ptr(sctep->queue.qlink);
			if (qte->global.svrcnt > 0) {
				long wkqueued = qte->local.wkqueued / qte->global.svrcnt;
				if (best_wkqueued > wkqueued) {
					best_index = index;

					if (sctep->parms.svc_policy & POLICY_IF) {
						sg_ste_t *step = ste_mgr.link2ptr(sctep->family.plink);
						if (step->global.status & IDLE)
							goto FOUND_LOCAL;
					} else if (wkqueued <= cache->load_limit) {
						goto FOUND_LOCAL;
					}

					best_wkqueued = wkqueued;
				}
			}
		}

		// 从头开始查找
		for (index = 0; index < cache->local_index; index++) {
			sctep = cache->local_sctes[index];
			if (SGC->_SGC_special_mid != BADMID && SGC->_SGC_special_mid != sctep->mid())
				continue;

			sg_qte_t *qte = qte_mgr.link2ptr(sctep->queue.qlink);
			if (qte->global.svrcnt > 0) {
				long wkqueued = qte->local.wkqueued / qte->global.svrcnt;
				if (best_wkqueued > wkqueued) {
					best_index = index;

					if (sctep->parms.svc_policy & POLICY_IF) {
						sg_ste_t *step = ste_mgr.link2ptr(sctep->family.plink);
						if (step->global.status & IDLE)
							goto FOUND_LOCAL;
					} else if (wkqueued <= cache->load_limit) {
						goto FOUND_LOCAL;
					}

					best_wkqueued = wkqueued;
				}
			}
		}

		if (best_index != -1 && (best_wkqueued <= cache->load_limit || (cache->svc_policy & POLICY_LF))) {
FOUND_LOCAL:
			cache->local_index = (best_index + 1) % cache->local_sctes.size();
			sctep = cache->local_sctes[best_index];
			if (kill(sctep->pid(), 0) == -1) {
				cache->local_sctes.erase(cache->local_sctes.begin() + best_index);
				if (cache->local_index == cache->local_sctes.size())
					cache->local_index--;
				continue;
			}

			break;
		}

		// 本地不满足，则查找服务，比较负载
		int rbest_index = -1;
		long rbest_wkqueued = std::numeric_limits<long>::max();

		// 从上次满足条件的下一个节点位置开始查找
		for (index = cache->remote_index; index < cache->remote_sctes.size(); index++) {
			sctep = cache->remote_sctes[index];
			if (SGC->_SGC_special_mid != BADMID && SGC->_SGC_special_mid != sctep->mid())
				continue;

			sg_qte_t *qte = qte_mgr.link2ptr(sctep->queue.qlink);
			if (qte->global.svrcnt > 0) {
				long wkqueued = qte->local.wkqueued / qte->global.svrcnt;
				if (rbest_wkqueued > wkqueued) {
					rbest_index = index;

					if (wkqueued <= cache->load_limit)
						goto FOUND_REMOTE;

					rbest_wkqueued = wkqueued;
				}
			}
		}

		// 从头开始查找
		for (index = 0; index < cache->remote_index; index++) {
			sctep = cache->remote_sctes[index];
			if (SGC->_SGC_special_mid != BADMID && SGC->_SGC_special_mid != sctep->mid())
				continue;

			sg_qte_t *qte = qte_mgr.link2ptr(sctep->queue.qlink);
			if (qte->global.svrcnt > 0) {
				long wkqueued = qte->local.wkqueued / qte->global.svrcnt;
				if (rbest_wkqueued > wkqueued) {
					rbest_index = index;

					if (wkqueued <= cache->load_limit)
						goto FOUND_REMOTE;

					rbest_wkqueued = wkqueued;
				}
			}
		}

		if (best_wkqueued <= rbest_wkqueued) {
			if (best_index == -1)
				return retval;

			cache->local_index = (best_index + 1) % cache->local_sctes.size();
			sctep = cache->local_sctes[best_index];
			if (kill(sctep->pid(), 0) == -1) {
				cache->local_sctes.erase(cache->local_sctes.begin() + best_index);
				if (cache->local_index == cache->local_sctes.size())
					cache->local_index--;
				continue;
			}

			break;
		} else {
FOUND_REMOTE:
			// 当前条件下rbest_index肯定不为-1
			cache->remote_index = (rbest_index + 1) % cache->remote_sctes.size();
			sctep = cache->remote_sctes[rbest_index];
			break;
		}
	}

	scte = *sctep;
	SGC->_SGC_svcindx = scte.hashlink.rlink;
	retval = true;
	return retval;
}

sg_scte_t * sg_scte::link2ptr(int link)
{
	sgc_ctx *SGC = SGT->SGC();
	return SGC->_SGC_sctes + link;
}

sg_scte_t * sg_scte::sgid2ptr(sgid_t sgid)
{
	sgc_ctx *SGC = SGT->SGC();
	return SGC->_SGC_sctes + sg_hashlink_t::get_index(sgid);
}

int sg_scte::ptr2link(const sg_scte_t *scte) const
{
	return scte->hashlink.rlink;
}

sgid_t sg_scte::ptr2sgid(const sg_scte_t *scte) const
{
	return scte->hashlink.sgid;
}

long sg_scte::get_load(const sg_scte_t& scte) const
{
	sgc_ctx *SGC = SGT->SGC();
	long load = scte.parms.svc_load;
#if defined(DEBUG)
	scoped_debug<long> debug(300, __PRETTY_FUNCTION__, "", &load);
#endif

	if (SGC->remote(scte.mid())) {
		sg_pte_t *pte = SGC->mid2pte(scte.mid());
		load += pte->netload;
	}

	return load;
}

int sg_scte::hash_value(const char *svc_name) const
{
	sgc_ctx *SGC = SGT->SGC();
	size_t len = ::strlen(svc_name);
	return static_cast<int>(boost::hash_range(svc_name, svc_name + len) % SGC->SVCBKTS());
}

sg_scte::sg_scte()
{
	bbversion = -1;
}

sg_scte::~sg_scte()
{
}

}
}

