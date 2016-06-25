#if !defined(__SG_IPC_H__)
#define __SG_IPC_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

class sg_ipc : public sg_manager
{
public:
	static sg_ipc& instance(sgt_ctx *SGT);

	// 发送消息
	bool send(message_pointer& msg, int flags);
	// 接收消息
	bool receive(message_pointer& msg, int flags, int timeout = 0);
	// 发送消息
	bool send_internal(message_pointer& msg, int flags);
	// 接收消息
	bool receive_internal(message_pointer& msg, int flags, int timeout);

private:
	sg_ipc();
	virtual ~sg_ipc();

	bool os_send(int mid, int qaddr, bool remote, message_pointer msg, int mtype, int flags);
	bool os_receive(int qaddr, message_pointer& msg, int mtype, int flags, int timeout);
	bool shm_transfer(message_pointer& msg);
	int findgw();
	int addbootstrap(bool count_only);
	void shutdown_due_to_sigterm();

	message_pointer sys_rcv_msg;

	friend class sgt_ctx;
};

}
}

#endif

