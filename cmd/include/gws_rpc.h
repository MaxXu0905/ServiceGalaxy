#if !defined(__GWS_RPC_H__)
#define __GWS_RPC_H__

#include "sg_internal.h"
#include "gwp_ctx.h"

namespace ai
{
namespace sg
{

int const SGRPTORQ = 0x00100000;

class gws_connection;

class gws_rpc : public sg_manager
{
public:
	static gws_rpc& instance(sgt_ctx *SGT);

	bool process_rpc(gws_connection *conn, message_pointer& msg);
	bool do_exit(message_pointer& rqst_msg, message_pointer& rply_msg);
	bool find_dbbm(message_pointer& rqst_msg, message_pointer& rply_msg);
	bool qctl(message_pointer& rqst_msg, message_pointer& rply_msg);
	bool rkill(message_pointer& rqst_msg, message_pointer& rply_msg);
	bool suspend();
	bool unsuspend();
	bool pclean(message_pointer& rqst_msg, message_pointer& rply_msg);
	message_pointer create_msg(int opcode, size_t datasize);

private:
	gws_rpc();
	virtual ~gws_rpc();

	sg_proc_t * find_dbbm_internal();
	int retrieve(sg_ste_t& ste);
	void nack_msg(gws_connection *conn, message_pointer& msg, int error_code);

	gwp_ctx *GWP;

	friend class gws_server;
	friend class gws_queue;
};

}
}

#endif

