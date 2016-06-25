#if !defined(__BSGWS_SWITCH_H__)
#define __BSGWS_SWITCH_H__

#include "gwp_ctx.h"
#include "bb_switch.h"

namespace ai
{
namespace sg
{

class bsgws_bbasw : public sg_bbasw
{
protected:
	virtual sg_bbasw_t * get_switch();
};

class bsgws_ldbsw : public sg_ldbsw
{
protected:
	virtual sg_ldbsw_t * get_switch();
};

class bsgws_authsw : public sg_authsw
{
protected:
	virtual sg_authsw_t * get_switch();
};

}
}

#endif

