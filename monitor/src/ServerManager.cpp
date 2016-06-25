/*
 * ServerManager.cpp
 *
 *  Created on: 2012-2-15
 *      Author: renyz
 */

#include "machine.h"
#include "ServerManager.h"
#include <protocol/TProtocol.h>
#include <transport/TServerSocket.h>
#include <transport/TTransport.h>
#include <transport/TBufferTransports.h>
#include <concurrency/PlatformThreadFactory.h>
using namespace std;
using namespace apache::thrift;
using boost::shared_ptr;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using apache::thrift::concurrency::PlatformThreadFactory;
namespace ai {
namespace sg {

ServerManager::ServerManager(const boost::shared_ptr<TProcessor>& processor,
		int port, string transport_type, string protocol_type,
		string server_type, size_t workers) :
	processor(processor), port(port), transport_type(transport_type),
			protocol_type(protocol_type), server_type(server_type), workers(
					workers) {
}
ServerManager::~ServerManager() {
	stop();
}
int ServerManager::run() {
	if (server.get()) {
		return -1;
	}
	shared_ptr<TProtocolFactory> protocolFactory;
	shared_ptr<TServerSocket> serverSocket;
	shared_ptr<TTransportFactory> transportFactory;
	serverSocket = shared_ptr<TServerSocket> (new TServerSocket(port));
	string info_msg = (_("INFO: server_type={1}, protocol_type={2}, transport_type={3}, port={4}")
		% server_type
		% protocol_type
		% transport_type
		% port).str(SGLOCALE);
	if (protocol_type == "json") {
		shared_ptr<TProtocolFactory> jsonProtocolFactory(
				new TJSONProtocolFactory());
		protocolFactory = jsonProtocolFactory;
	} else {
		shared_ptr<TProtocolFactory> binaryProtocolFactory(
				new TBinaryProtocolFactoryT<TBufferBase> ());
		protocolFactory = binaryProtocolFactory;
	}
	if (transport_type == "framed") {
		shared_ptr<TTransportFactory> framedTransportFactory(
				new TFramedTransportFactory());
		transportFactory = framedTransportFactory;
	} else {
		shared_ptr<TTransportFactory> bufferedTransportFactory(
				new TBufferedTransportFactory());
		transportFactory = bufferedTransportFactory;

	}

	if (server_type == "simple") {
		server.reset(new TSimpleServer(this->processor, serverSocket,
				transportFactory, protocolFactory));
	} else if (server_type == "thread-pool" || server_type == "nonblocking") {
		info_msg += ((_(", workers={1}") % workers).str(SGLOCALE));
		threadManager = ThreadManager::newSimpleThreadManager(workers);
		shared_ptr<PlatformThreadFactory> threadFactory = shared_ptr<
				PlatformThreadFactory> (new PlatformThreadFactory());
		threadManager->threadFactory(threadFactory);
		threadManager->start();
		if (server_type == "thread-pool") {
			server.reset(new TThreadPoolServer(this->processor, serverSocket,
					transportFactory, protocolFactory, threadManager));
		} else {
			server.reset(new TNonblockingServer(this->processor,
					 protocolFactory,
					 port, threadManager));
		}
	} else if (server_type == "threaded") {
		server.reset(new TThreadedServer(this->processor, serverSocket,
				transportFactory, protocolFactory));
	} else if (server_type == "nonblocking") {

	} else {
		server.reset(new TSimpleServer(this->processor, serverSocket,
				transportFactory, protocolFactory));
	}
	MonitorLog::write_log(info_msg);
	server->run();
	return 0;
}
int ServerManager::stop() {
	if (server.get()) {
		server->stop();
		if (threadManager.get()) {
			threadManager->stop();
		}
		return 0;
	}
	return -1;

}
} /* namespace sg */
} /* namespace ai */
