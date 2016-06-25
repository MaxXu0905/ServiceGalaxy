#if !defined(__DBC_MSVR_H__)
#define __DBC_MSVR_H__

#include "dbc_internal.h"

namespace ai
{
namespace sg
{

class dbc_msvr: public sg_svr
{
public:
	dbc_msvr();
	virtual ~dbc_msvr();

protected:
	virtual int svrinit(int argc, char **argv);
	virtual int svrfini();
	virtual int run(int flags = 0);

	dbcp_ctx *DBCP;
	dbct_ctx *DBCT;
	dbc_server svr_mgr;
	bool immediate;
	int cleantime;
	time_t start_time;
};

}
}

#endif

