#if !defined(__SYS_FUNC_H__)
#define __SYS_FUNC_H__

#include <string>
#include "gpp_ctx.h"

namespace ai
{
namespace sg
{

// OS abstraction for fork/exec
int const _GP_P_NOWAIT = 0x1;
int const _GP_P_DETACHED = 0x2;

using namespace std;

struct _gp_pinfo
{
	pid_t chpid; // this is the child pid that is needed for the wait
	int stdout_fd; // this saves a pointer to the original stdout
	char pager[20];
};

class sys_func
{
public:
	static sys_func& instance();

	int exec(void (*funcp)(char *), char *argp);
	int background();
	int new_func(void (*funcp)(char *), char *argp);
	int new_task(const char *task, char **argv, int mode);
	void progname(string& fullname, const string& name);
	int nodename(char *name, int length);
	int gp_status(int stat);
	bool page_start(_gp_pinfo **_gp_pHandle);
	void page_finish(_gp_pinfo **_gp_pHandle);
	void tempnam(string& tfn);

private:
	sys_func();
	~sys_func();

	gpp_ctx *GPP;

	static sys_func _instance;
};

}
}

#endif

