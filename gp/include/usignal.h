#if !defined(__USIGNAL_H__)
#define __USIGNAL_H__

#include <signal.h>
#include <sys/sem.h>

namespace ai
{
namespace sg
{

typedef void (*SIGCASTF)(int);

union semun
{
	int val;
	semid_ds *buf;
	unsigned short *array;
};

struct sig_tbl_t
{
	SIGCASTF func; // user action to be taken for signal ..., will be executed when signals not deferred
	int num_sigs;  // # times this sig deferred since prev Unosig
};

class sig_action
{
public:
	typedef void (*SIG_HANDLER)(int signo);

	sig_action();
	sig_action(int signo, SIG_HANDLER sig_handler, int flags = 0, bool released = false);
	~sig_action();

	SIG_HANDLER install(int signo, SIG_HANDLER sig_handler, int flags = 0, bool released = false);
	void release();

private:
	int signo;
	struct sigaction oact;
	bool released;
};

class usignal
{
public:
	static void setsig(int sig);
	static void siginit();
	static SIGCASTF signal(int signo, SIGCASTF func);
	static void chksig(int sig);
	static void do_sigs();
	static int get_pending();
	static void set_pending(int sig_pending);
	static int get_defer();
	static void set_defer(int sig_defer);
	static void defer();
	static unsigned long ualarm(unsigned long usecs, unsigned long reload);
	static void resume_sigs();
	static void ensure_sigs();
	static void use_defer_level(int new_level);

private:
	static void intercept(int sig);
	
	static int sig_pending; // 1 or more signals pending
	static int sig_defer; // "level" to which signals deferred
	static sig_tbl_t sigs[32];
	static long num_deferred;
	static long num_intercepted;
	static int intercept_set;
	static int immediate;
};

class scoped_usignal
{
public:
	scoped_usignal();
	~scoped_usignal();
};

}
}

#endif

