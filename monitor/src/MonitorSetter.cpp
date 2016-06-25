/*
 * MonitorSetter.cpp
 *
 *  Created on: 2012-2-23
 *      Author: renyz
 */
#include "machine.h"
#include "MonitorSetter.h"
#include "Config.h"
namespace {
inline int pro_args(int & port, string & transport_type,
		string & protocol_type, string & server_type, size_t & workers,
		int argc, char ** argv) {
	namespace po = boost::program_options;
	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help,h", (_("produce help message")).str(SGLOCALE).c_str())
		("port", po::value<int>(&port)->default_value(port), (_("set thrift listen port default(40001)")).str(SGLOCALE).c_str())
		("transport_type", po::value<string>(&transport_type), (_("set transport_type framed, buffered default(buffered)")).str(SGLOCALE).c_str())
		("protocol_type", po::value<string>(&protocol_type), (_("set protocol_type json, binary defalut(binary)")).str(SGLOCALE).c_str())
		("server_type",	po::value<string>(&server_type), (_("set process_type: simple, threaded, thread-pool, nonblocking default(simple)")).str(SGLOCALE).c_str())
		("workers", po::value<size_t>(&workers), (_("set workers: default(16)")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;
	try {
		po::store(
				po::command_line_parser(argc, argv) .options(desc).allow_unregistered() .run(),
				vm);
		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return 0;
		}
		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		return 1;
	}
	return 0;
}
}

namespace ai {
namespace sg {
const int MonitorSetter::port = 40001;
const string MonitorSetter::transport_type = "buffered";
const string MonitorSetter::protocol_type = "binary";
const string MonitorSetter::server_type = "thread-pool";
const size_t MonitorSetter::workers = 16;
class DefaultMonitorSetter: public MonitorSetter {
	int set(Monitor & monitor) {
		monitor.setPort(port) .setTransport_type(transport_type) .setProtocol_type(
				protocol_type) .setServer_type(server_type) .setWorkers(workers);
		return 0;
	}
};

class ArgsMoitotrSetter: public MonitorSetter {
public:
	ArgsMoitotrSetter(int argc, char** argv);
	~ArgsMoitotrSetter();
	int set(Monitor & monitor);
private:
	int argc;
	char * * argv;

};

class ConfigMonitorSetter: public MonitorSetter {
public:
	ConfigMonitorSetter(string filename) :
		filename(filename) {
	}
	;
	~ConfigMonitorSetter() {
	}
	;
	int set(Monitor & monitor);
private:
	string filename;
};

MonitorSetter::MonitorSetter() {
}
MonitorSetter::~MonitorSetter() {
}
boost::shared_ptr<MonitorSetter> MonitorSetter::factory() {
	boost::shared_ptr<MonitorSetter> result(new DefaultMonitorSetter());
	return result;
}
boost::shared_ptr<MonitorSetter> MonitorSetter::factory(int argc, char** argv) {
	boost::shared_ptr<MonitorSetter> result(new ArgsMoitotrSetter(argc, argv));
	return result;
}
boost::shared_ptr<MonitorSetter> MonitorSetter::factory(string filename) {
	boost::shared_ptr<MonitorSetter> result(new ConfigMonitorSetter(filename));
	return result;
}

ArgsMoitotrSetter::ArgsMoitotrSetter(int argc, char** argv) :
	argc(argc), argv(argv) {
}
ArgsMoitotrSetter::~ArgsMoitotrSetter() {
}
int ArgsMoitotrSetter::set(Monitor & monitor) {
	int port = monitor.getPort();
	string transport_type = monitor.getTransport_type();
	string protocol_type = monitor.getProtocol_type();
	string server_type = monitor.getServer_type();
	size_t workers = monitor.getWorkers();
	//处理参数
	if (pro_args(port, transport_type, protocol_type, server_type, workers,
			argc, argv)) {
		//参数解析失败，直接返回
		return -1;
	}
	monitor.setPort(port) .setTransport_type(transport_type) .setProtocol_type(
			protocol_type) .setServer_type(server_type) .setWorkers(workers);
	return 0;

}
int ConfigMonitorSetter::set(Monitor & monitor) {
	try {
		Config configSettings(filename);

		int port = monitor.getPort();
		string transport_type = monitor.getTransport_type();
		string protocol_type = monitor.getProtocol_type();
		string server_type = monitor.getServer_type();
		size_t workers = monitor.getWorkers();
		monitor.setPort(configSettings.Read("port", port)) .setTransport_type(
				configSettings.Read("transport_type", transport_type)) .setProtocol_type(
				configSettings.Read("protocol_type", protocol_type)) .setServer_type(
				configSettings.Read("server_type", server_type)) .setWorkers(
				configSettings.Read("workers", workers));

	} catch (Config::File_not_found & e) {
		return -1;
	}
	return 0;
}

}
}

