/*
 * NetMessageClient.cpp
 *
 *  Created on: 2012-3-26
 *      Author: renyz
 */

#include "NetMessageClient.h"
#include <NetMessageConfig.h>
#include "stdlib.h"
#include "boost/shared_ptr.hpp"
#include <protocol/TProtocol.h>
#include <transport/TSocket.h>
#include <transport/TTransport.h>
#include <protocol/TBinaryProtocol.h>
#include <transport/TBufferTransports.h>
#include <Message.h>

namespace ai {
namespace test {
using namespace apache::thrift::transport;
using namespace apache::thrift::protocol;
NetMessageClient::NetMessageClient() :
	messageClient(0) {
}
NetMessageClient::~NetMessageClient() {
	close();
}
int NetMessageClient::connect() {
	try {
		boost::shared_ptr<TSocket> socket(new TSocket(NetMessageConfig::address,NetMessageConfig::port));
		boost::shared_ptr<TTransport>* transport2=new boost::shared_ptr<TTransport>(new TBufferedTransport(socket));
		transport=(void *)transport2;
		boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(*transport2));
		transport2->get()->open();
		messageClient = new MessageClient(protocol);
	} catch (TTransportException &e) {
		return -1;
	}
	return 0;
}

int NetMessageClient::send(const string &key,const string &msg) {
	return messageClient->send(key,msg);
}
int NetMessageClient::sendadd(const string &key,const string &msg) {
	return messageClient->sendadd(key,msg);
}
int NetMessageClient::close() {
	if (messageClient != 0) {
		(*((boost::shared_ptr<TTransport> *)transport))->flush();
		(*((boost::shared_ptr<TTransport> *)transport))->close();
		delete messageClient;
		delete ((boost::shared_ptr<TTransport> *)transport);
		messageClient = 0;
	}
	return 0;
}
}
}
