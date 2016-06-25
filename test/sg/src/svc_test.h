#if !defined(__SVC_EXAMPLE_H__)
#define __SVC_EXAMPLE_H__

#include "sg_public.h"

namespace ai
{
namespace sg
{

class svc_test : public sg_svc
{
public:
	svc_test();
	~svc_test();

	svc_fini_t svc(message_pointer& svcinfo);

};
}
}

#endif

