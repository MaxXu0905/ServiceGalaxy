#if !defined(__METAREP_SVR_H__)
#define __METAREP_SVR_H__

#include "sg_internal.h"
#include "mrp_ctx.h"

namespace ai
{
namespace sg
{

class metarep_svr: public sg_svr
{
public:
	metarep_svr();
	virtual ~metarep_svr();

protected:
	virtual int svrinit(int argc, char **argv);
	virtual int svrfini();

private:
	bool load();

	mrp_ctx *MRP;
};

}
}

#endif

