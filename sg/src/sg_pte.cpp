#include "sg_internal.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

sg_pte& sg_pte::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_pte *>(SGT->_SGT_mgr_array[PTE_MANAGER]);
}

int sg_pte::create(sg_mchent_t& parms, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->cpte)(parms, flags);
}

int sg_pte::smcreate(sg_mchent_t& parms, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	if (bbp == NULL) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Not attached to a Bulletin Board")).str(SGLOCALE));
		return retval;
	}

	if (SGC->_SGC_ptes == NULL || SGC->_SGC_ntes == NULL
		|| bbp->bbparms.maxpes <= 0 || bbp->bbparms.maxnodes <= 0) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Internal system error")).str(SGLOCALE));
		return retval;
	}

	// 支持FAKE MP
	string pnode = parms.pmid;
	string::size_type pos = pnode.find_last_of('.');
	if (pos != string::npos && pos + 1 < pnode.size()
		&& pnode[pos + 1] >= '0' && pnode[pos + 1] <= '9')
		pnode.resize(pos);

	bool found = false;
	int pidx;
	int nidx;
	for (nidx = 0; !(SGC->_SGC_ntes[nidx].flags & NT_UNUSED) && nidx < bbp->bbparms.maxnodes; nidx++) {
		if (pnode == SGC->_SGC_ntes[nidx].pnode) {
			found = true;
			break;
		}
	}

	// 如果给定的物理主机还没有创建NTE
	if (!found) {
		if (nidx == bbp->bbparms.maxnodes) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: No space in Bulletin Board for NTE")).str(SGLOCALE));
			return retval;
		}

		if (nidx + 1 < bbp->bbparms.maxnodes) {
			// 不是最后一个节点
			SGC->_SGC_ntes[nidx + 1].flags = NT_UNUSED;
		}

		SGC->_SGC_ntes[nidx].flags &= ~NT_UNUSED;
		SGC->_SGC_ntes[nidx].link = -1;
		strcpy(SGC->_SGC_ntes[nidx].pnode, pnode.c_str());
	}

	// 检查是否有重复
	for (pidx = SGC->_SGC_ntes[nidx].link; pidx != -1; pidx = SGC->_SGC_ptes[pidx].link) {
		if (SGC->_SGC_ptes[pidx].flags & NP_INVALID)
			continue;

		if (strcmp(parms.pmid, SGC->_SGC_ptes[pidx].pname) == 0
			|| strcmp(parms.lmid, SGC->_SGC_ptes[pidx].lname) == 0)
			break;
	}

	// 有重复节点
	if (pidx != -1) {
		retval = 0;
		return retval;
	}

	for (pidx = 0; !(SGC->_SGC_ptes[pidx].flags & NP_UNUSED) && pidx < bbp->bbparms.maxpes; pidx++)
		continue;

	if (pidx >= bbp->bbparms.maxpes) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No space in Bulletin Board for PTE")).str(SGLOCALE));
		return retval;
	}

	if (pidx + 1 < bbp->bbparms.maxpes) {
		// 不是最后一个节点
		SGC->_SGC_ptes[pidx + 1].flags = NP_UNUSED;
	}

	// 初始化节点
	sg_pte_t& pte = SGC->_SGC_ptes[pidx];
	sg_nte_t& nte = SGC->_SGC_ntes[nidx];

	pte.link = nte.link;
	nte.link = pidx;
	pte.nlink = nidx;
	pte.mid = SGC->idx2mid(nidx, pidx);
	pte.maxaccsrs = parms.maxaccsrs;
	strcpy(pte.pname, parms.pmid);
	strcpy(pte.lname, parms.lmid);
	pte.perm = parms.perm;
	pte.uid = parms.uid;
	pte.gid = parms.gid;
	pte.netload = parms.netload;
	pte.maxconns = parms.maxconn;

	pte.flags = 0;
	if (parms.flags & MA_INVALID)
		pte.flags |= NP_INVALID;

	retval = 0;
	return retval;
}

int sg_pte::mbcreate(sg_mchent_t& parms, int flags)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	if (flags & U_LOCAL) {
		if (smcreate(parms, flags) < 0)
			return retval;
	}

	if (flags & U_GLOBAL) {
		sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
		message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_mchent_t)));
		if (msg == NULL) {
			// error_code already set
			return retval;
		}

		sg_rpc_rqst_t *rqst = msg->rpc_rqst();
		rqst->opcode = O_CPTE | VERSION;
		rqst->datalen = sizeof(sg_mchent_t);
		memcpy(rqst->data(), &parms, sizeof(sg_mchent_t));

		retval = rpc_mgr.asnd(msg);
		return retval;
	}

	retval = 0;
	return retval;
}

sg_pte::sg_pte()
{
}

sg_pte::~sg_pte()
{
}

}
}

