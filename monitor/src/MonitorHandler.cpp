/*
 * MonitorHandler.cpp
 *
 *Thrift Hander接口实现类
 *
 *  Created on: 2012-2-16
 *      Author: renyz
 */

#include "MonitorHandler.h"
#include "TaskManager.h"

namespace {
using namespace ai::sg;
/*针对pair元素数组对，进行循环转换（sg平台信息到thrift接口信息）并存入_return
 *
 * 模板方式，减少代码量
 * */
template<typename I, typename R, typename F> void pair2Vector(const std::pair<
		int, boost::shared_array<I> > & inPair, std::vector<R> & _return,
		const F & f/*回调函数，提供个性化处理*/
) {
	boost::function<void(I*, R*)> bf = f;
	for (int i = 0; i < inPair.first; i++) {
		R r;
		translate(inPair.second[i], r);
		if (!bf.empty()) {
			bf(&inPair.second[i], &r);
		}
		_return.push_back(r);
	}
}
template<typename I, typename R> inline void pair2Vector(const std::pair<int,
		boost::shared_array<I> > & inPair, std::vector<R> & _return) {
	boost::function<void(I*, R*)> bf;
	pair2Vector(inPair, _return, bf);
}
/*
 * 类型转换函数
 */
void translate(const sg_bbparms_t& s, ClusterInfo& d);
void translate(const sg_bbmeters_t& s, ClusterState& d);
void translate(const sg_netent_t& s, MachineNet& d);
void translate(const sg_mchent_t& s, MachineInfo& d);
void translate(const sg_sgte_t& s, ServerGroup& d);
void translate(const sg_ste_t& s, ServerInfo& d);
void translate(const sg_ste_t& s, ServerState& d);
void translate(const sg_scte_t& s, ServiceInfo& d);
void translate(const sg_scte_t& s, ServiceState& d);
void translate(const sg_qte_t& s, QueueInfo& d);
void translate(const sg_qte_t& s, QueueState& d);
//Server起停认务生成函数
OperateTaskPtr buildServerOperateTask(boost::shared_ptr<
		SGAdaptor> & adaptor, const ServerOperate & serverOperate);

}
namespace ai {
namespace sg {

void MonitorHandler::GetClusterInfo(ClusterInfo& clusterInfo) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	const sg_bboard_t& bboard = adaptor->get_sg_bboard_t();
	const sg_bbparms_t& bbparms = bboard.bbparms;
	translate(bbparms, clusterInfo);
}

void MonitorHandler::GetClusterState(ClusterState& clusterState) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	const sg_bboard_t& bboard = adaptor->get_sg_bboard_t();
	const sg_bbmeters_t& bbmeters = bboard.bbmeters;
	translate(bbmeters, clusterState);
}
void MonitorHandler::GetMachineInfoList(std::vector<MachineInfo> & machineInfos) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	boost::shared_ptr<std::vector<sg_mchent_t *> > vecptr = adaptor->get_sg_mchent_t();
	BOOST_FOREACH(sg_mchent_t * m, *vecptr ) {
		MachineInfo machineInfo;
		translate(*m,machineInfo);
		boost::shared_ptr<std::vector<sg_netent_t *> > netptr=adaptor->get_sg_netent_t(m->lmid);
		//TODO 可能采用一次循环net数组，缓存所属主机的方式进行优化
		BOOST_FOREACH(sg_netent_t * n, *netptr ) {
			MachineNet net;
			translate(*n,net);
			machineInfo.nets.push_back(net);
		}
		machineInfos.push_back(machineInfo);
	}
}

BBVERSION MonitorHandler::getBBVersion() {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	return adaptor->get_sg_bboard_t().bbparms.bbversion;
}
void MonitorHandler::setServerGroup(const sg_sgte_t* s, ServerGroup * d) {
	d->lmid= adaptor->mid2lmid(s->curmid());
}
void MonitorHandler::GetServerGroupList(std::vector<ServerGroup> & _return) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	pair2Vector(adaptor->get_sg_sgte_t(),_return,boost::bind(&MonitorHandler::setServerGroup, this,_1,_2));
}
/*
 回调函数设置
 queuesgid
 servname
 */
void MonitorHandler::setServerInfo(const sg_ste_t* s, ServerInfo * d) {
	std::pair<int, SGAdaptor::Tsg_qte_t> qs=adaptor->get_sg_qte_t(s->queue.qlink);
	if(qs.first==1) {
		d->queuesgid=qs.second[0].sgid();
		d->servname=qs.second[0].parms.filename;
	}
}
void MonitorHandler::GetServerInfoList(std::vector<ServerInfo> & _return) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	pair2Vector(adaptor->get_sg_ste_t(),_return,boost::bind(&MonitorHandler::setServerInfo, this,_1,_2));
}
void MonitorHandler::GetServerStateList(std::vector<ServerState> & _return) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	pair2Vector(adaptor->get_sg_ste_t(),_return);
}
void MonitorHandler::GetServiceInfoList(std::vector<ServiceInfo> & _return) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	pair2Vector(adaptor->get_sg_scte_t(),_return);
}
void MonitorHandler::GetServiceStateList(std::vector<ServiceState> & _return) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	pair2Vector(adaptor->get_sg_scte_t(),_return);
}
void MonitorHandler::GetQueueInfoList(std::vector<QueueInfo> & _return) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	pair2Vector(adaptor->get_sg_qte_t(),_return);
}
void MonitorHandler::GetQueueStateList(std::vector<QueueState> & _return) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	pair2Vector(adaptor->get_sg_qte_t(),_return);
}
void MonitorHandler::getAllInfo(AllInfo& _return, const BBVERSION bbversion) {
	GetClusterState(_return.clusterState);
	GetServerStateList(_return.serverStates);
	GetServiceStateList( _return.serviceStates);
	GetQueueStateList(_return.queueStates);
	_return.bbversion=getBBVersion();

	if(_return.bbversion!=bbversion) {
		GetClusterInfo(_return.clusterInfo);
		GetMachineInfoList(_return.machineInfos);
		GetServerGroupList(_return.serverGroups);
		GetServerInfoList(_return.serverInfos);
		GetServiceInfoList(_return.serviceInfos);
		GetQueueInfoList(_return.queueInfos);
	}
}

int32_t MonitorHandler::startOrStopServer(const std::vector<ServerOperate> & operates) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	TaskManager &tm=TaskManager::instance();
	BOOST_FOREACH(ServerOperate serverOperate,operates ) {
		boost::shared_ptr<OperateTask> task=buildServerOperateTask(adaptor,serverOperate);
		int32_t result=tm.push(task);
		if(result>0) {
			return result;
		}
	}
	return 0;
}
int32_t MonitorHandler::updateServer(const Operate::type operate, const std::vector<int64_t> & sgids) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	BOOST_FOREACH(int64_t id, sgids ) {
		if(operate==Operate::Resume) {
			return adaptor->resumeServer(id);
		} else if(operate==Operate::Suspend) {
			return adaptor->suspendServer(id);
		}
	}
	return -1;
}
int32_t MonitorHandler::updateQueue(const Operate::type operate, const std::vector<int64_t> & sgids) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	BOOST_FOREACH(int64_t id, sgids ) {
		if(operate==Operate::Resume) {
			return adaptor->resumeQueue(id);
		} else if(operate==Operate::Suspend) {
			return adaptor->suspendQueue(id);
		}
	}
	return -1;
}
int32_t MonitorHandler::updateService(const Operate::type operate, const std::vector<int64_t> & sgids) {
	adaptor->enter();
	BOOST_SCOPE_EXIT((&adaptor)) {
		adaptor->leave();
	} BOOST_SCOPE_EXIT_END

	BOOST_FOREACH(int64_t id, sgids ) {
		if(operate==Operate::Resume) {
			return adaptor->resumeService(id);
		} else if(operate==Operate::Suspend) {
			return adaptor->suspendService(id);
		}
	}
	return -1;
}

}

}
namespace {
using namespace ai::sg;

void translate(const sg_bbparms_t& s, ClusterInfo & d) {
	d.ipckey = s.ipckey;
	d.uid = s.uid;
	d.gid = s.gid;
	d.perm = s.perm;
	d.options = s.options;
	if (s.master[0][0] != '\0') {
		d.master = s.master[0];
		for (int i = 1; s.master[i][0] != '\0'; i++) {
			d.master += ",";
			d.master += s.master[i];
		}
	}
	d.current = s.current;
	d.maxpes = s.maxpes;
	d.maxnodes = s.maxnodes;
	d.maxaccsrs = s.maxaccsrs;
	d.maxques = s.maxques;
	d.maxsgt = s.maxsgt;
	d.maxsvrs = s.maxsvrs;
	d.maxsvcs = s.maxsvcs;
	d.quebkts = s.quebkts;
	d.sgtbkts = s.sgtbkts;
	d.svrbkts = s.svrbkts;
	d.svcbkts = s.svcbkts;
	d.scan_unit = s.scan_unit;
	d.sanity_scan = s.sanity_scan;
	d.stack_size = s.stack_size;
	d.max_num_msg = s.max_num_msg;
	d.max_msg_size = s.max_msg_size;
	d.block_time = s.block_time;
	d.dbbm_wait = s.dbbm_wait;
	d.bbm_query = s.bbm_query;
}

void translate(const sg_bbmeters_t& s, ClusterState & d) {
	d.currload = s.currload;
	//d.cmachines = s.cmachines;
	d.cques = s.cques;
	d.csvrs = s.csvrs;
	d.csvcs = s.csvcs;
	d.csgt = s.csgt;
	d.caccsrs = s.caccsrs;
	d.maxmachines = s.maxmachines;
	d.maxques = s.maxques;
	d.maxsvrs = s.maxsvrs;
	d.maxsvcs = s.maxsvcs;
	d.maxsgt = s.maxsgt;
	d.maxaccsrs = s.maxaccsrs;
	d.wkinitiated = s.wkinitiated;
	d.wkcompleted = s.wkcompleted;
	d.rreqmade = s.rreqmade;
}
void translate(const sg_netent_t& s, MachineNet& d) {
	d.lmid = s.lmid;
	d.naddr = s.naddr;
	d.nlsaddr = s.nlsaddr;
	d.netgroup = s.netgroup;
}
void translate(const sg_mchent_t& s, MachineInfo & d) {
	d.pmid = s.pmid;
	d.lmid = s.lmid;
	d.sgdir = s.sgdir;
	d.appdir = s.appdir;
	d.sgconfig = s.sgconfig;
	d.envfile = s.envfile;
	d.ulogpfx = s.ulogpfx;
	d.mid = s.mid;
	d.flags = s.flags;
	d.maxaccsrs = s.maxaccsrs;
	d.perm = s.perm;
	d.uid = s.uid;
	d.gid = s.gid;
	d.net_load = s.netload;
}
void translate(const sg_sgte_t& s, ServerGroup & d) {
	d.sgid = s.hashlink.sgid;
	d.grpid = s.parms.grpid;
	d.sgname = s.parms.sgname;
	d.curmid = s.curmid();
}

void translate(const sg_ste_t& s, ServerInfo& d) {
	d.sgid = s.hashlink.sgid;

	d.grpid = s.grpid();
	d.svrid = s.svrid();
	/*
	 d.queuesgid
	 d.servname;
	 通过回调函数设置
	 */
}
void translate(const sg_ste_t& s, ServerState& d) {
	d.sgid = s.sgid();
	d.serverid = s.svrid();
	d.total_idle = s.local.total_idle;
	d.total_busy = s.local.total_busy;
	d.ncompleted = s.local.ncompleted;
	d.wkcompleted = s.local.wkcompleted;
	d.ctime = s.global.ctime;
	d.status = s.global.status;
	d.mtime = s.global.mtime;
	d.svridmax = s.global.svridmax;
}
void translate(const sg_scte_t& s, ServiceInfo& d) {
	d.sgid = s.sgid();
	d.serverid = s.svrid();
	d.grpid=s.grpid();
	d.priority = s.parms.priority;
	d.svc_name = s.parms.svc_name;
	d.svc_cname = s.parms.svc_cname;
	d.svc_index = s.parms.svc_index;
	d.svc_load = s.parms.svc_load;
	d.load_limit = s.parms.load_limit;
	d.svctimeout = s.parms.svctimeout;
	d.svcblktime = s.parms.svcblktime;
}
void translate(const sg_scte_t& s, ServiceState& d) {
	d.sgid = s.sgid();
	d.ncompleted = s.local.ncompleted;
	d.nqueued = s.local.nqueued;
	d.status = s.global.status;
}
void translate(const sg_qte_t& s, QueueInfo& d) {
	d.sgid = s.sgid();
	d.options = s.parms.options;
	d.grace = s.parms.grace;
	d.maxgen = s.parms.maxgen;
	d.paddr = s.parms.paddr;
	d.saddr = s.parms.saddr;
	d.rcmd = s.parms.rcmd;
	d.filename = s.parms.filename;
}
void translate(const sg_qte_t& s, QueueState& d) {
	d.sgid = s.sgid();
	d.saddr = s.saddr();
	d.totnqueued = s.local.totnqueued;
	d.nqueued = s.local.nqueued;
	d.totwkqueued = s.local.totwkqueued;
	d.wkqueued = s.local.wkqueued;
	d.status = s.global.status;
	d.svrcnt = s.global.svrcnt;
	d.spawn_state = s.global.spawn_state;
}

OperateTaskPtr buildServerOperateTask(boost::shared_ptr<
		SGAdaptor> & adaptor, const ServerOperate & serverOperate) {

	class ServeTask: public OperateTask {
	public:
		ServeTask(boost::shared_ptr<SGAdaptor> adaptor, ServerOperate serverOperate) :
			adaptor(adaptor), serverOperate(serverOperate) {
		}
		void run() {

			  MonitorLog::write_log((_("INFO: ServeTask run operate task")).str(SGLOCALE));
			switch (serverOperate.type) {
			case OperateType::Server:
				   MonitorLog::write_log((_("INFO: OperateType =Process operate task")).str(SGLOCALE));
				if (serverOperate.operate == Operate::Start) {
					adaptor->startServer(serverOperate.groupId,
							serverOperate.serverId);
				} else if (serverOperate.operate == Operate::Stop) {
					adaptor->stopServer(serverOperate.groupId,
							serverOperate.serverId);
				}
				break;
			case OperateType::ServerGroup:
				if (serverOperate.operate == Operate::Start) {
					adaptor->startServerByGroup(serverOperate.groupId);
				} else if (serverOperate.operate == Operate::Stop) {
					adaptor->stopServerByGroup(serverOperate.groupId);
				}
				break;
			case OperateType::Machine:
				if (serverOperate.operate == Operate::Start) {
					adaptor->startServerByMachine(serverOperate.machineId);
				} else if (serverOperate.operate == Operate::Stop) {
					adaptor->stopServerByMachine(serverOperate.machineId);
				}
				break;
			case OperateType::All:
				if (serverOperate.operate == Operate::Start) {
					adaptor->startServerALL();
				} else if (serverOperate.operate == Operate::Stop) {
					adaptor->stopServerALL();
				}
				break;
			}
		}
	private:
		const boost::shared_ptr<SGAdaptor> adaptor;
		const ServerOperate serverOperate;
	};
	OperateTask * task = new ServeTask(adaptor, serverOperate);
	OperateTaskPtr result(task);
	return result;
}
}
