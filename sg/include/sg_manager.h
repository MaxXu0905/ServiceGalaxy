#if !defined(__SG_MANAGER_H__)
#define __SG_MANAGER_H__

namespace ai
{
namespace sg
{

class gpp_ctx;
class gpt_ctx;
class sgp_ctx;
class sgt_ctx;

enum sg_manager_t {
	PTE_MANAGER,
	NTE_MANAGER,
	RTE_MANAGER,
	SGTE_MANAGER,
	STE_MANAGER,
	SCTE_MANAGER,
	QTE_MANAGER,
	PROC_MANAGER,
	IPC_MANAGER,
	VIABLE_MANAGER,
	AGENT_MANAGER,
	RPC_MANAGER,
	SVCDSP_MANAGER,
	SVR_MANAGER,
	HANDLE_MANAGER,
	META_MANAGER,
	API_MANAGER,
	CHK_MANAGER,
	BRPC_MANAGER,
	GRPC_MANAGER,
	FRPC_MANAGER,
	ALIVE_MANAGER,
	TOTAL_MANAGER
};

class sg_manager
{
public:
	sg_manager();
	virtual ~sg_manager();

protected:
	gpp_ctx *GPP;
	gpt_ctx *GPT;
	sgp_ctx *SGP;
	sgt_ctx *SGT;
};

}
}

#endif

