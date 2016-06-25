/*
 * baseservertest.cpp
 *
 *  Created on: 2012-5-28
 *      Author: Administrator
 */

#include "base_server_test.h"
#include "NetMessageConfig.h"
namespace ai {
namespace sg{
using namespace ai::test;
int base_server_test::svrinit(int argc, char **argv) {
	NetMessageConfig::init(argc,argv);
	return 0;
}


} /* namespace test */
} /* namespace ai */
