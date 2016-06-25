/*
 * server_ulog.h
 *
 *  Created on: 2012-3-27
 *      Author: renyz
 */

#ifndef server_ulog_H_
#define server_ulog_H_

#include "sg_internal.h"
namespace ai
{
namespace sg{
class server_ulog : public sg_svr
{
public:

	server_ulog();
	~server_ulog();

protected:
	int svrinit(int argc, char **argv);
	int svrfini();
};

}
}
#endif /* server_ulog_H_ */
