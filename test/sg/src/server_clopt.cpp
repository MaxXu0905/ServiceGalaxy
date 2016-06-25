/*
 * server_clopt.cpp
 *
 *  Created on: 2012-3-27
 *      Author: renyz
 */

#include "server_clopt.h"
#include "NetMessageConfig.h"
namespace ai {
namespace sg {
server_clopt::server_clopt() {
	messageClient = new NetMessageClient();
	// TODO Auto-generated constructor stub

}

server_clopt::~server_clopt() {
	delete messageClient;
	// TODO Auto-generated destructor stub
}
int server_clopt::svrinit(int argc, char **argv) {
	NetMessageConfig::init(argc,argv);
	if (argc > 1) {
		if (!messageClient->connect()) {
			messageClient->send("serverclopt", argv[1]);
			messageClient->close();
		}
	}
	string mess="servercloptaaaaaaaaaaa";
	std::cout<<mess<<std::endl;
	std::cerr<<mess<<std::endl;
	return 0;
}

int server_clopt::svrfini() {
	return 0;
}

}
}
