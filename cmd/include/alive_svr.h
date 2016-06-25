#if !defined(__ALIVE_SVR_H__)
#define __ALIVE_SVR_H__

#include "sg_svr.h"
#include "sshp_ctx.h"

namespace ai
{
namespace sg
{

class alive_svr : public sg_svr
{
public:
	alive_svr();
	~alive_svr();

protected:
	virtual int svrinit(int argc, char **argv);
	virtual int svrfini();

private:
	sshp_ctx *SSHP;
};

}
}

#endif

