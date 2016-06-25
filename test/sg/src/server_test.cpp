/*
 * server_test.cpp
 *
 *  Created on: 2012-3-27
 *      Author: renyz
 */

#include "server_test.h"
#include "NetMessageConfig.h"
namespace ai
{
namespace sg{
server_test::server_test() {
	messageClient = new NetMessageClient();
	// TODO Auto-generated constructor stub

}

server_test::~server_test() {
	delete messageClient;
	// TODO Auto-generated destructor stub
}
int server_test::svrinit(int argc, char **argv) {
	NetMessageConfig::init(argc,argv);
	if (!messageClient->connect()) {
			messageClient->send("serveroverload", "started");
			messageClient->close();
		}
	return 0;
}

int server_test::svrfini() {
	if (!messageClient->connect()) {
			messageClient->send("serveroverload", "stoped");
			messageClient->close();
	}
	return 0;
}

}}
