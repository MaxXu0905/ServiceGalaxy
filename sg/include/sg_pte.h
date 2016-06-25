#if !defined(__SG_PTE_H__)
#define __SG_PTE_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

class sg_pte : public sg_manager
{
public:
	static sg_pte& instance(sgt_ctx *SGT);

	// 创建MTE节点
	int create(sg_mchent_t& parms, int flags);
	int smcreate(sg_mchent_t& parms, int flags);
	int mbcreate(sg_mchent_t& parms, int flags);

private:
	sg_pte();
	virtual ~sg_pte();

	friend class sgt_ctx;
};

}
}

#endif

