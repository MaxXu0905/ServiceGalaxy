#if !defined(__SG_VIABLE_H__)
#define __SG_VIABLE_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

class sg_viable : public sg_manager
{
public:
	static sg_viable& instance(sgt_ctx *SGT);

	int inviable(sg_ste_t *s, int& spawn_flags);
	int upinviable(sg_ste_t *s, int& spawn_flags);
	int nullinviable(sg_ste_t *s, int& spawn_flags);

	int dltsvr(sg_qte_t *q, sg_ste_t *s);
	int clnupsvr(sg_qte_t *q, sg_ste_t *s, int& spawn_flags);
	int rstsvr(sg_qte_t *q, sg_ste_t *s, int& spawn_flags);

private:
	sg_viable();
	virtual ~sg_viable();

	void cln_no_reg(sg_ste_t *ste);
	void reset_pmid(int mid);

	friend class sgt_ctx;
};

}
}

#endif

