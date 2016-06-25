#if !defined(__BBM_SERVER_H__)
#define __BBM_SERVER_H__

#include "bbm_ctx.h"

namespace ai
{
namespace sg
{

class bbm_server : public sg_svr
{
public:
	bbm_server();
	~bbm_server();

protected:
	int svrinit(int argc, char **argv);
	int svrfini();
	int run(int flags = 0);

private:
	static void sigterm(int signo);

	bbm_ctx *BBM;
	sg_ste_t dbbmste;
};

}
}

#endif

