#include "svc_example.h"

using namespace std;

namespace ai
{
namespace sg
{

svc_example::svc_example()
{
}

svc_example::~svc_example()
{
}

svc_fini_t svc_example::svc(message_pointer& svcinfo)
{
	return svc_fini(SGSUCCESS, 0, svcinfo);
}

}
}

