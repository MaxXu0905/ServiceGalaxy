#if !defined(__SG_RTE_H__)
#define __SG_RTE_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

class sg_rte : public sg_manager
{
public:
	static sg_rte& instance(sgt_ctx *SGT);

	bool enter();
	bool smenter();
	bool nullenter();
	void leave(sg_rte_t *rte);
	void smleave(sg_rte_t *rte);
	void nullleave(sg_rte_t *rte);
	bool update(sg_rte_t *rte);
	bool smupdate(sg_rte_t *rte);
	bool nullupdate(sg_rte_t *rte);
	bool clean(pid_t, sg_rte_t *rte);
	bool smclean(pid_t pid, sg_rte_t *rte);
	bool nullclean(pid_t pid, sg_rte_t *rte);

	virtual bool venter();
	virtual void vleave(sg_rte_t *rte);
	virtual bool vupdate(sg_rte_t *rte);
	virtual bool vclean(pid_t, sg_rte_t *rte);

	sg_rte_t * find_slot(pid_t pid);
	bool suser(const sg_bbparms_t& bbp);

	sg_rte_t * link2ptr(int link);
	int ptr2link(const sg_rte_t *rte) const;

protected:
	sg_rte();
	virtual ~sg_rte();

	bool get_Rslot();
	bool get_oldslot();
	bool get_slot();
	void reset_slot(sg_rte_t *rte, long timestamp);
	sg_rte_t * get();
	void put(sg_rte_t *rte);

	friend class sgt_ctx;
};

}
}

#endif

