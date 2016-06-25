#if !defined(__GWS_SWITCH_H__)
#define __GWS_SWITCH_H__

#include "gwp_ctx.h"
#include "bb_switch.h"

namespace ai
{
namespace sg
{

class gws_bbasw : public sg_bbasw
{
protected:
	virtual sg_bbasw_t * get_switch();
};

}
}

#endif

