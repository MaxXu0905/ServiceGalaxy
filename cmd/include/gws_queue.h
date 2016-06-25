#if !defined(__GWS_QUEUE_H__)
#define __GWS_QUEUE_H__

#include "sg_internal.h"
#include "gwp_ctx.h"

namespace ai
{
namespace sg
{

class gws_queue : public sg_svr
{
public:
	gws_queue();
	~gws_queue();

	static void * static_run(void *arg);

private:
	int run(int flags = 0);

	gwp_ctx *GWP;
};

}
}

#endif

