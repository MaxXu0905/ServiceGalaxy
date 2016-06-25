#include "sg_internal.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

sg_nte& sg_nte::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_nte *>(SGT->_SGT_mgr_array[NTE_MANAGER]);
}

int sg_nte::create(sg_netent_t& parms, int flags)
{
	return (this->*SGT->SGC()->_SGC_bbasw->cnte)(parms, flags);
}

int sg_nte::smcreate(sg_netent_t& parms, int flags)
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

	for (int pidx = 0; pidx < bbp->bbparms.maxpes && !(SGC->_SGC_ptes[pidx].flags & NP_UNUSED); pidx++) {
		if (SGC->_SGC_ptes[pidx].flags & NP_INVALID)
			continue;

		if (strcmp(parms.lmid, SGC->_SGC_ptes[pidx].lname) == 0) {
			SGC->_SGC_ptes[pidx].flags |= NP_NETWORK;
			retval = 0;
			return retval;
		}
	}

	return retval;
}

int sg_nte::mbcreate(sg_netent_t& parms, int flags)
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

		message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_netent_t)));
		if (msg == NULL) {
			// error_code already set
			return retval;
		}

		sg_rpc_rqst_t *rqst = msg->rpc_rqst();
		rqst->opcode = O_CNTE | VERSION;
		rqst->datalen = sizeof(sg_netent_t);
		memcpy(rqst->data(), &parms, sizeof(sg_netent_t));

		retval = rpc_mgr.asnd(msg);
		return retval;
	}

	retval = 0;
	return retval;
}

sg_nte::sg_nte()
{
}

sg_nte::~sg_nte()
{
}

}
}

