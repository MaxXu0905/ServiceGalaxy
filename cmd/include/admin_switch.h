#if !defined(__ADMIN_SWITCH_H__)
#define __ADMIN_SWITCH_H__

#include "bbm_ctx.h"

namespace ai
{
namespace sg
{

class admin_bbasw : public sg_bbasw
{
protected:
	virtual sg_bbasw_t * get_switch();
};

class admin_ldbsw : public sg_ldbsw
{
protected:
	virtual sg_ldbsw_t * get_switch();
};

class admin_authsw : public sg_authsw
{
protected:
	virtual sg_authsw_t * get_switch();
};

}
}

#endif

