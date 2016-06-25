/*
 * NetMessageConfig.h
 *
 *  Created on: 2012-5-28
 *      Author: Administrator
 */

#ifndef NETMESSAGECONFIG_H_
#define NETMESSAGECONFIG_H_
#include <string>
namespace ai {
namespace test {
struct NetMessageConfig {
	static void init(int argc, char **argv);
	static std::string address;
	static int port;
};

} /* namespace test */
} /* namespace ai */
#endif /* NETMESSAGECONFIG_H_ */
