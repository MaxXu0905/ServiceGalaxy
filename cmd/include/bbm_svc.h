#if !defined(__BB_SVC_H__)
#define __BB_SVC_H__

#include "bbm_ctx.h"

namespace ai
{
namespace sg
{

class bbm_svc : public sg_svc
{
public:
	bbm_svc();
	~bbm_svc();

	svc_fini_t svc(message_pointer& svcinfo);

private:
	void bbclean(int mid, ttypes sect);
	void unpartition(registry_t& reg);
	int nwpsusp(sgid_t sgid, int opcode);
	svc_fini_t prot_fail(message_pointer& svcinfo, message_pointer& fini_msg, sg_rpc_rply_t *rply, int err, int val);
	svc_fini_t svc_fini(message_pointer& svcinfo, message_pointer& fini_msg, int retval);
	void set_error(sg_rpc_rply_t *rply, int error_code);
	int getstat(sgid_t sgid);

	bbm_ctx *BBM;
	bool can_migrate;
};

}
}

#endif

