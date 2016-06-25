#if !defined(__FILE_SVR_H__)
#define __FILE_SVR_H__

#include "sg_svr.h"
#include "sshp_ctx.h"

namespace ai
{
namespace sg
{

class file_svr : public sg_svr
{
public:
	file_svr();
	~file_svr();

protected:
	virtual int svrinit(int argc, char **argv);
	virtual int svrfini();

private:
	bool advertise(int flags);

	sshp_ctx *SSHP;
};

}
}

#endif

