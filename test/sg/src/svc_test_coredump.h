#if !defined(__SVC_COREDUMP_H__)
#define __SVC_COREDUMP_H__

#include "sg_public.h"

namespace ai
{
namespace sg
{

class svc_test_coredump : public sg_svc
{
public:
	svc_test_coredump();
	~svc_test_coredump();

	svc_fini_t svc(message_pointer& svcinfo);

};
}
}

#endif

