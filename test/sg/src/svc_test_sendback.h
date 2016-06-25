/*
 * svc_test_sendback.h
 *
 *  Created on: 2012-3-26
 *      Author: renyz
 */

#ifndef SVC_TEST_SENDBACK_H_
#define SVC_TEST_SENDBACK_H_
#include "sg_public.h"
#include "NetMessageClient.h"
namespace ai
{
namespace sg
{
using namespace ai::test;
class svc_test_sendback:public sg_svc{
public:
	svc_test_sendback();
	virtual ~svc_test_sendback();
	svc_fini_t svc(message_pointer& svcinfo);
	private:
		NetMessageClient *  messageClient;
};

}
}
#endif /* SVC_TEST_SENDBACK_H_ */
