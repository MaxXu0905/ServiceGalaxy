/*
 * MonitorHandler.h
 *Thrift Hander接口实现类
 *  Created on: 2012-2-16
 *      Author: renyz
 */

#ifndef MONITORHANDLER_H_
#define MONITORHANDLER_H_
#include "monitorpch.h"
#include "Monitor.h"
#include "SGAdaptor.h"
namespace ai {
namespace sg {

class MonitorHandler: virtual public MonitorIf {
private:
	boost::shared_ptr<SGAdaptor> adaptor;
public:
	MonitorHandler() :
		adaptor(SGAdaptor::create()) {
		// Your initialization goes here
	}
	void GetClusterInfo(ClusterInfo& clusterInfo);
	void GetClusterState(ClusterState& _return);
	void GetMachineInfoList(std::vector<MachineInfo> & _return);
	BBVERSION getBBVersion();
	void GetServerGroupList(std::vector<ServerGroup> & _return);
	void GetServerInfoList(std::vector<ServerInfo> & _return);
	void GetServerStateList(std::vector<ServerState> & _return);
	void GetServiceInfoList(std::vector<ServiceInfo> & _return);
	void GetServiceStateList(std::vector<ServiceState> & _return);
	void GetQueueInfoList(std::vector<QueueInfo> & _return);
	void GetQueueStateList(std::vector<QueueState> & _return);
	void getAllInfo(AllInfo& _return, const BBVERSION bbversion);
	int32_t startOrStopServer(const std::vector<ServerOperate> & operates);
	int32_t updateServer(const Operate::type operate,
			const std::vector<int64_t> & sgids);
	int32_t updateQueue(const Operate::type operate,
			const std::vector<int64_t> & sgids);
	int32_t updateService(const Operate::type operate, const std::vector<
			int64_t> & sgids);

private:
	void setServerGroup(const sg_sgte_t* s, ServerGroup * d);
	void setServerInfo(const sg_ste_t * s, ServerInfo * d);
};

}

}

#endif /* MONITORHANDLER_H_ */
