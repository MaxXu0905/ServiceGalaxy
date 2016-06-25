/*
 * server_test.h
 *
 *  Created on: 2012-3-27
 *      Author: renyz
 */

#ifndef SERVER_TEST_H_
#define SERVER_TEST_H_

#include "sg_internal.h"
#include "NetMessageClient.h"
namespace ai
{
namespace sg{
using namespace ai::test;
class server_test : public sg_svr
{
public:

	server_test();
	~server_test();

protected:
	int svrinit(int argc, char **argv);
	int svrfini();
	NetMessageClient *  messageClient;
};

}
}
#endif /* SERVER_TEST_H_ */
