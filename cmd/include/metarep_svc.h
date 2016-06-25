#if !defined(__METAREP_SVC_H__)
#define __METAREP_SVC_H__

#include "sg_public.h"
#include "mrp_ctx.h"

namespace ai
{
namespace sg
{

class metarep_svc : public sg_svc
{
public:
	metarep_svc();
	~metarep_svc();

	svc_fini_t svc(message_pointer& svcinfo);

private:
	mrp_ctx *MRP;
};

}
}

#endif

