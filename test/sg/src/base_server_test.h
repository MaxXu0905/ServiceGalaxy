/*
 * baseservertest.h
 *
 *  Created on: 2012-5-28
 *      Author: Administrator
 */

#ifndef BASESERVERTEST_H_
#define BASESERVERTEST_H_
#include "sg_internal.h"
namespace ai {
namespace sg{

class base_server_test : public sg_svr{
protected:
	int svrinit(int argc, char **argv);
};

} /* namespace test */
} /* namespace ai */
#endif /* BASESERVERTEST_H_ */
