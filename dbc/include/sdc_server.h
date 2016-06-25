#if !defined(__SDC_SERVER_H__)
#define __SDC_SERVER_H__

#include "dbc_internal.h"

namespace ai
{
namespace sg
{

class sdc_server : public sg_svr
{
public:
	sdc_server();
	~sdc_server();

protected:
	virtual int svrinit(int argc, char **argv);
	virtual int svrfini();

private:
	bool advertise();

	dbcp_ctx *DBCP;
	dbct_ctx *DBCT;

	std::vector<std::string> table_names;
};

}
}

#endif

