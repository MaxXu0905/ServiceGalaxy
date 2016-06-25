#if !defined(__BBM_RTE_H__)
#define __BBM_RTE_H__

#include "sg_rte.h"
#include "bbm_ctx.h"

namespace ai
{
namespace sg
{

class bbm_rte : public sg_rte
{
protected:
	virtual bool venter();
	virtual void vleave(sg_rte_t *rte);

private:
	bbm_rte();
	~bbm_rte();

	void resign();

	bbm_ctx *BBM;

	friend class derived_switch_initializer;
};

}
}

#endif

