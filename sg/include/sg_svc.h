#if !defined(__SG_SVC_H__)
#define __SG_SVC_H__

#include <string>
#include "sg_message.h"
#include "sg_struct.h"

namespace ai
{
namespace sg
{

int const SGFAIL = 0x00000001;			// service FAILURE for svc_fini
int const SGSUCCESS = 0x00000002;		// service SUCCESS for svc_fini
int const SGEXIT = 0x08000000;			// service FAILURE with server exit
int const SGBREAK = 0x01000000;			// svc_fini - break out of loop

class svc_fini_t
{
private:
	svc_fini_t() {}

	friend class sg_svc;
	friend class bbm_svc;
};

class sg_svc
{
public:
	sg_svc();
	virtual ~sg_svc();

	virtual svc_fini_t svc(message_pointer& svcinfo) = 0;
	svc_fini_t forward(const char *service, message_pointer& msg, int flags);
	svc_fini_t svc_fini(int rval, int urcode, message_pointer& fini_msg);

protected:
	gpp_ctx *GPP;
	sgp_ctx *SGP;
	sgt_ctx *SGT;

	FRIEND_CLASS_DECLARE
};

}
}

#endif

