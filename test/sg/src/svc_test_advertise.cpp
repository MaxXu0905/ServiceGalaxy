/*
 * svc_test_advertise.cpp
 *
 *  Created on: 2012-3-26
 *      Author: renyz
 */

#include "svc_test_advertise.h"
#include "boost/token_functions.hpp"
#include "boost/tokenizer.hpp"

namespace ai {
namespace sg {
svc_test_advertise::svc_test_advertise() {
}
svc_test_advertise::~svc_test_advertise() {

}
svc_fini_t svc_test_advertise::svc(message_pointer& svcinfo) {
	try {
		boost::char_separator<char> sep(":");
		sg_api &sg_mgr = sg_api::instance(SGT);
		string inputcmd;
		inputcmd.assign(svcinfo->data(), svcinfo->data() + svcinfo->length());
		tokenizer tokens(inputcmd, sep);
		vector<string> token_vector;
		for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end();
				++iter) {
			token_vector.push_back(*iter);
		}
		if (token_vector.empty()) {
			return svc_fini(SGSUCCESS, 0, svcinfo);
		}
		string cmd = token_vector[0];
		if ("advertise" == cmd && token_vector.size() == 3) {
			if (!sg_mgr.advertise(token_vector[1], token_vector[2])) {
				return svc_fini(SGFAIL, -1, svcinfo);
			}
		} else if ("unadvertise" == cmd && token_vector.size() == 2) {
			if (!sg_mgr.unadvertise(token_vector[1])) {
				return svc_fini(SGFAIL, -1, svcinfo);
			}
		}else if("svc_fini"== cmd && token_vector.size() == 2){
			string rettype=token_vector[1];
			if("SGSUCCESS"==rettype){
				return svc_fini(SGSUCCESS, 77, svcinfo);
			}else if("SGFAIL"==rettype){
				return svc_fini(SGFAIL, 88, svcinfo);
			}else if("SGEXIT"==rettype){
				return svc_fini(SGEXIT, 99, svcinfo);
			}
		}else if("forward"==cmd && token_vector.size() == 3){
			message_pointer retmsg=sg_message::create();
			retmsg->assign(token_vector[2].c_str());
			return forward(token_vector[1].c_str(),retmsg,0);
		}else if("sleep"==cmd &&token_vector.size() == 2){
			sleep(atoi(token_vector[1].c_str()));
		}
	} catch (exception &ex) {
		std::cout << ex.what() << std::endl;
	} catch (...) {

	}
	return svc_fini(SGSUCCESS, 0, svcinfo);
}

}
}
