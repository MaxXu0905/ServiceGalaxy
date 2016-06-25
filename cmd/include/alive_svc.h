#if !defined(__ALIVE_SVC_H__)
#define __ALIVE_SVC_H__

#include "sg_internal.h"
#include "sshp_ctx.h"

namespace ai
{
namespace sg
{

struct alive_proc_t
{
	string usrname;
	string hostname;
	time_t expire;
	set<pid_t> pids;

	bool operator<(const alive_proc_t& rhs) const {
		if (usrname < rhs.usrname)
			return false;
		else if (usrname > rhs.usrname)
			return true;

		return (hostname < rhs.hostname);
	}
};

class alive_svc : public sg_svc
{
public:
	alive_svc();
	~alive_svc();

	svc_fini_t svc(message_pointer& svcinfo);

private:
	bool get_procs(const string& usrname, const string& hostname, set<pid_t>& pids);

	sshp_ctx *SSHP;
	message_pointer result_msg;
	set<alive_proc_t> procs;
};

}
}

#endif

