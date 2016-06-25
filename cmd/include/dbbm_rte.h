#if !defined(__DBBM_RTE_H__)
#define __DBBM_RTE_H__

#include "sg_rte.h"
#include "bbm_ctx.h"

namespace ai
{
namespace sg
{

class dbbm_rte : public sg_rte
{
protected:
	virtual bool venter();

private:
	dbbm_rte();
	~dbbm_rte();

	bbm_ctx *BBM;

	friend class derived_switch_initializer;
};

}
}

#endif

