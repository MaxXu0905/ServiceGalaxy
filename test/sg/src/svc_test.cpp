#include "svc_test.h"
#include "NetMessageClient.h"
using namespace std;

namespace ai {
namespace sg {

svc_test::svc_test() {

}

svc_test::~svc_test() {
}

svc_fini_t svc_test::svc(message_pointer& svcinfo) {
	return svc_fini(SGSUCCESS, 0, svcinfo);
}

}
}
