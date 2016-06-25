#include "gpt_ctx.h"

namespace ai
{
namespace sg
{

boost::thread_specific_ptr<gpt_ctx> gpt_ctx::_instance;

gpt_ctx * gpt_ctx::instance()
{
	gpt_ctx *GPT = _instance.get();
	if (GPT == NULL) {
		GPT = new gpt_ctx();
		_instance.reset(GPT);
	}
	return GPT;
}


gpt_ctx::gpt_ctx()
{
	_GPT_error_code = 0;
	_GPT_native_code = 0;
}

gpt_ctx::~gpt_ctx()
{
}

}
}

