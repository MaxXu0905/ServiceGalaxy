#if !defined(__DBBM_SERVER_H__)
#define __DBBM_SERVER_H__

#include "sg_internal.h"
#include "bbm_ctx.h"

namespace ai
{
namespace sg
{

class dbbm_server : public sg_svr
{
public:
	dbbm_server();
	~dbbm_server();

protected:
	int svrinit(int argc, char **argv);
	int svrfini();
	int run(int flags = 0);

private:
	void bbchk();

	static void sigterm(int signo);

	bbm_ctx *BBM;

	message_pointer rqst_msg;
	message_pointer rply_msg;
};

}
}

#endif


