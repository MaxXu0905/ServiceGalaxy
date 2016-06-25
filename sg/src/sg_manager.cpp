#include "sg_manager.h"
#include "gpp_ctx.h"
#include "gpt_ctx.h"
#include "sgp_ctx.h"
#include "sgt_ctx.h"

namespace ai
{
namespace sg
{

sg_manager::sg_manager()
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();
	SGP = sgp_ctx::instance();
	SGT = sgt_ctx::instance();
}

sg_manager::~sg_manager()
{
}

}
}

