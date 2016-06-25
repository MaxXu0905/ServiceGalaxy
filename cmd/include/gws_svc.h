#if !defined(__GWS_SVC_H__)
#define __GWS_SVC_H__

#include "sg_public.h"

namespace ai
{
namespace sg
{

class gws_svc : public sg_svc
{
public:
	gws_svc();
	~gws_svc();

	svc_fini_t svc(message_pointer& svcinfo);
};

}
}

#endif

