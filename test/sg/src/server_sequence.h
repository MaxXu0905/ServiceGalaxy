/*
 * server_sequence.h
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
class server_sequence : public sg_svr
{
public:

	server_sequence();
	~server_sequence();

protected:
	int svrinit(int argc, char **argv);
	int svrfini();
	NetMessageClient *  messageClient;
private:
	string seq;
};

}
}
#endif /* SERVER_TEST_H_ */
