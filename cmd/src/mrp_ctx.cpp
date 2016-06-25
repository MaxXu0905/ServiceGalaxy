#include "mrp_ctx.h"

namespace ai
{
namespace sg
{

boost::once_flag mrp_ctx::once_flag = BOOST_ONCE_INIT;
mrp_ctx * mrp_ctx::_instance = NULL;

mrp_ctx * mrp_ctx::instance()
{
	if (_instance == NULL)
		boost::call_once(once_flag, mrp_ctx::init_once);
	return _instance;
}

mrp_ctx::mrp_ctx()
{
}

mrp_ctx::~mrp_ctx()
{
}

void mrp_ctx::init_once()
{
	_instance = new mrp_ctx();
}

}
}

