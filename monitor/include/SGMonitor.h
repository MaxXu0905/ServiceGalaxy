/*
 * Monitor.h
 * ServerManager 的前置包装类，简化了ServerManager的配置
 *  Created on: 2012-2-15
 *      Author: renyz
 */

#ifndef SGMONITOR_H_
#define SGMONITOR_H_
#include "monitorpch.h"
using namespace std;
namespace ai {
namespace sg {
class ServerManager;
class Monitor {
public:
	Monitor():running(true) {}
	virtual ~Monitor() {}
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

	Monitor&  setPort(int port) {
		this->port = port;
		return *this;
	}

	Monitor&  setProtocol_type(string protocol_type) {
		this->protocol_type = protocol_type;
		return *this;
	}

	Monitor& setServer_type(string server_type) {
		this->server_type = server_type;
		return *this;
	}

	Monitor& setTransport_type(string transport_type) {
		this->transport_type = transport_type;
		return *this;
	}

	Monitor& setWorkers(size_t workers) {
		this->workers = workers;
		return *this;
	}
	bool isRunning(){return running;};
	int run();
	int stop();

private:
	int port;
	string transport_type;
	string protocol_type;
	string server_type;
	size_t workers;
	bool running;
	boost::shared_ptr<ServerManager> serverManager;
};
}
}
#endif /* MONITOR_H_ */
