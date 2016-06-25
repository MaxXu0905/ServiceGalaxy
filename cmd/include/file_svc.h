#if !defined(__FILE_SVC_H__)
#define __FILE_SVC_H__

#include "sg_internal.h"
#include "sshp_ctx.h"
#include "sftp.h"

namespace ai
{
namespace sg
{

class file_svc : public sg_svc
{
public:
	file_svc();
	~file_svc();

	svc_fini_t svc(message_pointer& svcinfo);

private:
	sshp_ctx *SSHP;
	message_pointer result_msg;
};

}
}

#endif

