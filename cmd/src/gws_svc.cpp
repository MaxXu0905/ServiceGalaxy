#include "gws_svc.h"

using namespace std;

namespace ai
{
namespace sg
{

gws_svc::gws_svc()
{
}

gws_svc::~gws_svc()
{
}

svc_fini_t gws_svc::svc(message_pointer& svcinfo)
{
	return svc_fini(SGSUCCESS, 0, svcinfo);
}

}
}

