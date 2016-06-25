#include "bbm_rte.h"
#include "bbm_rpc.h"

namespace ai
{
namespace sg
{

bool bbm_rte::venter()
{
	sg_svrent_t svrent;
	sg_svcparms_t svcparms;
	sgc_ctx *SGC = SGT->SGC();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	int i;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", &retval);
#endif

	/* 如果不是重启，需要检查一下是否有进程正在运行
	 * 如果是，说明是sgmngr异常导致的重启，只能由sgrecover回复
	 */
	if (!(SGP->_SGP_boot_flags & BF_RESTART)) {
		sg_rte& rte_mgr = sg_rte::instance(SGT);
		for (int i = 0; i < SGC->MAXACCSRS(); i++) {
			sg_rte_t *rte = rte_mgr.link2ptr(i);
			if (rte->pid > 0 && kill(rte->pid, 0) != -1) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: process {1} on slot {2} is alive, sgmngr should be recovered automatically") % rte->pid % rte->slot).str(SGLOCALE));
				return retval;
			}
		}
	}

	/*
	 * must first "get here", since not done until beserver...
	 * which is called after bbattach. bbl must get a queue though
	 * in order to register.
	 */
	SGP->setids(svrent.sparms.svrproc);

	if (!config_mgr.find(svrent))
		return retval;

	if (!SGC->get_here(svrent))
		return retval;

	// get full path name of server
	strcpy(svrent.sparms.rqparms.filename, "sgmngr");
	svrent.sparms.rqparms.paddr = SGC->_SGC_proc.rqaddr;
	svrent.sparms.svrproc = SGC->_SGC_proc;
	svrent.sparms.ctime = time(0);

	BOOST_SCOPE_EXIT((&SGP)(&SGC)(&BBM)(&retval)) {
		if (!retval) {
			BBM->_BBM_amregistered = false;
			SGC->qcleanup(true);
			SGC->_SGC_slot = -1;
			SGP->_SGP_ambbm = false;
			SGC->_SGC_crte = NULL;
		}
	} BOOST_SCOPE_EXIT_END

	strcpy(svcparms.svc_name, BBM_SVC_NAME);
	svcparms.svc_cname[0] = '\0';
	svcparms.svc_index = 0;

	if (!config_mgr.find(svcparms, SGC->_SGC_proc.grpid))
		return retval;

	int datalen = sizeof(long) + sizeof(sg_svrparms_t) + sizeof(sg_svcparms_t);
	size_t rqsz = sg_rpc_rqst_t::need(datalen);

	// set SGP->_SGP_ambbm temporarily so we can creat a message
	SGP->_SGP_ambbm = true;

	/*
	 * Do _tmsmenter so we get our _tmslotptr set before attempting
	 * to do any msg based stuff, i.e. register with the sgclst.
	 * register in reserved slot AT_BBM.
	 */
	SGC->_SGC_slot = SGC->MAXACCSRS() + AT_BBM;
	if (!smenter())
		return retval;

	message_pointer msg = rpc_mgr.create_msg(rqsz);
	if (msg == NULL)
		return retval;

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_RGSTR | VERSION;
	rqst->datalen = datalen;
	rqst->arg1.count = SGRELEASE;

	*reinterpret_cast<int *>(rqst->data()) = SGPROTOCOL;
	memcpy(rqst->data() + sizeof(long), &svrent.sparms, sizeof(sg_svrparms_t));
	memcpy(rqst->data() + sizeof(long) + sizeof(sg_svrparms_t), &svcparms, sizeof(sg_svcparms_t));

	if (!rpc_mgr.send_msg(msg, NULL, SGAWAITRPLY | SGSIGRSTRT))
		return retval;

	sg_msghdr_t& msghdr = *msg;
	sg_rpc_rply_t *rply = msg->rpc_rply();
	if (rply->rtn < 0){
		SGT->_SGT_error_code = rply->error_code;
		if(SGT->_SGT_error_code == SGEOS)
			SGT->_SGT_native_code = msghdr.native_code;

		return retval;
	}

	SGP->_SGP_ssgid = *reinterpret_cast<sgid_t *>(rply->data());

	/*
	 * upload BB but first we must remember the
	 * remember info pertinent to this BB
	 */
	sg_bbmeters_t bbmeters = bbp->bbmeters;
	sg_bbmap_t bbmap = bbp->bbmap;

	memcpy(&bbp->bbmeters, rply->data() + sizeof(sgid_t), rply->datalen - sizeof(sgid_t));

	// restore local info
	bbp->bbmeters.caccsrs = bbmeters.caccsrs;
	bbp->bbmeters.maxaccsrs = bbmeters.maxaccsrs;
	bbp->bbmeters.wkinitiated = bbmeters.wkinitiated;
	bbp->bbmeters.wkcompleted = bbmeters.wkcompleted;

	// store bbmap indexes so that we don't lose them
	bbmap.qte_free = bbp->bbmap.qte_free;
	bbmap.sgte_free = bbp->bbmap.sgte_free;
	bbmap.ste_free = bbp->bbmap.ste_free;
	bbmap.scte_free = bbp->bbmap.scte_free;
	bbp->bbmap = bbmap;

	SGP->_SGP_ste.svrproc() = SGC->_SGC_proc;
	if (ste_mgr.retrieve(S_GRPID, &SGP->_SGP_ste, &SGP->_SGP_ste, &SGP->_SGP_ssgid) < 0)
		return retval;

	/*
	 * Map the MID of the sgclst (sender of the reply) and set the current
	 * master value appropriately.
	 */
	int snode = SGC->midnidx(msghdr.sender.mid);
	for (i = 0; i < MAX_MASTERS; i++) {
		int mnode = SGC->midnidx(SGC->lmid2mid(bbp->bbparms.master[i]));
		if (mnode == snode) {
			bbp->bbparms.current = i;
			break;
		}
	}

	bbp->bbparms.bbrelease = SGPROTOCOL;

	BBM->_BBM_amregistered = true;
	SGP->_SGP_amserver = true;
	SGP->_SGP_ambbm = true;

	retval = true;
	return retval;
}

void bbm_rte::vleave(sg_rte_t *rte)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", NULL);
#endif

	bbm_rpc::instance(SGT).resign();		/* unregister with sgclst */
	smleave(rte);	/* remove local registry slot */
}

bbm_rte::bbm_rte()
{
	BBM = bbm_ctx::instance();
}

bbm_rte::~bbm_rte()
{
}

}
}

