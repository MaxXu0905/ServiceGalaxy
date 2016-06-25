/*
 * Monitor.cpp
 *
 *  Created on: 2012-2-15
 *      Author: renyz
 */

#include "monitorpch.h"
#include "SGMonitor.h"
#include "ServerManager.h"
#include "MonitorHandler.h"
#include "Monitor.h"
#include "TaskManager.h"
using namespace boost;

namespace ai {
namespace sg {
int Monitor::run() {
	using namespace apache::thrift;
	if (serverManager.get()) {
		return -1;
	}
	MonitorLog::write_log((_("INFO: Startup thrift gatetway")).str(SGLOCALE));
	if (serverManager.get()) {
		return -1;
	}
	MonitorLog::write_log((_("INFO: Initialize start")).str(SGLOCALE));
	boost::shared_ptr<MonitorHandler> monitorHandler(new MonitorHandler());
	boost::shared_ptr<MonitorProcessor> monitorProcessor(new MonitorProcessor(
			monitorHandler));
	serverManager.reset(new ServerManager(monitorProcessor, port,
			transport_type, protocol_type, server_type, workers));
	MonitorLog::write_log((_("INFO: Initialize finish")).str(SGLOCALE));
	try{
	serverManager->run();
	}catch(TException e){
		MonitorLog::write_log(e.what());
		running=false;
		return -1;
	}catch (exception e){
		MonitorLog::write_log(e.what());
		running=false;
		return -1;
	}
	running=true;
	return 0;
}

int Monitor::stop() {
	MonitorLog::write_log((_("INFO: ready to stop")).str(SGLOCALE));
	TaskManager::instance().stop();
	if (serverManager.get()) {
		serverManager->stop();
		running=false;
		return 0;
	}
	running=false;
	return -1;
}

}
}
