#if !defined(__ALIVE_RPC_H__)
#define __ALIVE_RPC_H__

#include "sg_internal.h"

namespace ai
{
namespace sg
{

class alive_rpc : public sg_manager
{
public:
	static alive_rpc& instance(sgt_ctx *SGT);

	bool alive(const std::string& usrname, const std::string& hostname, time_t expire, pid_t pid);

private:
	alive_rpc();
	virtual ~alive_rpc();

	message_pointer send_msg;
	message_pointer recv_msg;

	friend class sgt_ctx;
};

}
}

#endif

