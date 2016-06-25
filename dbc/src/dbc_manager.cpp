#include "dbc_internal.h"

namespace ai
{
namespace sg
{

dbc_manager::dbc_manager()
{
	DBCP = dbcp_ctx::instance();
	DBCT = dbct_ctx::instance();
}

dbc_manager::~dbc_manager()
{
}

}
}

