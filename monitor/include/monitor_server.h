#if !defined(__GW_SERVER_H__)
#define __GW_SERVER_H__
/*
 * SG server 实现类，起动TGWS
 */

#include "SGMonitor.h"
#include "sg_internal.h"
namespace ai
{
namespace sg
{

class monitor_server : public sg_svr
{
public:
	monitor_server();
	~monitor_server();

protected:
	int svrinit(int argc, char **argv);
	int svrfini();
	int run(int flags = 0);
private:
	Monitor monitor;
	boost::thread_group _thread_group;
};

}
}


#endif

