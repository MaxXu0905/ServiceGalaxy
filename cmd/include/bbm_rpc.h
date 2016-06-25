#if !defined(__BBM_RPC_H__)
#define __BBM_RPC_H__

#include "bbm_ctx.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

class bbm_rpc : public sg_manager
{
public:
	static bbm_rpc& instance(sgt_ctx *SGT);

	int bbmsnd(message_pointer& msg, int flags);
	int broadcast(message_pointer& msg);
	int getreplies(int cnt);
	void cleanup();
	void pclean(int deadpe, sg_rpc_rply_t *rply);
	void updatevs(time_t now);
	bool pairedgw(const sg_proc_t& proc, sg_ste_t *ste, sgid_t *gwsgid);
	void updatemaster(sg_ste_t *ste);
	bool udbbm(sg_ste_t *ste);
	void resign();

private:
	bbm_rpc();
	virtual ~bbm_rpc();

	bbm_ctx *BBM;

	message_pointer rply_msg;
	int cmsgid;
	int curopcode;

	friend class derived_switch_initializer;
};

}
}

#endif
