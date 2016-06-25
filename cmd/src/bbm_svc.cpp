#include "bbm_svc.h"
#include "bbm_rpc.h"

namespace ai
{
namespace sg
{

bbm_svc::bbm_svc()
{
	BBM = bbm_ctx::instance();
	can_migrate = false;
}

bbm_svc::~bbm_svc()
{
}

svc_fini_t bbm_svc::svc(message_pointer& svcinfo)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	bbm_rpc& brpc_mgr = bbm_rpc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	int rgst_mid = -1;
	registry_t rgst_regent;

	sg_rpc_rqst_t *rqst = svcinfo->rpc_rqst();
	sg_msghdr_t& msghdr = *svcinfo;
	int s_elen;
	int t_elen;
	int maxent;
	int cnt;
	int ret = SGSUCCESS;
	bool have_bbm = false;
	boost::unordered_map<int, registry_t>::iterator iter;
	bool forward = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("op_name={1}, opcode={2}") % msghdr.svc_name % (rqst->opcode & ~VERSMASK)).str(SGLOCALE), NULL);
#endif

	BBM->_BBM_inmasterbb = true;
	if (SGP->_SGP_amdbbm)
		forward = true;

	message_pointer rqst_msg = svcinfo->clone();
	if (rqst_msg == NULL)
		return prot_fail(svcinfo, rqst_msg, NULL, SGT->_SGT_error_code, SGFAIL);

	rpc_mgr.init_rqst_hdr(rqst_msg);

	message_pointer rply_msg = rpc_mgr.create_msg(sg_rpc_rply_t::need(MAXRPC));
	sg_rpc_rply_t *rply = rply_msg->rpc_rply();
	rply->datalen = 0;

	if ((rqst->opcode & VERSMASK) != VERSION)
		return prot_fail(svcinfo, rply_msg, rply, SGESYSTEM, SGFAIL);

	int opcode = rqst->opcode & ~VERSMASK;
	if (SGP->_SGP_amdbbm) {
		if (bbp->bbmeters.status & SHUTDOWN)
			return prot_fail(svcinfo, rply_msg, rply, SGENOCLST, SGFAIL);

		iter = BBM->_BBM_regmap.find(msghdr.sender.mid);
		if (iter != BBM->_BBM_regmap.end()) {
			registry_t& regent = iter->second;

			if (!can_migrate) {
				have_bbm = !(regent.status & PARTITIONED);
				if (!have_bbm) {
					GPP->write_log(__FILE__, __LINE__, (_("INFO: Message from partitioned sgmngr on node {1}")
						% SGC->pmtolm(regent.proc.mid)).str(SGLOCALE));
				}
			} else {
				have_bbm = true;
			}
		} else { // only admins come here
			have_bbm = false;
			for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
				registry_t& regent = iter->second;

				if (regent.proc.pid == msghdr.sender.pid) {
					have_bbm = true;
					break;
				}
			}
		}
	}

	// 初始化错误代码，有些逻辑需要依赖该值打印错误信息
	SGT->_SGT_error_code = 0;

	switch (opcode) {
	case O_CPTE:
		{
			sg_mchent_t *mchent = reinterpret_cast<sg_mchent_t *>(rqst->data());

			if ((rply->rtn = sg_pte::instance(SGT).create(*mchent, U_GLOBAL)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_CNTE:
		{
			sg_netent_t *netent = reinterpret_cast<sg_netent_t *>(rqst->data());

			if ((rply->rtn = sg_nte::instance(SGT).create(*netent, U_GLOBAL)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_CQTE:
		{
			sgid_t sgid;
			size_t datalen = sizeof(sgid_t);

			if (SGP->_SGP_amdbbm && !have_bbm) {
				SGT->_SGT_error_code = SGENOMNGR;
				sgid = BADSGID;
			} else {
				sg_queparms_t *queparms = reinterpret_cast<sg_queparms_t *>(rqst->data());
				sgid = sg_qte::instance(SGT).create(*queparms);
			}

			if (sgid == BADSGID) {
				set_error(rply, SGT->_SGT_error_code);
				break;
			}

			rply->rtn = 1;
			if (SGP->_SGP_amdbbm) {
				rply->datalen = datalen;
				*reinterpret_cast<sgid_t *>(rply->data()) = sgid;
			}
		}
		break;
	case O_DQTE:
		{
			sg_qte& qte_mgr = sg_qte::instance(SGT);
			sgid_t *sgids = reinterpret_cast<sgid_t *>(rqst->data());
			int cnt = rqst->datalen / sizeof(sgid_t);
			if ((rply->rtn = qte_mgr.destroy(sgids, cnt)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_UQTE:
		{
			sg_qte& qte_mgr = sg_qte::instance(SGT);
			sg_qte_t *qtes = reinterpret_cast<sg_qte_t *>(rqst->data());
			int cnt = rqst->datalen / sizeof(sg_qte_t);
			if ((rply->rtn = qte_mgr.update(qtes, cnt, rqst->arg1.flag)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_CSGTE:
		{
			sg_sgparms_t *sgparms = reinterpret_cast<sg_sgparms_t *>(rqst->data());

			sgid_t sgid = sg_sgte::instance(SGT).create(*sgparms);
			if (sgid == BADSGID) {
				set_error(rply, SGT->_SGT_error_code);
				break;
			}

			rply->rtn = 1;
			size_t datalen = sizeof(sgid_t);
			rply->datalen = datalen;
			*reinterpret_cast<sgid_t *>(rply->data()) = sgid;
		}
		break;
	case O_DSGTE:
		{
			sgid_t *sgids = reinterpret_cast<sgid_t *>(rqst->data());
			int cnt = rqst->datalen / sizeof(sgid_t);
			if ((rply->rtn = sg_sgte::instance(SGT).destroy(sgids, cnt)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_USGTE:
		{
			sg_sgte_t *sgtes = reinterpret_cast<sg_sgte_t *>(rqst->data());
			int cnt = rqst->datalen / sizeof(sg_sgte_t);
			if ((rply->rtn = sg_sgte::instance(SGT).update(sgtes, cnt, rqst->arg1.flag)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_CSTE:
		{
			sgid_t sgid;
			size_t datalen = sizeof(sgid_t);

			if (SGP->_SGP_amdbbm && !have_bbm) {
				SGT->_SGT_error_code = SGENOMNGR;
				sgid = BADSGID;
			} else {
				sg_ste& ste_mgr = sg_ste::instance(SGT);
				sg_svrparms_t *svrparms = reinterpret_cast<sg_svrparms_t *>(rqst->data());
				sgid = ste_mgr.create(rqst->arg1.sgid, *svrparms);
			}

			if (sgid == BADSGID) {
				set_error(rply, SGT->_SGT_error_code);
				break;
			}

			rply->rtn = 1;
			if (SGP->_SGP_amdbbm) {
				rply->datalen = datalen;
				*reinterpret_cast<sgid_t *>(rply->data()) = sgid;
			} else {
				sg_qte& qte_mgr = sg_qte::instance(SGT);
				qte_mgr.syncq();
			}
		}
		break;
	case O_DSTE:
		{
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sgid_t *sgids = reinterpret_cast<sgid_t *>(rqst->data());
			int cnt = rqst->datalen / sizeof(sgid_t);
			if ((rply->rtn = ste_mgr.destroy(sgids, cnt)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_RMSTE:
		{
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sgid_t sgid = *reinterpret_cast<sgid_t *>(rqst->data());
			if ((rply->rtn = ste_mgr.remove(sgid)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_USTE:
		{
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_ste_t *stes = reinterpret_cast<sg_ste_t *>(rqst->data());
			int cnt = rqst->datalen / sizeof(sg_ste_t);
			if ((rply->rtn = ste_mgr.update(stes, cnt, rqst->arg1.flag)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_CSCTE:
	case O_ASCTE:
		{
			sgid_t sgid;
			size_t datalen = sizeof(sgid_t);
			sg_scte& scte_mgr = sg_scte::instance(SGT);
			sg_svcparms_t *svcparms = reinterpret_cast<sg_svcparms_t *>(rqst->data());

			if (SGP->_SGP_amdbbm && !have_bbm) {
				SGT->_SGT_error_code = SGENOMNGR;
				sgid = BADSGID;
			} else if (opcode == O_ASCTE) {
				sgid = scte_mgr.add(rqst->arg1.sgid, *svcparms);
			} else {
				sgid = scte_mgr.create(rqst->arg1.sgid, *svcparms, rqst->arg1.flag);
			}

			if (sgid == BADSGID) {
				set_error(rply, SGT->_SGT_error_code);
				break;
			}

			if (opcode == O_ASCTE) {
				if (!rpc_mgr.set_stat(rqst->arg1.sgid, ~SUSPENDED, STATUS_OP_AND)) {
					if (SGP->_SGP_amdbbm) {
						// 删除服务节点
						sg_scte_t vkey;
						int cnt = rqst->datalen / sizeof(sgid_t);
						boost::shared_array<sgid_t> auto_sgids(new sgid_t[cnt + 1]);
						sgid_t *tsgids = auto_sgids.get();
						strcpy(vkey.parms.svc_name, svcparms->svc_name);
						vkey.grpid() = msghdr.sender.grpid;
						vkey.svrid() = msghdr.sender.svrid;

						scoped_bblock bblock(SGT);
						cnt = scte_mgr.retrieve(S_GRPID, &vkey, &vkey, tsgids);
						scte_mgr.destroy(tsgids, cnt);
						cnt = scte_mgr.retrieve(S_QUEUE, &vkey, NULL, tsgids);
						scte_mgr.destroy(tsgids, cnt);
						set_error(rply, SGT->_SGT_error_code);
						break;
					}
				}
			}

			rply->rtn = 1;
			if (SGP->_SGP_amdbbm) {
				rply->datalen = datalen;
				*reinterpret_cast<sgid_t *>(rply->data()) = sgid;
			}
		}
		break;
	case O_DSCTE:
		{
			sg_scte& scte_mgr = sg_scte::instance(SGT);
			sgid_t *sgids = reinterpret_cast<sgid_t *>(rqst->data());
			int cnt = rqst->datalen / sizeof(sgid_t);
			if ((rply->rtn = scte_mgr.destroy(sgids, cnt)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_USCTE:
		{
			sg_scte& scte_mgr = sg_scte::instance(SGT);
			sg_scte_t *sctes = reinterpret_cast<sg_scte_t *>(rqst->data());
			int cnt = rqst->datalen / sizeof(sg_scte_t);
			if ((rply->rtn = scte_mgr.update(sctes, cnt, rqst->arg1.flag)) < 0)
				set_error(rply, SGT->_SGT_error_code);
		}
		break;
	case O_OGSVCS:
		{
			sgid_t sgid;
			size_t datalen = sizeof(sgid_t);
			rply_msg->reserve(sg_rpc_rply_t::need(datalen));
			rply = rply_msg->rpc_rply();

			sg_svrparms_t *svrparms = reinterpret_cast<sg_svrparms_t *>(rqst->data());
			sg_svcparms_t *svcparms = reinterpret_cast<sg_svcparms_t *>(rqst->data() + sizeof(sg_svrparms_t));
			int cnt = (rqst->datalen - sizeof(sg_svrparms_t)) / sizeof(sg_svcparms_t);

			if (SGP->_SGP_amdbbm && !have_bbm) {
				SGT->_SGT_error_code = SGENOMNGR;
				sgid = BADSGID;
			} else {
				sg_ste& ste_mgr = sg_ste::instance(SGT);
				sgid = ste_mgr.offer(*svrparms, svcparms, cnt);
			}

			if (sgid == BADSGID) {
				set_error(rply, SGT->_SGT_error_code);
				break;
			}

			rply->rtn = 1;
			if (SGP->_SGP_amdbbm) {
				rply->datalen = datalen;
				*reinterpret_cast<sgid_t *>(rply->data()) = sgid;
			} else {
				sg_qte& qte_mgr = sg_qte::instance(SGT);
				qte_mgr.syncq();
			}
		}
		break;
	case O_RQTE:
		s_elen = sizeof(sg_qte_t);
		maxent = SGC->MAXQUES();
		goto BEGIN_LABEL;
	case O_RSGTE:
		s_elen = sizeof(sg_sgte_t);
		maxent = SGC->MAXSGT();
		goto BEGIN_LABEL;
	case O_RSTE:
		s_elen = sizeof(sg_ste_t);
		maxent = SGC->MAXSVRS();
		goto BEGIN_LABEL;
	case O_RSCTE:
		s_elen = sizeof(sg_scte_t);
		maxent = SGC->MAXSVCS();
		goto BEGIN_LABEL;

BEGIN_LABEL:
		{
			char *key = NULL;
			char *s = NULL;
			sgid_t *sgids = NULL;
			char *dp = NULL;
			int tabsz = 0;
			int sgidsz = 0;
			bool shift = false;
			size_t rtnsz = 0;

			forward = false;
			t_elen = sizeof(sgid_t);

			switch (rqst->arg1.scope) {
			case S_SVRID:
			case S_GRPID:
			case S_GROUP:
			case S_QUEUE:
			case S_MACHINE:
			case S_ANY:
			case S_GRPNAME:
				key = rqst->data();

				// make buffer large enough
				if (rqst->arg2.tabents && rqst->arg3.sgids)
					rtnsz = maxent * (s_elen + t_elen);
				else if (rqst->arg2.tabents)
					rtnsz = maxent * s_elen;
				else if (rqst->arg3.sgids)
					rtnsz = maxent * t_elen;

				rply_msg->reserve(sg_rpc_rply_t::need(rtnsz));
				rply = rply_msg->rpc_rply();
				rply->datalen = rtnsz;

				// reply to be copied into msg buffer directly
				if (rqst->arg2.tabents)
					s = rply->data();

				if (rqst->arg3.sgids){
					if (s){ // leave enough room for tbl ents
						sgids = reinterpret_cast<sgid_t *>(s + (maxent * s_elen));
						shift = true;
					} else {
						sgids = reinterpret_cast<sgid_t *>(rply->data());
					}
				}
				break;
			case S_BYSGIDS:
				if (rqst->arg3.sgids == 1) {
					rply_msg->reserve(sg_rpc_rply_t::need(s_elen));
					rply = rply_msg->rpc_rply();
					rply->datalen = s_elen;
					key = rqst->data();
				} else {
					rply_msg->reserve(sg_rpc_rply_t::need((rqst->arg3.sgids - 1) * s_elen));
					rply = rply_msg->rpc_rply();
					rply->datalen = (rqst->arg3.sgids - 1) * s_elen;
				}
				s = rply->data();
				sgids = reinterpret_cast<sgid_t *>(rqst->data());
				break;
			case S_BYRLINK:
				if (rqst->arg3.sgids > 0) { // key is present
					rply_msg->reserve(sg_rpc_rply_t::need(s_elen));
					rply = rply_msg->rpc_rply();
					rply->datalen = s_elen;
					key = rqst->data();
					s = rply->data();
				} else if (rqst->arg2.tabents > 1) {
					/*
					 * tbl entries present
					 * note: reply must be moved to msg
					 */
					rply_msg->reserve(sg_rpc_rply_t::need((rqst->arg2.tabents - 1) * s_elen));
					rply = rply_msg->rpc_rply();
					rply->datalen = (rqst->arg2.tabents - 1) * s_elen;
					s = rqst->data();
				} else {
					set_error(rply, SGEINVAL);
				}
				break;
			}

			switch (opcode) {
			case O_RQTE:
				{
					sg_qte& qte_mgr = sg_qte::instance(SGT);
					rply->rtn = qte_mgr.retrieve(rqst->arg1.scope, reinterpret_cast<sg_qte_t *>(key),
						reinterpret_cast<sg_qte_t *>(s), sgids);
				}
				break;
			case O_RSGTE:
				{
					sg_sgte& sgte_mgr = sg_sgte::instance(SGT);
					rply->rtn = sgte_mgr.retrieve(rqst->arg1.scope, reinterpret_cast<sg_sgte_t *>(key),
						reinterpret_cast<sg_sgte_t *>(s), sgids);
				}
				break;
			case O_RSTE:
				{
					sg_ste& ste_mgr = sg_ste::instance(SGT);
					rply->rtn = ste_mgr.retrieve(rqst->arg1.scope, reinterpret_cast<sg_ste_t *>(key),
						reinterpret_cast<sg_ste_t *>(s), sgids);
				}
				break;
			case O_RSCTE:
				{
					sg_scte& scte_mgr = sg_scte::instance(SGT);
					rply->rtn = scte_mgr.retrieve(rqst->arg1.scope, reinterpret_cast<sg_scte_t *>(key),
						reinterpret_cast<sg_scte_t *>(s), sgids);
				}
				break;
			}

			if (rply->rtn < 0) {
				set_error(rply, SGT->_SGT_error_code);
				break;
			}

			// say if table entries and/or sgids are returned
			rply->flags = 0;
			if (s) {
				tabsz = rply->rtn * s_elen;
				rply->flags |= SGRPTABENTS;
			}
			if (sgids && rqst->arg1.scope != S_BYSGIDS) {
				sgidsz = rply->rtn * t_elen;
				rply->flags |= SGRPSGIDS;
			}

			if (tabsz || sgidsz) { // data to send back
				rply->datalen = tabsz + sgidsz;

				if (shift) { // remove gap between two tables
					dp = s + tabsz;
					memcpy(dp, sgids, sgidsz);
				}
				// copy reply from request buffer to msg
				if(rqst->arg1.scope == S_BYRLINK && !rqst->arg3.sgids)
					memcpy(rply->data(), s, tabsz);
			}
		}
		break;
	case O_USTAT:
	case O_VUSTAT:
		{
			sgid_t *sgids = reinterpret_cast<sgid_t *>(rqst->data());
			int cnt = rqst->datalen / sizeof(sgid_t);
			if ((rply->rtn = rpc_mgr.ustat(sgids, cnt, rqst->arg2.status, rqst->arg1.flag)) != cnt)
				set_error(rply, SGT->_SGT_error_code);

			if (opcode == O_VUSTAT)
				return svc_fini(svcinfo, rply_msg, rply->rtn);

			if (cnt == 1 && (*sgids & SGID_TMASK) == SGID_SVRT) {
				sg_ste& ste_mgr = sg_ste::instance(SGT);
				sg_ste_t *ste = ste_mgr.sgid2ptr(*sgids);
				if (ste->svrproc().is_gws()) {
					forward = false;
					rpc_mgr.laninc();
				} else if (ste->svrproc().is_bbm()) {
					iter = BBM->_BBM_regmap.find(ste->mid());
					if (iter != BBM->_BBM_regmap.end()) {
						registry_t& regent = iter->second;
						if (rqst->arg2.status == PSUSPEND)
							regent.status = PARTITIONED;
						else if (rqst->arg2.status == ~PSUSPEND)
							regent.status = 0;
					}
				}
			}
		}
		break;
	case O_BBCLEAN:
		{
			boost::unordered_map<int, registry_t>::iterator iter;

			if (SGP->_SGP_ambbm) {
				BBM->_BBM_cleantime = bbp->bbparms.sanity_scan;
				return svc_fini(svcinfo, rply_msg, SGFAIL);
			}

			GPP->write_log((_("INFO: Forcing check of all sgmngrs")).str(SGLOCALE));

			// If this is set, then we won't log a MACHINE SLOW event.
			BBM->_BBM_force_check = true;

			// Check status of partitioned BBMs
			for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
				if (iter->second.status & PARTITIONED)
					unpartition(iter->second);
			}

			// Reset status so sanity check below will contact all BBMs
			for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter)
				iter->second.tardy++; // Force check

			// Schedule sanity check of all BBMs
			BBM->_BBM_querytime = bbp->bbparms.bbm_query;

			return svc_fini(svcinfo, rply_msg, SGFAIL);
		}
	case O_SNDSTAT:
		{
			int status;

			if((status = getstat(SGP->_SGP_ssgid)) == -1){
				// bad shape: can't find myself
				set_error(rply, SGT->_SGT_error_code);
				break;
			}

			rply->rtn = (status & (SUSPENDED | PSUSPEND)) ? -1 : 1;
			rply->datalen = sizeof(status);
			*reinterpret_cast<int *>(rply->data()) = status;
			/* when sgclst asks, shedule clean (even if sane)
			 * if we just did bbclean, should wait one scan_unit,
			 * otherwise, sgclear/sgrecover may be failed to start
			 */
			if (BBM->_BBM_cleantime == 0)
				BBM->_BBM_cleantime = bbp->bbparms.sanity_scan - 1;
			else
				BBM->_BBM_cleantime = bbp->bbparms.sanity_scan;
		}
		break;
	case O_LANINC:
		{
			if (rpc_mgr.laninc() < 0)
				set_error(rply, SGT->_SGT_error_code);
			else
				rply->rtn = 1;
		}
		break;
	case O_RGSTR:
		{
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_ste_t ste;
			sgid_t sgid;

			int nprotocol = *reinterpret_cast<int *>(rqst->data());
			sg_svrparms_t *svrparms = reinterpret_cast<sg_svrparms_t *>(rqst->data() + sizeof(long));
			int bbm_mid = svrparms->svrproc.mid;

			if (SGP->_SGP_amdbbm) {
				if (have_bbm) {
					set_error(rply, SGT->_SGT_error_code);
					break;
				}

				SGC->_SGC_ntes[SGC->midnidx(bbm_mid)].nprotocol = nprotocol;
				if (nprotocol != SGPROTOCOL) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Older release sites cannot join application in non-interoperability mode")).str(SGLOCALE));
					set_error(rply, SGESYSTEM);
					break;
				}
			} else {
				SGC->_SGC_ntes[SGC->midnidx(bbm_mid)].nprotocol = nprotocol;
			}

			// advertise sgmngr
			sg_svcparms_t *svcparms = reinterpret_cast<sg_svcparms_t *>(svrparms + 1);
			if ((sgid = ste_mgr.offer(*svrparms, svcparms, 1)) == BADSGID) {
				if (SGT->_SGT_error_code == SGEDUPENT)
					SGT->_SGT_error_code = SGENEEDPCLEAN;
				set_error(rply, SGESYSTEM);
				break;
			}

			// 如果BBM正在重启，offer()可能会更改我们的queue
			if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, &sgid) != 1) {
				set_error(rply, SGESYSTEM);
				break;
			}

			ste.global.srelease = rqst->arg1.count;

			if (ste_mgr.update(&ste, 1, U_VIEW) != 1) {
				set_error(rply, SGESYSTEM);
				break;
			}

			rply->rtn = 1;

			if ((cnt = ste_mgr.retrieve(S_SVRID, &ste, NULL, NULL)) > 0) {
				cnt -= 1;	// 去掉DBBM
				if (cnt > bbp->bbmeters.maxmachines)
					bbp->bbmeters.maxmachines = cnt;
			}

			if (SGP->_SGP_amdbbm) {
				size_t datalen = sizeof(sgid_t) + BBM->_BBM_pbbsize;
				rply_msg->reserve(sg_rpc_rply_t::need(datalen));
				rply = rply_msg->rpc_rply();
				memcpy(rply->data(), &sgid, sizeof(sgid_t));

				// 删除已经注册的节点
				iter = BBM->_BBM_regmap.find(msghdr.sender.mid);
				if (iter != BBM->_BBM_regmap.end())
					BBM->_BBM_regmap.erase(iter);

				// save entry, and add it later
				rgst_regent.proc = ste.svrproc();
				rgst_regent.sgid = sgid;
				rgst_regent.status = 0;
				rgst_regent.timestamp = time(0);
				rgst_regent.tardy = 0;
				rgst_regent.release = rqst->arg1.count;
				rgst_regent.protocol = nprotocol;
				rgst_regent.bbvers = 0;
				rgst_regent.sgvers = 0;
				rgst_regent.usrcnt = 0;
				rgst_mid = ste.mid();

				/*
				 * Copy BB into reply and remember sgmngr's sgid
				 * place at beginning of BB.
				 */
				memcpy(rply->data() + sizeof(sgid_t), &bbp->bbmeters, BBM->_BBM_pbbsize);
				rply->datalen = sizeof(sgid_t) + BBM->_BBM_pbbsize;
				rply->flags = 0;
			}
		}
		break;
	case O_URGSTR:
		{
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_ste_t ste;
			sgid_t sgid;

			if (SGP->_SGP_amdbbm) {
				for (iter = BBM->_BBM_regmap.begin(); iter != BBM->_BBM_regmap.end(); ++iter) {
					if (iter->second.proc == msghdr.sender)
						break;
				}
				if (iter == BBM->_BBM_regmap.end()) {
					set_error(rply, SGESYSTEM);
					break;
				}
				BBM->_BBM_regmap.erase(iter);
			}

			if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, &rqst->arg1.sgid) == 1) {
				if (brpc_mgr.pairedgw(ste.svrproc(), NULL, &sgid)) {
					if (ste_mgr.remove(sgid) < 0) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot remove sggws on node {1}")
							% SGC->pmtolm(ste.mid())).str(SGLOCALE));
					}
					rpc_mgr.laninc();
				}
			}

			if ((rply->rtn = ste_mgr.remove(rqst->arg1.sgid)) < 0) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot unregister sgmngr on node {1}")
					% SGC->pmtolm(msghdr.sender.mid)).str(SGLOCALE));
				set_error(rply, SGT->_SGT_error_code);
				break;
			}

			if (SGP->_SGP_ambbm) // don't send reply
				return svc_fini(svcinfo, rply_msg, rply->rtn);
		}
		break;
	case O_ARGSTR:
		{
			sg_ste_t ste;
			sg_proc_t *up;
			sg_rte_t *rte;
			sg_rte& rte_mgr = sg_rte::instance(SGT);

			if (SGP->_SGP_amdbbm) {
				iter = BBM->_BBM_regmap.find(msghdr.sender.mid);
				if (iter == BBM->_BBM_regmap.end()) {
					set_error(rply, SGENOENT);
					break;
				}

				int nodes = SGC->MAXNODES();
				size_t datalen = nodes * sizeof(sg_nte_t);
				forward = false;
				rply_msg->reserve(sg_rpc_rply_t::need(datalen));
				rply = rply_msg->rpc_rply();
				rply->rtn = 1;
				rply->num = nodes;
				rply->datalen = datalen;
				memcpy(rply->data(), SGC->_SGC_ntes, datalen);
			}

			regstr_t *myptr = reinterpret_cast<regstr_t *>(rqst->data());
			if (myptr->orig < MAXADMIN) {
				if (SGP->_SGP_amdbbm || msghdr.sender.grpid != CLST_PGID) {
					up = &msghdr.sender;
					myptr->mark = 1;
					myptr->myexitproc = msghdr.sender;
				} else {
					up = &myptr->myexitproc;
				}

				registry_t& regent = BBM->_BBM_regadm[myptr->orig];
				if (!myptr->orig) {
					regent.proc = *up;
				} else {
					if (!proc_mgr.alive(regent.proc)) {
						regent.proc = *up;
					} else {
						set_error(rply, SGEDUPENT);
						break;
					}
				}

				regent.timestamp = time(0);
				if (!SGP->_SGP_amdbbm && msghdr.sender.grpid == CLST_PGID) {
					int slot = SGC->MAXACCSRS() + myptr->orig;
					rte = rte_mgr.link2ptr(slot);
					memset(rte, '\0', sizeof(sg_rte_t));
					rte->slot = slot;
					strcpy(rte->usrname, SGC->admuname());
					strcpy(rte->cltname, SGC->admcnm(myptr->orig));
					if (regent.timestamp == rte->rtimestamp)
						regent.timestamp++;
					rte->rt_timestamp = regent.timestamp;
					rte->rtimestamp = regent.timestamp;
					rte->pid = regent.proc.pid;
					rte->rt_release = SGPROTOCOL;
					rte->rflags = ADMIN;
					switch (myptr->orig) {
					case AT_ADMIN:
					case AT_SHUTDOWN:
						rte->rflags |= CLIENT;
						break;
					}

					rte->qaddr = regent.proc.rqaddr;
				}

				if (SGP->_SGP_amdbbm) {
					rpc_mgr.send_msg(svcinfo, &regent.proc, SGNOREPLY | SGNOBLOCK | SGSIGRSTRT);
				}
			} else {
				set_error(rply, SGEINVAL);
				break;
			}
		}
		break;
	case O_AURGSTR:
		{
			sg_proc_t *up;
			sg_rte_t *rte;
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_rte& rte_mgr = sg_rte::instance(SGT);

			forward = false;
			rply->rtn = 1;

			regstr_t *myptr = reinterpret_cast<regstr_t *>(rqst->data());
			if (myptr->orig < MAXADMIN) {
				if (SGP->_SGP_amdbbm || msghdr.sender.grpid != CLST_PGID) {
					up = &msghdr.sender;
					myptr->mark = 1;
					myptr->myexitproc = msghdr.sender;
				} else {
					up = &myptr->myexitproc;
				}

				if (myptr->orig == AT_SHUTDOWN)
					BBM->_BBM_cleantime = bbp->bbparms.sanity_scan;

				registry_t& regent = BBM->_BBM_regadm[myptr->orig];
				if (regent.proc == *up) {
					regent.proc.pid = 0;
				} else {
					set_error(rply, SGEINVAL);
					break;
				}
			} else {
				set_error(rply, SGEINVAL);
				break;
			}

			if (SGP->_SGP_ambbm) {
				/*
				 * When shutdown unregisters, restore the old
				 * sgclst STE if we have one since the TEMP one is
				 * gone now.
				 */
				if (BBM->_BBM_oldflag && myptr->orig == AT_SHUTDOWN) {
					if (ste_mgr.update(&BBM->_BBM_olddbbm, 1, U_LOCAL) != 1)
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not restore sgclst entry after temp change for shutdown")).str(SGLOCALE));
					brpc_mgr.updatemaster(&BBM->_BBM_olddbbm);
					BBM->_BBM_oldflag = false;
				}
				if (msghdr.sender.grpid == CLST_PGID) {
					int slot = SGC->MAXACCSRS() + myptr->orig;
					rte = rte_mgr.link2ptr(slot);
					memset(rte, '\0', sizeof(sg_rte_t));
				}
				return svc_fini(svcinfo, rply_msg, rply->rtn);
			} else if (SGP->_SGP_amdbbm) {
				iter = BBM->_BBM_regmap.find(msghdr.sender.mid);
				if (iter != BBM->_BBM_regmap.end())
					rpc_mgr.send_msg(svcinfo, &iter->second.proc, SGNOREPLY | SGNOBLOCK | SGSIGRSTRT);
			}
		}
		break;
	case O_BBMS:
		break;
	case O_CDBBM:
		{
			sg_ste_t dbbmste;
			sg_ste_t bbm;
			sg_qte_t dbbmqte;
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_qte& qte_mgr = sg_qte::instance(SGT);

			if (SGP->_SGP_amdbbm)
				return svc_fini(svcinfo, rply_msg, SGFAIL);

			// Retrieve the sgclst STE and see if it's still alive
			dbbmste.svrid() = MNGR_PRCID;
			dbbmste.grpid() = CLST_PGID;
			if (ste_mgr.retrieve(S_GRPID, &dbbmste, &dbbmste, NULL) != 1)
				return svc_fini(svcinfo, rply_msg, SGFAIL);

			/* If the sgmngr on the original master is in partitioned state, we don't
			 * have to check the aliveness of sgclst on that node.
			 */
			bbm.svrid() = MNGR_PRCID;
			bbm.grpid() = SGC->mid2grp(SGC->lmid2mid(bbp->bbparms.master[bbp->bbparms.current]));
			if (ste_mgr.retrieve(S_GRPID, &bbm, &bbm, NULL) != 1)
				return svc_fini(svcinfo, rply_msg, SGFAIL);

			if (!(bbm.global.status & PSUSPEND)) {
				if (proc_mgr.alive(dbbmste.svrproc()) && SGT->_SGT_error_code != SGETIME)
					return svc_fini(svcinfo, rply_msg, SGFAIL);
			}

			/*
			 * If this is a TEMP sgclst creation then save the old STE
			 * at this point so we may later restore it.
			 */
			if (rqst->arg1.flag & DR_TEMP) {
				BBM->_BBM_olddbbm = dbbmste;
				BBM->_BBM_oldflag = true;
			}

			/*
			 * Set the mid to the BBMs mid so that the sgmngr will think it
			 * needs to restart the sgclst. Also set the pid to MAXLONG32
			 * so that we don't accidentally get a _tmprocalive of true.
			 */
			dbbmste.mid() = rqst->arg2.to; /* set to indicated node */
			dbbmste.pid() = std::numeric_limits<pid_t>::max();
			dbbmste.local.currclient = msghdr.rplyto;
			dbbmste.local.currrtype = msghdr.rplymtype;
			dbbmste.local.currriter = msghdr.rplyiter;
			dbbmste.local.curroptions = rqst->arg1.flag;
			dbbmste.local.currmsgid = msghdr.alt_flds.msgid;
			strcpy(dbbmste.local.currservice, "SGCDBBM");

			sg_wkinfo_t wkinfo;
			if (proc_mgr.get_wkinfo(SGC->_SGC_wka, wkinfo)) {
				dbbmste.rqaddr() = wkinfo.qaddr;
				dbbmste.rpaddr() = wkinfo.qaddr;
			} else {
				int saved_rqaddr = SGC->_SGC_proc.rqaddr;
				int qaddr = proc_mgr.getq(SGC->_SGC_proc.mid, 0, SGC->_SGC_uid, SGC->_SGC_gid, SGC->_SGC_perm);
				SGC->_SGC_proc.rqaddr = qaddr;
				if (SGC->_SGC_proc.rqaddr < 0)
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not create message queue for temporary sgclst")).str(SGLOCALE));
				proc_mgr.create_wkid(true);
				SGC->_SGC_proc.rqaddr = saved_rqaddr;
				dbbmste.rqaddr() = qaddr;
				dbbmste.rpaddr() = qaddr;
			}

			if (ste_mgr.update(&dbbmste, 1, U_LOCAL) != 1) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not update bulletin board for temporary sgclst")).str(SGLOCALE));
				return svc_fini(svcinfo, rply_msg, SGFAIL);
			}
			/*
			 * See if we need to update the QTE for sgclst.
			 */
			dbbmqte.hashlink.rlink = dbbmste.queue.qlink;
			if (qte_mgr.retrieve(S_BYRLINK, &dbbmqte, &dbbmqte, NULL) == 1) {
				dbbmqte.parms.paddr = dbbmste.rqaddr();
				qte_mgr.update(&dbbmqte, 1, 0);
			}

			// BBCLEAN right away instead of scheduling it for the next sanityscan
			bbclean(rqst->arg2.to, NOTAB);

			return svc_fini(svcinfo, rply_msg, SGFAIL);
		}
	case O_MIGDBBM:
		break;
	case O_PSUSPEND:
		{
			rpc_mgr.ustat(&rqst->arg1.sgid, 1, PSUSPEND, STATUS_OP_OR);
			return svc_fini(svcinfo, rply_msg, SGFAIL);
		}
	case O_PUNSUSPEND:
		{
			rpc_mgr.ustat(&rqst->arg1.sgid, 1, ~PSUSPEND, STATUS_OP_AND);
			return svc_fini(svcinfo, rply_msg, SGFAIL);
		}
	case O_PCLEAN:
		{
			brpc_mgr.pclean(rqst->arg1.host, rply);
			// Don't forward if there are no updates to be made.
			if (rply->rtn <= 0)
				forward = false;
		}
		break;
	case O_UDBBM:
		if (!brpc_mgr.udbbm(reinterpret_cast<sg_ste_t *>(rqst->data()))) {
			set_error(rply, SGESYSTEM);
			break;
		}
		rply->rtn = 1;
		break;
	case O_RRTE:
		{
			sg_clinfo_t info;
			sg_rte_t *rte;
			sg_rte_t *endp;
			sg_rte_t *clients;
			int cnt;
			int max;

			forward = false;
			memcpy(&info, rqst->data(), sizeof(sg_clinfo_t));
			int flag = rqst->arg1.flag;

			rte = SGC->_SGC_rtes;
			endp = SGC->_SGC_rtes + SGC->MAXACCSRS() + MAXADMIN;

			clients = reinterpret_cast<sg_rte_t *>(rply->data());
			time_t curtime = time(0);

			for (cnt = max = 0; rte < endp; rte++) {
				// Skip not in use entries, pid <= 0
				if (rte->pid <= 0)
					continue;

				// Skip server just coming up w/ both CLIENT & SERVER
				if ((flag & CLIENT) && (rte->rflags & SERVER))
					continue;

				// check match
				if((rte->rflags & flag)
					&& (info.usrname[0] == '\0' || strcmp(info.usrname,rte->usrname) == 0)
					&& (info.cltname[0] == '\0' || strcmp(info.cltname,rte->cltname) == 0)) {
					// check enough space in the message
					if (cnt + 1 > max) {
						max += 10;
						rply_msg->reserve(sg_rpc_rply_t::need(max * sizeof(sg_rte_t)));
						rply = rply_msg->rpc_rply();
						clients = reinterpret_cast<sg_rte_t *>(rply->data());
					}
					memcpy(clients + cnt, rte, sizeof(sg_rte_t));
					clients[cnt].rtimestamp = curtime - clients[cnt].rtimestamp;
					if (clients[cnt].rtimestamp < 0)
						clients[cnt].rtimestamp = 0;
					cnt++;
				}
			}

			if (cnt >= 0) {
				rply->datalen = cnt * sizeof(sg_rte_t);
				rply->rtn = cnt;
				rply->flags = SGRPTABENTS;
			}
		}
		break;
	case O_UPLOAD:
		{
			size_t pte_offset = sizeof(sg_bbmeters_t) + sizeof(sg_bbmap_t);
			int i;
			sg_ste_t gwste;
			sg_ste_t ste;
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_scte& scte_mgr = sg_scte::instance(SGT);
			sg_qte& qte_mgr = sg_qte::instance(SGT);

			scoped_bblock bblock(SGT);

			boost::shared_array<sg_lserver_t> auto_oldstes(new sg_lserver_t[SGC->MAXSVRS()]);
			sg_lserver_t *oldstes = auto_oldstes.get();

			boost::shared_array<sg_lservice_t> auto_oldsctes(new sg_lservice_t[SGC->MAXSVCS()]);
			sg_lservice_t *oldsctes = auto_oldsctes.get();

			boost::shared_array<sg_lqueue_t> auto_oldqtes(new sg_lqueue_t[SGC->MAXQUES()]);
			sg_lqueue_t *oldqtes = auto_oldqtes.get();

			for (i = 0; i < SGC->MAXSVRS(); i++) {
				sg_ste_t *ste = ste_mgr.link2ptr(i);
				oldstes[i] = ste->local;
			}

			for (i = 0; i < SGC->MAXSVCS(); i++) {
				sg_scte_t *scte = scte_mgr.link2ptr(i);
				oldsctes[i] = scte->local;
			}

			for (i = 0; i < SGC->MAXQUES(); i++) {
				sg_qte_t *qte = qte_mgr.link2ptr(i);
				oldqtes[i] = qte->local;
			}

			bool paired = brpc_mgr.pairedgw(SGC->_SGC_proc, &gwste, NULL);

			bbp->bbparms.bbversion = rqst->arg2.vers;

			// copy bbmeters to the local BB
			bbp->bbmeters.refresh_data(*reinterpret_cast<sg_bbmeters_t *>(rqst->data()));

			// update the bbmap
			sg_bbmap_t *bbmapp = reinterpret_cast<sg_bbmap_t *>(rqst->data() + sizeof(sg_bbmeters_t));
			bbp->bbmap.qte_free = bbmapp->qte_free;
			bbp->bbmap.sgte_free = bbmapp->sgte_free;
			bbp->bbmap.ste_free = bbmapp->ste_free;
			bbp->bbmap.scte_free = bbmapp->scte_free;

			// update the rest of the BB
			memcpy(SGC->_SGC_ptes, rqst->data() + pte_offset, rqst->datalen - pte_offset);

			for (i = 0; i < SGC->MAXSVRS(); i++) {
				sg_ste_t *ste = ste_mgr.link2ptr(i);
				ste->local = oldstes[i];
			}

			for (i = 0; i < SGC->MAXSVCS(); i++) {
				sg_scte_t *scte = scte_mgr.link2ptr(i);
				scte->local = oldsctes[i];
			}

			for (i = 0; i < SGC->MAXQUES(); i++) {
				sg_qte_t *qte = qte_mgr.link2ptr(i);
				qte->local = oldqtes[i];
			}

			if (paired && (gwste.global.status & RESTARTING)) {
				if (ste_mgr.update(&gwste, 1, U_LOCAL) < 0)
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to update status of restarting sggws")).str(SGLOCALE));
			}

			/*
			 * Retrieve the DBBMs STE and update the master site if
			 * necessary.
			 */
			ste.svrid() = MNGR_PRCID;
			ste.grpid() = CLST_PGID;
			if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) == 1)
				brpc_mgr.updatemaster(&ste);

			return svc_fini(svcinfo, rply_msg, SGSUCCESS);
		}
		break;
	case O_REFRESH:
		{
			forward = false;
			rply_msg->reserve(sg_rpc_rply_t::need(BBM->_BBM_pbbsize));
			rply = rply_msg->rpc_rply();
			rply->rtn = 1;

			memcpy(rply->data(), &bbp->bbmap, BBM->_BBM_pbbsize);
			rply->datalen = BBM->_BBM_pbbsize;
			rply->flags = 0;
		}
		break;
	case O_IMOK:	// sent from BBMs to sgclst to indicate they're OK
		{
			// 同时更新统计信息，更新所有节点
			int i;
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_scte& scte_mgr = sg_scte::instance(SGT);
			sg_qte& qte_mgr = sg_qte::instance(SGT);

			if (SGP->_SGP_ambbm) {
				BBM->_BBM_DBBM_status = DBBM_STATE_OK;
				return svc_fini(svcinfo, rply_msg, SGSUCCESS);
			}

			iter = BBM->_BBM_regmap.find(msghdr.sender.mid);
			if (iter == BBM->_BBM_regmap.end())
				return svc_fini(svcinfo, rply_msg, SGSUCCESS);

			registry_t& regent = iter->second;
			regent.tardy = 0;	// Remember sgmngr heard from recently
			regent.bbvers = rqst->arg1.vers;
			regent.sgvers = rqst->arg2.vers;
			regent.usrcnt = rqst->arg3.usrcnt;

			if (regent.status & PARTITIONED) {
				// It's back!  Try to unpartition
				unpartition(regent);
			}

			// 更新统计信息
			scoped_bblock bblock(SGT);

			char *data = rqst->data();
			int ste_cnt = *reinterpret_cast<int *>(data);
			data += sizeof(int);
			int scte_cnt = *reinterpret_cast<int *>(data);
			data += sizeof(int);

			int datalen = sizeof(int) * 2
				+ (sizeof(sgid_t) + sizeof(sg_nserver_t) + sizeof(sgid_t) + sizeof(sg_lqueue_t)) * ste_cnt
				+ (sizeof(sgid_t) + sizeof(sg_lservice_t)) * scte_cnt;
			if (rqst->datalen != datalen) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid request data length")).str(SGLOCALE));
				return svc_fini(svcinfo, rply_msg, SGFAIL);
			}

			for (i = 0; i < ste_cnt; i++) {
				sgid_t sgid = *reinterpret_cast<sgid_t *>(data);
				data += sizeof(sgid_t);

				sg_ste_t *ste = ste_mgr.sgid2ptr(sgid);
				if (ste->hashlink.sgid == sgid && !ste->svrproc().is_dbbm()) {
					sg_lserver_t *local = &ste->local;
					sg_nserver_t *nlocal = reinterpret_cast<sg_nserver_t *>(data);
					local->total_idle = nlocal->total_idle;
					local->total_busy = nlocal->total_busy;
					local->ncompleted = nlocal->ncompleted;
					local->wkcompleted = nlocal->wkcompleted;
					data += sizeof(sg_nserver_t);

					sgid = *reinterpret_cast<sgid_t *>(data);
					data += sizeof(sgid_t);

					sg_qte_t *qte = qte_mgr.sgid2ptr(sgid);
					if (qte->hashlink.sgid == sgid)
						qte->local = *reinterpret_cast<sg_lqueue_t *>(data);
					data += sizeof(sg_lqueue_t);
				} else {
					data += sizeof(sg_nserver_t) + sizeof(sgid_t) + sizeof(sg_lqueue_t);
				}
			}

			for (i = 0; i < scte_cnt; i++) {
				sgid_t sgid = *reinterpret_cast<sgid_t *>(data);
				data += sizeof(sgid_t);

				sg_scte_t *scte = scte_mgr.sgid2ptr(sgid);
				if (scte->hashlink.sgid == sgid && !scte->svrproc().is_dbbm())
					scte->local = *reinterpret_cast<sg_lservice_t *>(data);
				data += sizeof(sg_lservice_t);
			}

			sg_msghdr_t& rqst_msghdr = *rqst_msg;
			strcpy(rqst_msghdr.svc_name, BBM_SVC_NAME);
			rqst_msghdr.rcvr = regent.proc;
			rqst_msghdr.rplymtype = NULLMTYPE;

			sg_metahdr_t& rqst_metahdr = *rqst_msg;
			rqst_metahdr.mid = regent.proc.mid;
			rqst_metahdr.qaddr = regent.proc.rqaddr;
			rqst_metahdr.flags |= METAHDR_FLAGS_DBBMFWD;

			rqst_msg->set_length(sg_rpc_rqst_t::need(0));
			ipc_mgr.send(rqst_msg, 0);
		}
		return svc_fini(svcinfo, rply_msg, SGSUCCESS);
	case O_UPLOCAL:
		{
			// 更新统计信息，只更新远程节点
			int i;
			sg_ste& ste_mgr = sg_ste::instance(SGT);
			sg_scte& scte_mgr = sg_scte::instance(SGT);
			sg_qte& qte_mgr = sg_qte::instance(SGT);

			if (SGP->_SGP_amdbbm) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgclst can't handle this opcode")).str(SGLOCALE));
				set_error(rply, SGESYSTEM);
				break;
			}

			char *data = rqst->data();
			int ste_cnt = *reinterpret_cast<int *>(data);
			data += sizeof(int);
			int scte_cnt = *reinterpret_cast<int *>(data);
			data += sizeof(int);

			int datalen = sizeof(int) * 2
				+ (sizeof(sgid_t) + sizeof(sg_nserver_t) + sizeof(sgid_t) + sizeof(sg_lqueue_t)) * ste_cnt
				+ (sizeof(sgid_t) + sizeof(sg_lservice_t)) * scte_cnt;
			if (rqst->datalen != datalen) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid request data length")).str(SGLOCALE));
				set_error(rply, SGESYSTEM);
				break;
			}

			// 更新统计信息时需要保证一致性
			scoped_bblock bblock(SGT);

			for (i = 0; i < ste_cnt; i++) {
				sgid_t sgid = *reinterpret_cast<sgid_t *>(data);
				data += sizeof(sgid_t);

				sg_ste_t *ste = ste_mgr.sgid2ptr(sgid);
				if (ste->hashlink.sgid == sgid && (ste->mid() != SGC->_SGC_proc.mid || ste->svrproc().is_dbbm())) {
					sg_lserver_t *local = &ste->local;
					sg_nserver_t *nlocal = reinterpret_cast<sg_nserver_t *>(data);
					local->total_idle = nlocal->total_idle;
					local->total_busy = nlocal->total_busy;
					local->ncompleted = nlocal->ncompleted;
					local->wkcompleted = nlocal->wkcompleted;
					data += sizeof(sg_nserver_t);

					sgid = *reinterpret_cast<sgid_t *>(data);
					data += sizeof(sgid_t);

					sg_qte_t *qte = qte_mgr.sgid2ptr(sgid);
					if (qte->hashlink.sgid == sgid)
						qte->local = *reinterpret_cast<sg_lqueue_t *>(data);
					data += sizeof(sg_lqueue_t);
				} else {
					data += sizeof(sg_nserver_t) + sizeof(sgid_t) + sizeof(sg_lqueue_t);
				}
			}

			for (i = 0; i < scte_cnt; i++) {
				sgid_t sgid = *reinterpret_cast<sgid_t *>(data);
				data += sizeof(sgid_t);

				sg_scte_t *scte = scte_mgr.sgid2ptr(sgid);
				if (scte->hashlink.sgid == sgid && (scte->mid() != SGC->_SGC_proc.mid || scte->svrproc().is_dbbm()))
					scte->local = *reinterpret_cast<sg_lservice_t *>(data);
				data += sizeof(sg_lservice_t);
			}
		}
		break;
	default:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown opcode")).str(SGLOCALE));
		set_error(rply, SGT->_SGT_error_code);
		break;
	}

	// broadcast request & get replies
	if (SGP->_SGP_amdbbm && rply->rtn > 0 && forward) {
		int wflag;
		int bcnt;

		switch (opcode) {
		case O_URGSTR:
		case O_AURGSTR:
		case O_LANINC:
			wflag = false;
			break;
		default:
			wflag = SGAWAITRPLY;
			break;
		}

		if ((bcnt = brpc_mgr.bbmsnd(rqst_msg, SGSIGRSTRT | wflag)) < 0) {
			set_error(rply, SGEBBINSANE);
			ret = -1;
		}

		if (!(wflag & SGAWAITRPLY)) {
			if (bcnt != BBM->_BBM_regmap.size())
				brpc_mgr.cleanup();
		}
	}

	if (SGP->_SGP_amdbbm && rply->rtn == 1) {
		if (opcode == O_RGSTR) {
			GPP->write_log((_("INFO: sgmngr started on {1} - Release {2}")
				% SGC->_SGC_ptes[SGC->midpidx(rgst_mid)].lname
				% rgst_regent.release).str(SGLOCALE));
			BBM->_BBM_regmap[rgst_mid] = rgst_regent;
		}
	}

	switch (opcode) {
	case O_LANINC:
	case O_AURGSTR:
		return svc_fini(svcinfo, rply_msg, SGFAIL);
	default:
		break;
	}

	sg_handle::instance(SGT).mkstale(NULL);

	// Now set up return header and send reply
	if (rply->rtn < 0)
		rply->datalen = 0;		/* error no data */

	sg_msghdr_t& rply_msghdr = *rply_msg;
	rply_msghdr = msghdr;
	if (rply->rtn < 0) {
		rply_msghdr.error_code = SGT->_SGT_error_code;
		if (SGT->_SGT_error_code == SGEOS)
			rply_msghdr.native_code = SGT->_SGT_native_code;
		else
			rply_msghdr.native_code = 0;
	} else {
		rply_msghdr.error_code = 0;
		rply_msghdr.native_code = 0;
	}
	rply_msghdr.error_detail = SGT->_SGT_error_detail;
	rply_msghdr.ur_code = 0;
	rply_msghdr.sender = SGC->_SGC_proc;
	rply_msghdr.rcvr = rply_msghdr.rplyto;

	rply_msg->set_length(sg_rpc_rply_t::need(rply->datalen));

	if (rply_msghdr.rplywanted() && !ipc_mgr.send(rply_msg, SGSIGRSTRT | SGREPLY))
		return svc_fini(svcinfo, rply_msg, -1);

	return svc_fini(svcinfo, rply_msg, ret);
}

// The sgmngr sends an BBCLEAN opcode message to itself so that the sgclst entry is cleaned up right away.
void bbm_svc::bbclean(int mid, ttypes sect)
{
	sg_ste_t bbm;
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("mid={1}, sect={2}") % mid % sect).str(SGLOCALE), NULL);
#endif

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(int)));
	if (msg == NULL)
		return;

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_BBCLEAN | VERSION;
	*reinterpret_cast<int *>(rqst->data()) = sect;

	sg_msghdr_t& msghdr = *msg;
	msghdr.rplymtype = NULLMTYPE;

	bbm.grpid() = SGC->mid2grp(mid);
	bbm.svrid() = MNGR_PRCID;
	if (ste_mgr.retrieve(S_GRPID, &bbm, &bbm, NULL) < 0) {
		if (bbm.svrproc().is_dbbm())
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: bbclean failed, couldn't find sgclst")).str(SGLOCALE));
		else
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: bbclean failed, couldn't find sgmngr")).str(SGLOCALE));
		return;
	}

	if (!rpc_mgr.send_msg(msg, &bbm.svrproc(), SGNOREPLY | SGNOBLOCK))
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: bbclean failed, message send/receive error")).str(SGLOCALE));
}


/*
 *  unpartition:  Verify that sgmngr i is no longer partitioned, and
 *		  unpartition it if possible.
 */
void bbm_svc::unpartition(registry_t& reg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	bbm_rpc& brpc_mgr = bbm_rpc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	/*
	 *  This sgmngr was partitioned from the sgclst and
	 *  now appears to be alive.  To prevent thrashing, verify
	 *  that it's REALLY back, alive, and working.
	 *  _tmprocalive() can't be trusted because it returns
	 *  true on timeout, so do a lower-level kill 0.
	 */
	if (!proc_mgr.nwkill(reg.proc, 0, bbp->bbparms.scan_unit)) {
		// Something still wrong, leave partitioned
		return;
	}

	/*
	 *  The sgmngr is now known to be back and reconnected,
	 *  so "unpartition" and upload it.
	 */
	GPP->write_log((_("INFO: sgmngr now unpartitioned, node= {1}")
		% SGC->pmtolm(reg.proc.mid)).str(SGLOCALE));

	reg.status &= ~PARTITIONED;

	if (nwpsusp(reg.sgid, O_PUNSUSPEND) < 0) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not unsuspend reconnected node {1}")
			% SGC->pmtolm(reg.proc.mid)).str(SGLOCALE));
		reg.status |= BCSTFAIL; // Partition it again
		brpc_mgr.cleanup();
		return;
	}

	reg.status |= (NACK_DONE | SGCFGFAIL); // Force upload of BB and SGPROFILE
	brpc_mgr.cleanup();
}

int bbm_svc::nwpsusp(sgid_t sgid, int opcode)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	bbm_rpc& brpc_mgr = bbm_rpc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("sgid={1}, opcode={2}") % sgid % opcode).str(SGLOCALE), &retval);
#endif

	// First mark the appropriate status locally and the broadcast update.
	if (opcode == O_PSUSPEND) {
		if (rpc_mgr.ustat(&sgid, 1, PSUSPEND, STATUS_OP_OR) < 0)
			return retval;
	} else {
		if (rpc_mgr.ustat(&sgid, 1, ~PSUSPEND, STATUS_OP_AND) < 0)
			return retval;
	}

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(0));
	sg_msghdr_t& msghdr = *msg;
	msghdr.rplymtype = NULLMTYPE;

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->datalen = 0;
	rqst->opcode = opcode | VERSION;
	rqst->arg1.sgid = sgid;

	// sgclst broadcasts to BBMs to mark the partitioned entry in BB

	retval = brpc_mgr.broadcast(msg);
	return retval;
}

svc_fini_t bbm_svc::prot_fail(message_pointer& svcinfo, message_pointer& fini_msg, sg_rpc_rply_t *rply, int err, int val)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("err={1}, val={2}") % err % val).str(SGLOCALE), NULL);
#endif

	if (rply != NULL) {	// an RPC_RQUST buffer
		set_error(rply, err);
		rply->datalen = 0;
		fini_msg->set_length(sg_rpc_rply_t::need(rply->datalen));
	}

	sgc_ctx *SGC = SGT->SGC();
	sg_msghdr_t& msghdr = *fini_msg;
	msghdr = SGT->_SGT_msghdr;
	msghdr.error_code = SGT->_SGT_error_code;
	msghdr.native_code = 0;

	// send response to requester
	if (msghdr.rplywanted()) {
		msghdr.sender = SGC->_SGC_proc;
		msghdr.rcvr = msghdr.rplyto;

		if (!sg_ipc::instance(SGT).send(fini_msg, SGSIGRSTRT | SGREPLY)) {
			if (val == SGFATAL) {
				SGT->_SGT_error_code = err;
				return svc_fini(svcinfo, fini_msg, val);
			} else {
				return svc_fini(svcinfo, fini_msg, -1);
			}
		}
	} else {
		SGT->_SGT_error_code = err;

		// 当系统结束时，忽略来自GWS的消息
		sg_bboard_t *& bbp = SGC->_SGC_bbp;
		if (!(bbp->bbmeters.status & SHUTDOWN) || !msghdr.sender.is_gws()) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation request from process {1} failed")
				% msghdr.sender.pid).str(SGLOCALE));
		}
	}

	if (val == SGFATAL)
		return svc_fini(svcinfo, fini_msg, val);
	else
		return svc_fini(svcinfo, fini_msg, SGFAIL);	// force positive return
}

svc_fini_t bbm_svc::svc_fini(message_pointer& svcinfo, message_pointer& fini_msg, int rval)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("rval={1}") % rval).str(SGLOCALE), NULL);
#endif

	if (SGT->_SGT_fini_called)
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: svc_fini() called more times")).str(SGLOCALE));

	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	BBM->_BBM_inmasterbb = false;

	/*
	 * If we're a bbm and this message came from sgclst, then update our
	 * BB and SGPROFILE version numbers appropriately.
	 */
	sg_msghdr_t& msghdr = *svcinfo;

	if (SGP->_SGP_ambbm && rval == SGSUCCESS && msghdr.sender.grpid == CLST_PGID) {
		if (msghdr.alt_flds.prevbbv != -1
			&& (bbp->bbparms.bbversion == 0 || msghdr.alt_flds.prevbbv == bbp->bbparms.bbversion)) {
			bbp->bbparms.bbversion = msghdr.alt_flds.newver;
		}
		if (msghdr.alt_flds.prevsgv != -1 && msghdr.alt_flds.prevsgv == bbp->bbparms.sgversion)
			bbm_rpc::instance(SGT).updatevs(msghdr.alt_flds.newver);
	}

	SGT->_SGT_fini_called = true;
	SGT->_SGT_fini_rval = rval;
	SGT->_SGT_fini_urcode = 0;
	SGT->_SGT_fini_msg = fini_msg;

	return svc_fini_t();
}

void bbm_svc::set_error(sg_rpc_rply_t *rply, int error_code)
{
	rply->error_code = error_code;
	rply->rtn = -1;
}

/*
 * Retrieve the status field of the entry associated with sgid.
 * Upon successful retrieval of the indicated entry, the status
 * field is returned as the value of the function. Otherwise,
 * -1 is returned and tperrno is set to indicate the cause of failure.
 */
int bbm_svc::getstat(sgid_t sgid)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("sgid={1}") % sgid).str(SGLOCALE), &retval);
#endif

	if (sgid == BADSGID) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid table entry information given")).str(SGLOCALE));
		}
		return retval;
	}

	switch (sgid & SGID_TMASK) {
	case SGID_QUET:
		{
			sg_qte_t qte;
			sg_qte& qte_mgr = sg_qte::instance(SGT);

			if (qte_mgr.retrieve(S_BYSGIDS, &qte, &qte, &sgid) == 1)
				retval = qte.global.status;
			break;
		}
	case SGID_SVRT:
		{
			sg_ste_t ste;
			sg_ste& ste_mgr = sg_ste::instance(SGT);

			if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, &sgid) == 1)
				retval = ste.global.status;
			break;
		}
	case SGID_SVCT:
		{
			sg_scte_t scte;
			sg_scte& scte_mgr = sg_scte::instance(SGT);

			if (scte_mgr.retrieve(S_BYSGIDS, &scte, &scte, &sgid) == 1)
				retval = scte.global.status;
			break;
		}
	default:
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid table entry information given")).str(SGLOCALE));
		}
		break;
	}

	return retval;
}

}
}

