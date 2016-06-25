#include "sg_internal.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

sg_sgte& sg_sgte::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_sgte *>(SGT->_SGT_mgr_array[SGTE_MANAGER]);
}

sgid_t sg_sgte::create(sg_sgparms_t& parms)
{
	return (this->*SGT->SGC()->_SGC_bbasw->csgte)(parms);
}

sgid_t sg_sgte::smcreate(sg_sgparms_t& parms)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	sg_bbmeters_t& bbmeter = bbp->bbmeters;
	sgid_t retval = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif
	scoped_bblock bblock(SGT);

	if (bbmap->sgte_free == -1) {
		SGT->_SGT_error_code = SGELIMIT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No room to create a new sgte.")).str(SGLOCALE));
		return retval;
	}

	sg_sgte_t *entry = sg_entry_manage<sg_sgte_t>::get(this, bbmap->sgte_free, bbmeter.csgt, bbmeter.maxsgt);
	(void)new (entry) sg_sgte_t(parms);
	int bucket = hash_value(parms.sgname);
	sg_entry_manage<sg_sgte_t>::insert_hash(this, entry, SGC->_SGC_sgte_hash[bucket]);
	entry->hashlink.make_sgid(SGID_SGT, entry);
	if (sg_rpc::instance(SGT).ubbver() < 0)
		return retval;

	retval = entry->hashlink.sgid;
	return retval;
}

sgid_t sg_sgte::mbcreate(sg_sgparms_t& parms)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sgid_t retval = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_sgparms_t)));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_CSGTE | VERSION;
	rqst->datalen = sizeof(sg_sgparms_t);
	memcpy(rqst->data(), &parms, sizeof(sg_sgparms_t));

	retval = rpc_mgr.tsnd(msg);
	return retval;
}

int sg_sgte::destroy(sgid_t *sgids, int cnt)
{
	return (this->*SGT->SGC()->_SGC_bbasw->dsgte)(sgids, cnt);
}

int sg_sgte::smdestroy(sgid_t *sgids, int cnt)
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
		if ((sgid & SGID_TMASK) != SGID_SGT || idx > SGC->MAXSGT())
			continue;

		sg_sgte_t *sgte = link2ptr(idx);
		if (sgte->hashlink.sgid != sgid)
			continue;

		int bucket = hash_value(sgte->parms.sgname);
		sg_entry_manage<sg_sgte_t>::erase_hash(this, sgte, SGC->_SGC_sgte_hash[bucket]);
		sg_entry_manage<sg_sgte_t>::put(this, sgte, bbmap->sgte_free, bbmeter.csgt);

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

int sg_sgte::mbdestroy(sgid_t *sgids, int cnt)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif
	retval = sg_rpc::instance(SGT).dsgid(O_DSGTE, sgids, cnt);
	return retval;
}

int sg_sgte::nulldestroy(sgid_t *sgids, int cnt)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}") % cnt).str(SGLOCALE), &retval);
#endif
	retval = 0;
	return retval;
}

int sg_sgte::update(sg_sgte_t *sgtes, int cnt, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->usgte)(sgtes, cnt, flags);
}

int sg_sgte::smupdate(sg_sgte_t *sgtes, int cnt, int flags)
{
	int idx;
	int nupdates = 0;
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}, flags={2}") % cnt % flags).str(SGLOCALE), &retval);
#endif
	scoped_bblock bblock(SGT);

	for (; cnt != 0; cnt--, sgtes++) {
		if ((idx = sgtes->hashlink.rlink) >= SGC->MAXSGT())
			continue;

		sg_sgte_t *sgte = link2ptr(idx);
		if (strcmp(sgte->parms.sgname, sgtes->parms.sgname) == 0) {
			sgte->parms = sgtes->parms;
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

int sg_sgte::mbupdate(sg_sgte_t *sgtes, int cnt, int flags)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("cnt={1}, flags={2}") % cnt % flags).str(SGLOCALE), &retval);
#endif

	size_t datalen = cnt * sizeof(sg_sgte_t);
	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(datalen));
	if (msg == NULL) {
		// error_code already set
		return retval;
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_USGTE | VERSION;
	rqst->datalen = datalen;
	rqst->arg1.flag = flags;
	rqst->arg2.count = cnt;
	memcpy(rqst->data(), sgtes, datalen);

	retval = rpc_mgr.asnd(msg);
	return retval;
}

int sg_sgte::retrieve(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids)
{
	return (this->*SGT->SGC()->_SGC_bbasw->rsgte)(scope, key, sgtes, sgids);
}

int sg_sgte::smretrieve(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids)
{
	int bucket = 0;
	int idx;
	sg_sgte_t *sgte;
	int nfound = 0;
	sgid_t sgid;
	const sg_sgte_t *pkey;
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
			if (idx < SGC->MAXSGT()) {
				sgte = link2ptr(idx);
				if (sgid == sgte->hashlink.sgid) {
					*sgtes++ = *sgte;
					nfound++;
				}
			}
		}
		break;
	case S_BYRLINK:
		pkey = key ? key : sgtes;
		if (!pkey || pkey->hashlink.rlink < 0 || pkey->hashlink.rlink >= SGC->MAXSGT()) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Process Group parameter error in internal routine]")).str(SGLOCALE));
			return retval;
		}

		do {
			sg_sgte_t *tsgtep = link2ptr(pkey->hashlink.rlink);
			sgid = tsgtep->hashlink.sgid;
			if (key)
				break;
		} while(sgid == BADSGID && (++pkey)->hashlink.rlink != -1);

		while (sgid != BADSGID) {
			int idx = sg_hashlink_t::get_index(sgid);
			if (idx < SGC->MAXSGT()) {
				sgte = link2ptr(idx);
				if (sgid == sgte->hashlink.sgid) {
					if (sgtes != NULL)
						*sgtes++ = *sgte;
					if (sgids != NULL)
						*sgids++ = sgte->hashlink.sgid;
					nfound++;
				}
			}

			if (key)
				break;

			pkey++;
			if (pkey->hashlink.rlink < 0 || pkey->hashlink.rlink < SGC->MAXSGT())
				break;

			sgte = link2ptr(pkey->hashlink.rlink);
			sgid = sgte->hashlink.sgid;
		}
		break;
	case S_GROUP:
		for (bucket = 0; bucket < SGC->SGTBKTS(); bucket++) {
			for (idx = SGC->_SGC_sgte_hash[bucket]; idx != -1; idx = sgte->hashlink.fhlink) {
				sgte = link2ptr(idx);
				if (key->parms.grpid == sgte->parms.grpid) {
					if (sgtes != NULL)
						*sgtes++ = *sgte;
					if (sgids != NULL)
						*sgids++ = sgte->hashlink.sgid;
					nfound++;
				}
			}
		}
		break;
	case S_MACHINE:
		for (bucket = 0; bucket < SGC->SGTBKTS(); bucket++) {
			for (idx = SGC->_SGC_sgte_hash[bucket]; idx != -1; idx = sgte->hashlink.fhlink) {
				sgte = link2ptr(idx);
				if (key->curmid() == ALLMID || key->curmid() == sgte->curmid()) {
					if (sgtes != NULL)
						*sgtes++ = *sgte;
					if (sgids != NULL)
						*sgids++ = sgte->hashlink.sgid;
					nfound++;
				}
			}
		}
		break;
	case S_GRPNAME:
		bucket = hash_value(key->parms.sgname);
		for (idx = SGC->_SGC_sgte_hash[bucket]; idx != -1; idx = sgte->hashlink.fhlink) {
			sgte = link2ptr(idx);
			if (strcmp(key->parms.sgname, sgte->parms.sgname) == 0) {
				if (sgtes != NULL)
					*sgtes++ = *sgte;
				if (sgids != NULL)
					*sgids++ = sgte->hashlink.sgid;
				nfound++;
				break;
			}
		}
		break;
	default:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Parameter error in internal routine]")).str(SGLOCALE));
		SGT->_SGT_error_code = SGESYSTEM;
		return retval;
	}

	if (nfound == 0) {
		SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	retval = nfound;
	return retval;
}

int sg_sgte::mbretrieve(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("scope={1}") % scope).str(SGLOCALE), &retval);
#endif
	retval = sg_rpc::instance(SGT).rtbl(O_RSGTE, scope, key, sgtes, sgids);
	return retval;
}

bool sg_sgte::getsgrps()
{
	sg_sgparms_t parms;
	sg_config& config_mgr = sg_config::instance(SGT);
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	for (sg_config::sgt_iterator iter = config_mgr.sgt_begin(); iter != config_mgr.sgt_end(); ++iter) {
		strcpy(parms.sgname, iter->sgname);
		parms.grpid = iter->grpid;

		for (int i = 0; i < MAXLMIDSINLIST; i++) {
			if (iter->lmid[i][0] == '\0') {
				parms.midlist[i] = BADMID;
			} else {
				if ((parms.midlist[i] = SGC->lmid2mid(iter->lmid[i])) == BADMID) {
					SGT->_SGT_error_code = SGEINVAL;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid logical node name - {1}") % iter->lmid[i]).str(SGLOCALE));
					return retval;
				}
			}
		}
		parms.curmid = iter->curmid;

		if (parms.curmid < 0 || parms.curmid >= MAXLMIDSINLIST) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid host id for process group")).str(SGLOCALE));
			return retval;
		}

		if (create(parms) == BADSGID) {
			if (SGT->_SGT_error_code == 0) {
				SGT->_SGT_error_code = SGESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Duplicate process group entry")).str(SGLOCALE));
			}
			return retval;
		}
	}

	retval = true;
	return retval;
}

sg_sgte_t * sg_sgte::link2ptr(int link)
{
	sgc_ctx *SGC = SGT->SGC();
	return SGC->_SGC_sgtes + link;
}

sg_sgte_t * sg_sgte::sgid2ptr(sgid_t sgid)
{
	sgc_ctx *SGC = SGT->SGC();
	return SGC->_SGC_sgtes + sg_hashlink_t::get_index(sgid);
}

int sg_sgte::ptr2link(const sg_sgte_t *sgte) const
{
	return sgte->hashlink.rlink;
}

sgid_t sg_sgte::ptr2sgid(const sg_sgte_t *sgte) const
{
	return sgte->hashlink.sgid;
}

sg_sgte::sg_sgte()
{
}

sg_sgte::~sg_sgte()
{
}

int sg_sgte::hash_value(const char *sgname) const
{
	sgc_ctx *SGC = SGT->SGC();
	size_t len = ::strlen(sgname);
	return static_cast<int>(boost::hash_range(sgname, sgname + len) % SGC->SGTBKTS());
}

}
}

