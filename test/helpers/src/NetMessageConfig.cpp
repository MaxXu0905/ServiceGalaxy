/*
 * NetMessageConfig.cpp
 *
 *  Created on: 2012-5-28
 *      Author: Administrator
 */

#include "NetMessageConfig.h"
#include <boost/program_options.hpp>

namespace ai {
namespace test {

void  NetMessageConfig::init(int argc, char **argv){
	using namespace std;
	namespace po = boost::program_options;
		po::options_description desc((boost::format("Allowed options")).str(SGLOCALE));
		desc.add_options()("address,a",po::value<string>(&address)->default_value("10.1.247.2"),"address")
				("port,p",
				po::value<int>(&port)->default_value(33001),
				"port default(33001)");
		po::variables_map vm;
		try {
			po::store(
					po::command_line_parser(argc, argv) .options(desc).allow_unregistered() .run(),
					vm);
			po::notify(vm);
		} catch (exception& ex) {
			std::cout << ex.what() << std::endl;
			std::cout << desc << std::endl;
		}
}
std::string NetMessageConfig::address;
int NetMessageConfig::port;
} /* namespace test */
} /* namespace ai */
