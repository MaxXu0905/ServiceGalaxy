#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include "usignal.h"

namespace ai
{
namespace sg
{

sig_action::sig_action()
{
	signo = -1;
	released = true;
}

sig_action::sig_action(int signo, SIG_HANDLER sig_handler, int flags, bool released)
{
	install(signo, sig_handler, flags, released);
}

sig_action::~sig_action()
{
	release();
}

sig_action::SIG_HANDLER sig_action::install(int signo, SIG_HANDLER sig_handler, int flags, bool released)
{
	struct sigaction act;

	this->signo = signo;
	this->released = released;

	// install our SIGPIPE signal handler
	act.sa_handler = sig_handler;
	act.sa_flags = flags;
	::sigemptyset(&act.sa_mask);
	::sigaction(signo, &act, &oact);
	return oact.sa_handler;
}

void sig_action::release()
{
	// restore the previous signal handler
	if (!released) {
		::sigaction(signo, &oact, NULL);
		released = true;
	}
}

int usignal::sig_pending = 0; // 1 or more signals pending
int usignal::sig_defer = 0; // "level" to which signals deferred
sig_tbl_t usignal::sigs[32];
long usignal::num_deferred = 0;
long usignal::num_intercepted = 0;
int usignal::intercept_set = 0;
int usignal::immediate = 0;

void usignal::intercept(int sig)
{
	SIGCASTF user_func;

	num_intercepted++;

	if (sigs[sig].func == (SIGCASTF)(SIG_IGN)) {
		struct sigaction act;
		act.sa_handler = sigs[sig].func;
		act.sa_flags = (sig == SIGCHLD) ? SA_NOCLDWAIT : 0;
		::sigemptyset(&act.sa_mask);
		::sigaction(sig, &act, NULL);
		sigs[sig].num_sigs = 0;
		return;
	}

	if (sig_defer != 0) {
		num_deferred++;
		/* signals being deferred ... post and return */
		/*
			note ... arrival of same signal
			during following increment operation could cause
			miscount in numsigs, if increment is not
			atomic action
		*/
		sigs[sig].num_sigs++;
		sig_pending = 1;
		return;
	}

	if (sigs[sig].func == (SIGCASTF)(SIG_DFL)){
		if (sig == SIGQUIT)
			::abort();
		::exit(sig);
	}

	user_func = sigs[sig].func;
	(*user_func)(sig);
	return;
}

void usignal::setsig(int sig)
{
	struct sigaction act, oact;

	act.sa_handler = (SIGCASTF)intercept;
	act.sa_flags = 0;
	::sigemptyset(&act.sa_mask);
	::sigaction(sig, &act, &oact);
	if (oact.sa_handler == (SIGCASTF)(SIG_IGN))
		::sigaction(sig, &oact, NULL);
	sigs[sig].func = (SIGCASTF)oact.sa_handler;
}

void usignal::siginit()
{
	if (intercept_set || immediate)
		return;

	setsig(SIGHUP);
	setsig(SIGINT);
	setsig(SIGQUIT);
	setsig(SIGALRM);
	setsig(SIGTERM);
	setsig(SIGUSR1);
	setsig(SIGUSR2);

	intercept_set = 1;
}

SIGCASTF usignal::signal(int signo, SIGCASTF func)
{
	struct sigaction act, oact;

	if (!intercept_set)
		siginit();

	if (immediate){
		act.sa_handler = func;
		act.sa_flags = 0;
		::sigemptyset(&act.sa_mask);
		::sigaction(signo, &act, &oact);
		return (SIGCASTF)oact.sa_handler;
	}

	switch (signo){
	case SIGHUP:
	case SIGINT:
	case SIGQUIT:
	case SIGALRM:
	case SIGTERM:
	case SIGUSR1:
	case SIGUSR2:
		break;
	default:
		act.sa_handler = func;
		act.sa_flags = 0;
		::sigemptyset(&act.sa_mask);
		::sigaction(signo, &act, &oact);
		return (SIGCASTF)oact.sa_handler;
	}

	oact.sa_handler = (SIGCASTF)sigs[signo].func;
	sigs[signo].func = (SIGCASTF)func;

	if (func == (SIGCASTF)SIG_IGN) {
		// if signal is to be ignored, pass it thru immediately
		act.sa_handler = (SIGCASTF)SIG_IGN;
		act.sa_flags = (signo == SIGCHLD) ? SA_NOCLDWAIT : 0;
		::sigemptyset(&act.sa_mask);
		::sigaction(signo, &act, NULL);
		sigs[signo].num_sigs = 0;
	} else {
		// if signal had been ignored, now intercept it
		if (oact.sa_handler == (SIGCASTF)SIG_IGN){
			act.sa_handler = (SIGCASTF)intercept;
			act.sa_flags = 0;
			::sigemptyset(&act.sa_mask);
			::sigaction(signo, &act, NULL);
		}
	}

	return (SIGCASTF)oact.sa_handler;
}

void usignal::chksig(int sig)
{
	SIGCASTF user_func;

	while (sigs[sig].num_sigs > 0) {
		if (sigs[sig].func == (SIGCASTF)SIG_IGN) {
			// user routine itself could call Usignal to stop signal ... if so discard remaining count
			sigs[sig].num_sigs = 0;
			return;
		}
		if (sigs[sig].func == (SIGCASTF)SIG_DFL) {
			if (sig == SIGQUIT)
				::abort();
			::exit(sig);
		}
		user_func = sigs[sig].func;
		(*user_func)(sig);
		sigs[sig].num_sigs--;
	}
}

void usignal::do_sigs()
{
	/*
	 * set off sig_pending before calling chksig ...
	 * otherwise user handling function might set and
	 * restore signals ... leading to a recursive call
	 * to do_sigs
	 */
	set_pending(0);

	chksig(SIGHUP);
	chksig(SIGINT);
	chksig(SIGQUIT);
	chksig(SIGALRM);
	chksig(SIGTERM);
	chksig(SIGUSR1);
	chksig(SIGUSR2);
}

int usignal::get_pending()
{
	return usignal::sig_pending;
}

void usignal::set_pending(int sig_pending)
{
	usignal::sig_pending = sig_pending;
}

int usignal::get_defer()
{
	return usignal::sig_defer;
}

void usignal::set_defer(int sig_defer)
{
	usignal::sig_defer = sig_defer;
}

void usignal::defer()
{
	usignal::sig_defer++;
}

/*
 * Generate a SIGALRM signal in ``usecs'' microseconds.
 * If ``reload'' is non-zero, keep generating SIGALRM
 * every ``reload'' microseconds after the first signal.
 */
unsigned long usignal::ualarm(unsigned long usecs, unsigned long reload)
{
	int const USPS = 1000000; // # of microseconds in a second

	itimerval newval;
	itimerval oldval;

	newval.it_interval.tv_usec = reload % USPS;
	newval.it_interval.tv_sec = reload / USPS;

	newval.it_value.tv_usec = usecs % USPS;
	newval.it_value.tv_sec = usecs / USPS;

	if (::setitimer(ITIMER_REAL, &newval, &oldval) == 0)
		return (oldval.it_value.tv_sec * USPS + oldval.it_value.tv_usec);

	return 0;
}

// reinstate signals ... note that reinstatement really unstacks
void usignal::resume_sigs()
{
	if (sig_defer > 0)
		sig_defer--;
	if (sig_defer <= 0 && sig_pending)
		do_sigs();
}

// pop deferral stack completely, ensures that sigs will be processed
void usignal::ensure_sigs()
{
	sig_defer = 0;
	if (sig_pending)
		do_sigs();
}

// set defer level
void usignal::use_defer_level(int new_level)
{
	sig_defer = (new_level < 0) ? 0 : new_level;
	if (sig_defer <= 0 && sig_pending)
		do_sigs();
}

scoped_usignal::scoped_usignal()
{
	usignal::siginit();
	usignal::defer();
}

scoped_usignal::~scoped_usignal()
{
	usignal::resume_sigs();
}

}
}

