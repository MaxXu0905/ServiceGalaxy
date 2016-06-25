/*
 * SGAdaptor.cpp
 *
 *  Created on: 2012-2-15
 *      Author: renyz
 */

#include "monitorpch.h"
#include "sg_internal.h"
#include "SGAdaptor.h"
namespace {
using namespace ai::sg;
class Impl: public ai::sg::SGAdaptor {
public:
	Impl() :
		GPP(gpp_ctx::instance()), SGP(sgp_ctx::instance()) {
	}

	bool enter()
	{
		sgt_ctx *SGT = sgt_ctx::instance();
		sgc_ctx *SGC = SGT->SGC();
		return SGC->enter(SGT, 0);
	}

	bool leave()
	{
		sgt_ctx *SGT = sgt_ctx::instance();
		sgc_ctx *SGC = SGT->SGC();
		return SGC->leave(SGT, 0);
	}

	sg_bboard_t& get_sg_bboard_t() {
		sgt_ctx *SGT = sgt_ctx::instance();
		sgc_ctx *SGC = SGT->SGC();
		return *(SGC->_SGC_bbp);
	}
	//主机信息数组
	boost::shared_ptr<std::vector<sg_mchent_t *> > get_sg_mchent_t() {
		sgt_ctx *SGT = sgt_ctx::instance();
		std::vector<sg_mchent_t *> *mvec = new std::vector<sg_mchent_t *>;
		boost::shared_ptr<std::vector<sg_mchent_t *> > result(mvec);
		sg_config::mch_iterator iter = SGCONF(SGT).mch_begin();
		sg_config::mch_iterator end = SGCONF(SGT).mch_end();
		while (iter != end) {
			sg_mchent_t &m = *iter;
			mvec->push_back(&m);
			iter++;
		}
		return result;
	}
	//主机监听数组
	boost::shared_ptr<std::vector<sg_netent_t *> > get_sg_netent_t(
			const char *lmid /*输入0 返回所有，指主机返回主机所属的net*/) {
		sgt_ctx *SGT = sgt_ctx::instance();
		std::vector<sg_netent_t *> *mvec = new std::vector<sg_netent_t *>;
		boost::shared_ptr<std::vector<sg_netent_t *> > result(mvec);
		sg_config::net_iterator iter = SGCONF(SGT).net_begin();
		sg_config::net_iterator end = SGCONF(SGT).net_end();
		while (iter != end) {
			sg_netent_t & net = *iter;
			if (lmid) {
				if (strcmp(iter->lmid, lmid) == 0) {
					mvec->push_back(&net);
				}
			} else {
				mvec->push_back(&net);
			}
			iter++;
		}
		return result;
	}
	//队列
	std::pair<int, Tsg_qte_t> get_sg_qte_t(int rlink) {
		sgt_ctx *SGT = sgt_ctx::instance();
		sgc_ctx *SGC = SGT->SGC();

		sg_qte & qte_mgr = sg_qte::instance(SGT);
		sg_qte_t key;
		int cnt;
		Tsg_qte_t keys;
		if (rlink != -1) {
			key.hashlink.rlink = rlink;
			Tsg_qte_t auto_qtes(new sg_qte_t[1]);
			cnt = qte_mgr.retrieve(S_BYRLINK, &key, auto_qtes.get(), NULL);
			keys = auto_qtes;
		} else {
			key.saddr()[0] = '\0';
			Tsg_qte_t auto_qtes(new sg_qte_t[SGC->MAXQUES() + 1]);
			cnt = qte_mgr.retrieve(S_QUEUE, &key, auto_qtes.get(), NULL);
			keys = auto_qtes;
		}
		if (cnt < 0)
			return std::pair<int, Tsg_qte_t> (0, Tsg_qte_t());
		else
			return std::pair<int, Tsg_qte_t> (cnt, keys);
	}
	//Server Group
	std::pair<int, Tsg_sgte_t> get_sg_sgte_t() {
		sgt_ctx *SGT = sgt_ctx::instance();
		sgc_ctx *SGC = SGT->SGC();

		sg_sgte_t key;
		sg_sgte& sgte_mgr = sg_sgte::instance(SGT);
		key.curmid() = ALLMID;
		Tsg_sgte_t auto_sgtes(new sg_sgte_t[SGC->MAXSGT() + 1]);
		int cnt = sgte_mgr.retrieve(S_MACHINE, &key, auto_sgtes.get(), NULL);
		if (cnt < 0)
			return std::pair<int, Tsg_sgte_t>(0, Tsg_sgte_t());
		else
			return std::pair<int, Tsg_sgte_t>(cnt, auto_sgtes);
	}
	//Server
	std::pair<int, Tsg_ste_t> get_sg_ste_t() {
		sgt_ctx *SGT = sgt_ctx::instance();
		sgc_ctx *SGC = SGT->SGC();

		sg_ste_t key;
		sg_ste & ste_mgr = sg_ste::instance(SGT);
		key.mid() = ALLMID;
		Tsg_ste_t auto_stes(new sg_ste_t[SGC->MAXSVRS() + 1]);
		int cnt = ste_mgr.retrieve(S_MACHINE, &key, auto_stes.get(), NULL);
		if (cnt < 0)
			return std::pair<int, Tsg_ste_t>(0, Tsg_ste_t());
		else
			return std::pair<int, Tsg_ste_t>(cnt, auto_stes);
	}
	//Service
	std::pair<int, Tsg_scte_t> get_sg_scte_t() {
		sgt_ctx *SGT = sgt_ctx::instance();
		sgc_ctx *SGC = SGT->SGC();

		sg_scte_t key;
		key.parms.svc_name[0]='\0';
		sg_scte& scte_mgr = sg_scte::instance(SGT);
		boost::shared_array<sg_scte_t> auto_sctes(
				new sg_scte_t[SGC->MAXSVCS() + 1]);
		int cnt = scte_mgr.retrieve(S_ANY, &key, auto_sctes.get(), NULL);
		if (cnt < 0)
			return std::pair<int, Tsg_scte_t>(0, Tsg_scte_t());
		else
			return std::pair<int, Tsg_scte_t>(cnt, auto_sctes);
	}
	const char* mid2lmid(int mid) {
		sgt_ctx *SGT = sgt_ctx::instance();
		sgc_ctx *SGC = SGT->SGC();

		return SGC->mid2lmid(mid);
	}

	int32_t startServer(int32_t groupId, int32_t serverId) {
		string s = (boost::format("sgboot -g%1%  -i%2%") % groupId % serverId).str();
		MonitorLog::write_log((_("run command ")).str(SGLOCALE) + s);
		return system(s.c_str());
	}
	int32_t stopServer(int32_t groupId, int32_t serverId) {
		string s = (boost::format("sgshut -g%1%  -i%2%") % groupId % serverId).str();
		MonitorLog::write_log((_("run command ")).str(SGLOCALE) + s);
		return system(s.c_str());
	}
	int32_t startServerByGroup(int32_t groupId) {
		string s = (boost::format("sgboot -g%1%") % groupId).str();
		MonitorLog::write_log((_("INFO: run command ")).str(SGLOCALE) + s);
		return system(s.c_str());
	}
	int32_t stopServerByGroup(int32_t groupId) {
		string s = (boost::format("sgshut -g%1% -E") % groupId).str();
		MonitorLog::write_log((_("INFO: run command ")).str(SGLOCALE) + s);
		return system(s.c_str());
	}
	int32_t startServerByMachine(string machineId) {
		string s = (boost::format("sgboot -l%1%") % machineId).str();
		MonitorLog::write_log((_("INFO: run command ")).str(SGLOCALE) + s);
		return system(s.c_str());
	}
	int32_t stopServerByMachine(string machineId) {
		string s = (boost::format("sgshut -l%1% -E") % machineId).str();
		MonitorLog::write_log((_("INFO: run command ")).str(SGLOCALE) + s);
		return system(s.c_str());
	}
	int32_t startServerALL() {
		string s = "sgboot -S";
		MonitorLog::write_log((_("run command ")).str(SGLOCALE) + s);
		return system(s.c_str());
	}
	int32_t stopServerALL() {
		string s = "sgshut -S -E";
		MonitorLog::write_log((_("run command ")).str(SGLOCALE) + s);
		return system(s.c_str());
	}

	int32_t resumeServer(const int64_t sgid) {
		sgt_ctx *SGT = sgt_ctx::instance();
		sg_rpc & sg_mgr = sg_rpc::instance(SGT);
		bool result= sg_mgr.set_stat(sgid, ~SUSPENDED, STATUS_OP_AND);
		return !result;
	}
	int32_t suspendServer(const int64_t sgid) {
		sgt_ctx *SGT = sgt_ctx::instance();
		sg_rpc & sg_mgr = sg_rpc::instance(SGT);
		bool result =sg_mgr.set_stat(sgid, SUSPENDED, STATUS_OP_OR);
		return !result;
	}

	int32_t resumeQueue(const int64_t sgid) {
		sgt_ctx *SGT = sgt_ctx::instance();
		sg_rpc & sg_mgr = sg_rpc::instance(SGT);
		return !sg_mgr.set_stat(sgid, ~SUSPENDED, STATUS_OP_AND);
	}

	int32_t suspendQueue(const int64_t sgid) {
		sgt_ctx *SGT = sgt_ctx::instance();
		sg_rpc & sg_mgr = sg_rpc::instance(SGT);
		return !sg_mgr.set_stat(sgid, SUSPENDED, STATUS_OP_OR);
	}

	virtual int32_t resumeService(const int64_t sgid) {
		sgt_ctx *SGT = sgt_ctx::instance();
		sg_rpc & sg_mgr = sg_rpc::instance(SGT);
		return !sg_mgr.set_stat(sgid, ~SUSPENDED, STATUS_OP_AND);
	}
	virtual int32_t suspendService(const int64_t sgid) {
		sgt_ctx *SGT = sgt_ctx::instance();
		sg_rpc & sg_mgr = sg_rpc::instance(SGT);
		return !sg_mgr.set_stat(sgid, SUSPENDED, STATUS_OP_OR);
	}

private:
	sg_config& SGCONF(sgt_ctx *SGT) {
		return sg_config::instance(SGT);
	}
	gpp_ctx *GPP;
	sgp_ctx *SGP;
};
}
namespace ai {
namespace sg {
SGAdaptor::SGAdaptor() {
}
SGAdaptor::~SGAdaptor() {
}
boost::shared_ptr<SGAdaptor> SGAdaptor::create() {
	Impl * impl = new Impl;
	boost::shared_ptr<SGAdaptor> implptr(impl);
	return implptr;
}
}

}

