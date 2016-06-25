#include "svc_test_coredump.h"
#include "NetMessageClient.h"
using namespace std;

namespace ai {
namespace sg {

svc_test_coredump::svc_test_coredump() {

}

svc_test_coredump::~svc_test_coredump() {
}

svc_fini_t svc_test_coredump::svc(message_pointer& svcinfo) {
	abort();
	return svc_fini(SGSUCCESS, 0, svcinfo);
}

}
}
