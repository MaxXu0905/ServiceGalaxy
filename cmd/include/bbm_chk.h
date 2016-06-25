#if !defined(__BBM_CHK_H__)
#define __BBM_CHK_H__

#include "bbm_ctx.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

class bbm_chk : public sg_manager
{
public:
	static bbm_chk& instance(sgt_ctx *SGT);

	int chk_bbsanity(ttypes sect, int& spawn_flags);
	void chk_accsrs(int flag);
	int chk_svrs(int& spawn_flags);
	void chk_misc();
	void pclean(int deadpe);
	void chk_blockers();

private:
	bbm_chk();
	virtual ~bbm_chk();

	bbm_ctx *BBM;

	friend class derived_switch_initializer;
};

}
}

#endif

