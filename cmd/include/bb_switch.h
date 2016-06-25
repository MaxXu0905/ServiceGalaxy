#if !defined(__BB_SWITCH_H__)
#define __BB_SWITCH_H__

#include "sg_switch.h"
#include "bbm_ctx.h"

namespace ai
{
namespace sg
{

class bb_switch : public sg_switch
{
public:
	bb_switch();
	~bb_switch();

	bool getack(sgt_ctx *SGT, message_pointer& msg, int flags, int timeout);
	void shutrply(sgt_ctx *SGT, message_pointer& msg, int status, sg_bbparms_t *bbparms);

protected:
	bbm_ctx *BBM;
};

}
}

#endif

