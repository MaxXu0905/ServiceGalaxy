#if !defined(__CLEANUPSVR_H__)
#define __CLEANUPSVR_H__

#include "sg_internal.h"

namespace ai
{
namespace sg
{

int const SUCCESS = 0;	/* everything OK */
int const ERROR = 1;	/* fatal error - don't do anything */
int const MAXECODE = 2;	/* to size stats array */

class sgclear : public sg_manager
{
public:
	sgclear();
	~sgclear();

	int run(int argc, char **argv);

private:
	int init(int argc, char **argv);
	int do_clean();
	bool morework(scoped_bblock& bblock);
	void drop_msg(message_pointer& msg);
	void cleanup();
	void tixe(scoped_bblock& bblock);
	void logmsg(const char* filename, int linenum, const string& msg);
	static void onintr(int signo);

	bool aflag;
	int grpid;
	int svrid;

	bool notty;
	sg_proc_t myproc;
	pid_t oldpid;
	sg_ste_t ste;
	std::vector<sgid_t> processed_sgids;
	int stats[MAXECODE];

	static bool gotintr;
};

}
}

#endif

