#if !defined(__DBC_MSVC_H__)
#define __DBC_MSVC_H__

#include "dbc_internal.h"

namespace ai
{
namespace sg
{

class dbc_msvc : public sg_svc
{
public:
	dbc_msvc();
	~dbc_msvc();

	svc_fini_t svc(message_pointer& svcinfo);

private:
	dbct_ctx *DBCT;
	dbc_api& api_mgr;
};

}
}

#endif

