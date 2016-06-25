/*
 * server_ulog.cpp
 *
 *  Created on: 2012-3-27
 *      Author: renyz
 */

#include "server_ulog.h"
#include "NetMessageConfig.h"
namespace ai {
namespace sg {
using namespace ai::test;
server_ulog::server_ulog() {
	// TODO Auto-generated constructor stub

}

server_ulog::~server_ulog() {
	// TODO Auto-generated destructor stub
}
int server_ulog::svrinit(int argc, char **argv) {
	NetMessageConfig::init(argc,argv);
	GPP->write_log("ulogtestaaaaaaaaaaaaaaaaaaaaa");
	return 0;
}

int server_ulog::svrfini() {
	return 0;
}

}
}
