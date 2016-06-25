#if !defined(__BBM_SWITCH_H__)
#define __BBM_SWITCH_H__

#include "bbm_ctx.h"
#include "bb_switch.h"

namespace ai
{
namespace sg
{

class bbm_bbasw : public sg_bbasw
{
protected:
	virtual sg_bbasw_t * get_switch();
};

class bbm_ldbsw : public sg_ldbsw
{
protected:
	virtual sg_ldbsw_t * get_switch();
};

class bbm_authsw : public sg_authsw
{
protected:
	virtual sg_authsw_t * get_switch();
};

class bbm_switch : public bb_switch
{
public:
	bbm_switch();
	~bbm_switch();

	bool svrstop(sgt_ctx *SGT);
	void bbclean(sgt_ctx *SGT, int mid, ttypes sect, int& spawn_flags);
};

}
}

#endif

