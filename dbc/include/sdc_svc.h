#if !defined(__SDC_SVC_H__)
#define __SDC_SVC_H__

#include "dbc_internal.h"

namespace ai
{
namespace sg
{

class sdc_svc : public sg_svc
{
public:
	sdc_svc();
	~sdc_svc();

	svc_fini_t svc(message_pointer& svcinfo);

private:
	dbc_api& api_mgr;
	dbct_ctx *DBCT;
	message_pointer result_msg;
};

}
}

#endif

