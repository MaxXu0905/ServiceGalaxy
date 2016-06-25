/*
 * ServerManager.h
 *Thrift �ӿ�ʵ����
 *  Created on: 2012-2-15
 *      Author: renyz
 */

#ifndef SERVERMANAGER_H_
#define SERVERMANAGER_H_
#include <boost/shared_ptr.hpp>
#include <TProcessor.h>
#include <server/TServer.h>
#include <concurrency/ThreadManager.h>

using apache::thrift::TProcessor;
using apache::thrift::server::TServer;
using apache::thrift::concurrency::ThreadManager;
namespace ai {
namespace test {
using namespace std;
class ServerManager {
public:
	ServerManager(const boost::shared_ptr<TProcessor>& processor, int port,
			string transport_type, string protocol_type
			, string server_type , size_t workers);
	virtual ~ServerManager();
	int run();
	int stop();
	bool isrun();
	int getPort() const {
		return port;
	}
	string getProtocol_type() const {
		return protocol_type;
	}
	string getServer_type() const {
		return server_type;
	}
	string getTransport_type() const {
		return transport_type;
	}
	size_t getWorkers() const {
		return workers;
	}
private:
	boost::shared_ptr<TProcessor> processor;
	boost::shared_ptr<TServer> server;
	boost::shared_ptr<ThreadManager> threadManager;
	int port;
	string transport_type;
	string protocol_type;
	string server_type;
	size_t workers;

};

} /* namespace sg */
} /* namespace ai */
#endif /* SERVERMANAGER_H_ */
