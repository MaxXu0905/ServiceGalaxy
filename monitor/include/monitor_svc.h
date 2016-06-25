/*
 * monitor_svc.h
 * ø’service  µœ÷
 *  Created on: 2012-2-24
 *      Author: renyz
 */

#ifndef MONITOR_SVC_H_
#define MONITOR_SVC_H_
namespace ai
{
namespace sg
{
class monitor_svc : public sg_svc
{
public:
	monitor_svc(){}
	~monitor_svc(){}

	svc_fini_t svc(message_pointer& svcinfo){
		return svc_fini(SGSUCCESS, 0, svcinfo);
	}
};
}
}

#endif /* MONITOR_SVC_H_ */
