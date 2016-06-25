/*
 * svc_test_sendback.cpp
 *
 *  Created on: 2012-3-26
 *      Author: renyz
 */

#include "svc_test_sendback.h"

namespace ai {
namespace sg {
using namespace ai::test;
svc_test_sendback::svc_test_sendback() {
	messageClient = new NetMessageClient();
}
svc_test_sendback::~svc_test_sendback() {
	delete messageClient;
}
svc_fini_t svc_test_sendback::svc(message_pointer& svcinfo) {
	try {
		if (!messageClient->connect()) {
			string sendback;
			sendback.assign(svcinfo->data(), svcinfo->data()
					+ svcinfo->length());
			messageClient->send("servicecallback", sendback);
			messageClient->close();
		}
	} catch (exception &ex) {
		std::cout << ex.what() << std::endl;
	} catch (...) {

	}
	return svc_fini(SGSUCCESS, 0, svcinfo);
}

}
}
