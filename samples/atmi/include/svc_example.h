#if !defined(__SVC_EXAMPLE_H__)
#define __SVC_EXAMPLE_H__

#include "sg_public.h"

namespace ai
{
namespace sg
{

class svc_example : public sg_svc
{
public:
	svc_example();
	~svc_example();

	svc_fini_t svc(message_pointer& svcinfo);
};

}
}

#endif

