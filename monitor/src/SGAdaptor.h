/*
 * SGAdaptor.h
 *
 *  Created on: 2012-2-15
 *      Author: renyz
 *      SG平台适配器，封装SG平以相关的操作
 */

#ifndef SGADAPTOR_H_
#define SGADAPTOR_H_
#undef roundup
#include <utility>
#include "sg_struct.h"
namespace ai {
namespace sg {

class SGAdaptor {

public:
	typedef boost::shared_array<sg_qte_t> Tsg_qte_t;
	typedef boost::shared_array<sg_sgte_t> Tsg_sgte_t;
	typedef boost::shared_array<sg_ste_t> Tsg_ste_t;
	typedef boost::shared_array<sg_scte_t> Tsg_scte_t;
	SGAdaptor();
	virtual ~SGAdaptor();
	static boost::shared_ptr<SGAdaptor> create();

	virtual bool enter() = 0;
	virtual bool leave() = 0;
	virtual sg_bboard_t& get_sg_bboard_t()=0;
	//主机信息数组
	virtual boost::shared_ptr<std::vector<sg_mchent_t *> > get_sg_mchent_t()=0;
	//主机监听数组
	virtual boost::shared_ptr<std::vector<sg_netent_t *> > get_sg_netent_t(const char *lmid)=0;
	//队列
	virtual std::pair<int, Tsg_qte_t> get_sg_qte_t(int rlink = -1)=0;
	//Server Group
	virtual std::pair<int, Tsg_sgte_t> get_sg_sgte_t()=0;
	//Server
	virtual std::pair<int, Tsg_ste_t> get_sg_ste_t()=0;
	//Service
	virtual std::pair<int, Tsg_scte_t> get_sg_scte_t()=0;
	virtual const char* mid2lmid(int mid)=0;

	virtual int32_t startServer(int32_t groupId, int32_t serverId)=0;
	virtual int32_t stopServer(int32_t groupId, int32_t serverId)=0;
	virtual int32_t startServerByGroup(int32_t groupId)=0;
	virtual int32_t stopServerByGroup(int32_t groupId)=0;
	virtual int32_t startServerByMachine(string machineId)=0;
	virtual int32_t stopServerByMachine(string machineId)=0;
	virtual int32_t startServerALL()=0;
	virtual int32_t stopServerALL()=0;

	virtual int32_t resumeServer(const int64_t sgid) = 0;
	virtual int32_t suspendServer(const int64_t sgid) = 0;

	virtual int32_t resumeQueue(const int64_t sgid) = 0;
	virtual int32_t suspendQueue(const int64_t sgid) = 0;

	virtual int32_t resumeService(const int64_t sgid) = 0;
	virtual int32_t suspendService(const int64_t sgid) = 0;

};

}

}

#endif /* SGADAPTOR_H_ */
