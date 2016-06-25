/*
 * server_clopt.h
 *
 *  Created on: 2012-3-27
 *      Author: renyz
 */

#ifndef server_clopt_H_
#define server_clopt_H_

#include "sg_internal.h"
#include "NetMessageClient.h"
namespace ai
{
namespace sg{
using namespace ai::test;
class server_clopt : public sg_svr
{
public:

	server_clopt();
	~server_clopt();

protected:
	int svrinit(int argc, char **argv);
	int svrfini();
	NetMessageClient *  messageClient;
};

}
}
#endif /* server_clopt_H_ */
