#if !defined(__SG_NTE_H__)
#define __SG_NTE_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

class sg_nte : public sg_manager
{
public:
	static sg_nte& instance(sgt_ctx *SGT);

	// ´´½¨NTE
	int create(sg_netent_t& parms, int flags);
	int smcreate(sg_netent_t& parms, int flags);
	int mbcreate(sg_netent_t& parms, int flags);

private:
	sg_nte();
	virtual ~sg_nte();

	friend class sgt_ctx;
};

}
}

#endif

