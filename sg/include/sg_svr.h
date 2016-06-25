#if !defined(__SG_SVR_H__)
#define __SG_SVR_H__

#include <string>
#include <vector>
#include "sg_struct.h"
#include "sg_message.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

typedef sg_svr * (*INSTANCE_FUNC)();

class sg_svr : public sg_manager
{
public:
	static sg_svr& instance(sgt_ctx *SGT);

	sg_svr();
	virtual ~sg_svr();

	int _main(int argc, char **argv, INSTANCE_FUNC create_instance);
	void cleanup();
	int stop_serv();
	bool rcvrq(message_pointer& rmsg, int flags, int timeout = 0);
	bool finrq();

	static void * static_run(void *arg);

protected:
	virtual int svrinit(int argc, char **argv);
	virtual int svrfini();
	virtual int run(int flags = 0);

	int stdinit(int argc, char **argv);
	sgid_t enroll(const char *aout, std::vector<sg_svcparms_t>& svcparms_vector);
	bool synch(int status);
	bool chkclient(message_pointer& rmsg);
	bool rtntosndr(message_pointer& rmsg);

private:
	sgid_t failure();

	static void stdsigterm(int signo);
	static void sigpipe(int signo);

	static bool sync_gotsig;
};

}
}

#endif

