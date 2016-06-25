/*
 * server_sequence.cpp
 *
 *  Created on: 2012-3-27
 *      Author: renyz
 */

#include "server_sequence.h"
#include "NetMessageConfig.h"
namespace ai
{
namespace sg{
server_sequence::server_sequence() {
	messageClient = new NetMessageClient();
	// TODO Auto-generated constructor stub

}

server_sequence::~server_sequence() {
	delete messageClient;
	// TODO Auto-generated destructor stub
}
int server_sequence::svrinit(int argc, char **argv) {
	NetMessageConfig::init(argc,argv);
	ostringstream stream;
	stream << atoi(argv[0]);
	seq=stream.str();
	if (!messageClient->connect()) {
			messageClient->sendadd("serversequence", seq);
			messageClient->close();
	}
	return 0;
}
int server_sequence::svrfini() {
	if (!messageClient->connect()) {
			messageClient->sendadd("serversequence", seq);
			messageClient->close();
	}
	return 0;
}

}}
