#if !defined(__SDC_SQLSVC_H__)
#define __SDC_SQLSVC_H__

#include "dbc_internal.h"
#include "dbc_control.h"

namespace ai
{
namespace sg
{

using namespace ai::scci;

class sdc_sqlsvc : public sg_svc
{
public:
	sdc_sqlsvc();
	~sdc_sqlsvc();

	svc_fini_t svc(message_pointer& svcinfo);

private:
	dbc_api& api_mgr;
	dbct_ctx *DBCT;
	Generic_Database *dbc_db;
};

}
}

#endif

