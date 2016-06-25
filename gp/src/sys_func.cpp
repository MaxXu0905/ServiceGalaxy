#include <sys/utsname.h>
#include "common.h"
#include "sys_func.h"
#include "gpenv.h"
#include "usignal.h"
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

namespace ai
{
namespace sg
{

sys_func sys_func::_instance;

sys_func& sys_func::instance()
{
	return _instance;
}

int sys_func::background()
{
	switch (::fork()) {
	case 0:
		// make immune to signals sent to caller
		::setpgid(0, 0);
		return 0;
	case -1:
		// log fork error
		return -1;
	default:
		::exit(0);
		break;
	}
	// NOT REACHED
	return 0;
}

/*
 * Parameters:
 *
 *	funcp	A function pointer. The function must return void.
 *
 *	argp	A pointer to the argument. Before it is passed
 *		to the function, a private copy of the argument is made
 *		so that it can be changed without affecting the calling
 *		process. It is assumed that the data structure which pointed
 * 		by argp is flat (e.g., doesn't include pointers) in making
 *		the copy. Any data needed by the function must be passed
 *		as an element in the data structure (e.g., don't depend on
 *		global variables).
 *
 * Return:
 *	0	success
 *	-1	failed
 */
int sys_func::exec(void (*funcp)(char *), char *argp)
{
	int status;

	switch (::fork()) {
	case 0:
		switch (::fork()) {
		case 0:
			// grandchild
			(*funcp)(argp);
			// FALL THROUGH
		default:
			::exit(0);
			// NOTREACHED
		case -1:
			// fork error for grandchild
			::exit(-1);
		}
		break;
	case -1:
		// fork error
		return -1;
		// NOTREACHED
	default:
		::wait(&status);
		return gp_status(status);
		// NOTREACHED
	}
	// NOTREACHED
	return 0;
}

/*
 * Parameters:
 *
 *	funcp	A function pointer. The function must return void.
 *
 *	argp	A pointer to the argument. Before it is passed
 *		to the function, a private copy of the argument is made
 *		so that it can be changed without affecting the calling
 *		process. It is assumed that the data structure which pointed
 * 		by argp is flat (e.g., doesn't include pointers) in making
 *		the copy. Any data needed by the function must be passed
 *		as an element in the data structure (e.g., don't depend on
 *		global variables).
 *
 * Return:
 *	0	success
 *	-1	failed
 */
int sys_func::new_func(void (*funcp)(char *), char *argp)
{
	int status;

	switch (::fork()) {
	case 0:
		switch (::fork()) {
		case 0:
			// grandchild
			(*funcp)(argp);
			// FALL THROUGH
		default:
			::exit(0);
			// NOTREACHED
		case -1:
			// fork error for grandchild
			::exit(-1);
		}
		break;
	case -1:
		// fork error
		return -1;
		// NOTREACHED
	default:
		::wait(&status);
		return gp_status(status);
		// NOTREACHED
	}
	// NOTREACHED
	return 0;
}

/*
 *	Execute a new task.  By default, the calling process
 *	will wait for the new task to complete.
 *
 * Parameters:
 *
 *	task	Task can be the name of an executable file, or a
 *		command line. If a full path is not given, the PATH
 *		or similar environment variable will be used to locate
 *		the file.
 *
 *	argv	Pointer to an array of character pointers to
 *		null-terminated strings. These strings constitute
 *		the arg list available to the new process.
 *		If the argv is NULL, a new argv will be constructed
 *		from the character string passed in task.
 *
 *	mode	If the mode is _GP_P_NOWAIT then the calling process will
 *		not wait.
 *
 * Return:
 *	Returns 0 on success. Returns -1 when the calling process failed
 *	to start the new process. If the _GP_P_NOWAIT is not specified,
 *	it will return the exit code from the new task.
 */
int sys_func::new_task(const char *task, char **argv, int mode)
{
	int status = 0;
	pid_t ret_code;
	int wait_flag = 0;

	sig_action sig(SIGCHLD, SIG_DFL);

	if (mode & _GP_P_NOWAIT)
		wait_flag = _GP_P_NOWAIT;

	pid_t pid = ::fork();
	switch (pid) {
	case 0:
		if (wait_flag == _GP_P_NOWAIT) {
			switch (::fork()) {
			case 0:
				// grandchild
				break;
			case -1:
				// fork error for grandchild
				::exit(-1);
				// NOTREACHED
			default:
				::exit(0);
				// NOTREACHED
			}
		}

		if (argv == NULL)
			ret_code = ::execl("/bin/sh", "/bin/sh", "-c", task, (char *)0);
		else
			ret_code = ::execvp(task, argv);
		::exit(ret_code);
		// NOTREACHED
	case -1:
		// fork error
		return -1;
		// NOTREACHED
	default:
		/*
		 * This 'while' effectively implements waitpid(), which I think
		 * should have used in the first place.
		 * Note that the block comments for this function state that if
		 * _GP_P_NOWAIT is specified, there is no wait. But here, the
		 * parent process is doing a wait() regardless of mode's value.
		 * The parent either waits (the _GP_P_NOWAIT case) for the child
		 * to do a fork() and exit(), or for the child to do the exec().
		 * At least now, with the addition of the while loop,
		 * the parent will be waiting for the correct child process,
		 * whereas before, the single wait() call was terminating with the
		 * death of a *previously* forked child (from unrelated code!),
		 * and this was mis-interpreted (in one CR094635 test scenario)
		 * as a failure to exec cleanupsrv.
		 */
		while ((ret_code = wait(&status)) != -1 && ret_code != pid) {
			/*
			 * Reset status, in case a later wait() call returns -1.
			 * (But note that there is no provision to properly
			 * report, using errno, this type of failure. At least
			 * this line of code keeps us from reporting a leftover
			 * status from a previous wait() call.)
			 */
			status = 0;
		}
		return gp_status(status);
		// NOTREACHED
	}
}

// Return full path name of a program
void sys_func::progname(string& fullname, const string& name)
{
	const char *path = ::getenv("PATH");
	if (path == NULL)
		path = "";

	vector<string> the_vector;
	common::string_to_array(path, ':', the_vector);

	BOOST_FOREACH(const string& v, the_vector) {
		string tmp = v + '/' + name;
		if (access(tmp.c_str(), X_OK) == 0) {
			fullname = tmp;
			return;
		}
	}

	if (name[0] != '/') {
		// path is relative
		char curr_dir[_POSIX_PATH_MAX];
		::getcwd(curr_dir, sizeof(curr_dir));
		fullname = curr_dir;
		fullname += '/';
		fullname += name;
	} else {
		fullname = name;
	}
}

int sys_func::nodename(char *name, int length)
{
	struct utsname host;

	if (::uname(&host) < 0)
		return -1;
	strncpy(name, host.nodename, length-1);
	name[length-1] = '\0';
	return 0;
}

int sys_func::gp_status(int stat)
{
#if defined(WIFEXITED) && defined(WEXITSTATUS)
	if (WIFEXITED(stat))	/* non-zero if status for normal termination */
		return WEXITSTATUS(stat);
	else
		return -1;	/* error occurred */
#else
	/* classic version used to return -1 on error */
	return stat;
#endif
}

bool sys_func::page_start(_gp_pinfo **_gp_pHandle)
{
	/*
	 *  Redirect standard output to a pipe.  A child
	 *  process is spawned to read from the pipe and
	 *  paginate the output.
	 *
	 *  See also:  pipe_finish()
	 */
	int rc;
	const char *pager;
	int  pipefd[2];
	int  stdout_fd;
	pid_t child_pid;
	_gp_pinfo *pHandle;

	if ((pager = ::getenv("PAGER")) == NULL)
		pager = _TMPAGER;
	errno = 0; // don't put stale values in userlog()'s below

	::fflush(stdout);

	if (::pipe(pipefd) == -1) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: could not make pipe, errno={1}") % errno).str(SGLOCALE));
		return false; // stdout will not be redirected
	}

	if ((child_pid = ::fork()) == 0) {
		// child process' thread of execution
		::close(0);
		// set stdin to be read end of pipe
		if ((rc = ::dup(pipefd[0])) != 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: child dup error, rc={1}, errno={2}") % rc % errno).str(SGLOCALE));
			::exit(1);
		}
		::close(pipefd[1]); // child doesn't need write end
		::close(pipefd[0]); // read end is already dup'ed

		rc = gp_status(::system(pager));
		if (rc != 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: {1}: rc={2}, errno={3}") % pager % rc % errno).str(SGLOCALE));
			if ((rc & 0xff) == 0)
				rc = 2;
		}
		::_exit(rc);
	} else if (child_pid == -1) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: could not fork, errno={1}") % errno).str(SGLOCALE));
		return false; // stdout will not be redirected
	}

	// parent process' thread of execution

	// save stdout file descriptor
	if ((stdout_fd = ::dup(1)) == -1) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: parent dup error while saving stdout, errno={1}") % errno).str(SGLOCALE));
		return false; // don't close stdout if can't restore later
	}

	::close(1);
	// set stdout to be write end of pipe
	if ((rc = ::dup(pipefd[1])) != 1)
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: parent dup error for pipe, rc={1}, errno={2}") % rc % errno).str(SGLOCALE));

	::close(pipefd[0]); // parent doesn't need read end
	::close(pipefd[1]); // write end is already dup'ed
	pHandle = new _gp_pinfo();
	if (pHandle == NULL) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: malloc() failed, error_code = {1}") % errno).str(SGLOCALE));
		::close(1);

		// restore stdout file descriptor
		if ((rc = ::dup(pHandle->stdout_fd)) != 1)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: restore dup error, rc={1}, errno={2}") % rc % errno).str(SGLOCALE));

		::close(stdout_fd); // get rid of copy
		delete pHandle;
		return false;
	}
	pHandle->chpid = child_pid;
	pHandle->stdout_fd = stdout_fd;
	::strcpy(pHandle->pager, pager);
	*_gp_pHandle = pHandle;
	return true;
}

void sys_func::page_finish(_gp_pinfo **_gp_pHandle)
{
	/*
	 *  Finish redirecting standard output to a pipe.
	 *  Wait until the child process is finished, and
	 *  then restore standard output.
	 *
	 *  See also:  page_start()
	 */
	int status = 0;
	int rc;
	pid_t pid;
	_gp_pinfo *pHandle;

	errno = 0; // don't put stale values in userlog()'s below
	pHandle = *_gp_pHandle;

	if (pHandle == NULL)
		return;

	// flush output and send EOF to child process
	::fflush(stdout);
	::close(1);

	// restore stdout file descriptor
	if ((rc = ::dup(pHandle->stdout_fd)) != 1)
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: restore dup error, rc={1}, errno={2}") % rc % errno).str(SGLOCALE));

	::close(pHandle->stdout_fd); // get rid of copy

	while ((pid = ::wait(&status)) != pHandle->chpid) {
		// wait for the child process to finish
		if (pid == -1 && errno == ECHILD)
			break; // no children waiting!
	}

	if (status != 0)
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: {1}: wait status {2}, errno={3}") % pHandle->pager % status % errno).str(SGLOCALE));
	delete *_gp_pHandle;
}

/*
 * Abstraction for tempnam used to generate temporary
 * filenames for ServiceGalaxy. The goals of this routine are:
 *
 * 	Allow the ServiceGalaxy user to specify the directory
 * 	in which all temporary files will be created.
 *
 * 	To be thread safe.
 *
 * 	To generate a unique filename per process.
 *
 * 	Call must not fail after TMP_MAX iterations
 * 	(tmpnam only works for TMP_MAX iterations).
 *
 *
 * tempnam tacks on the threadid as a method
 * for returning a unique filename in a multi-threaded
 * environment. ("SYS" is used as a prefix to identify
 * the files).
 *
 * If successful, this routine returns a pointer to the name
 * generated, otherwise NULL is returned. The temporary file
 * name is also copied into the ARG passed, if it is not NULL.
 *
 * NOTE: IT IS THE CALLER'S RESPONSIBILITY TO FREE THE SPACE
 * 	 WHEN CALLED WITH A NULL ARGUMENT AND IT IS NO LONGER
 * 	 NEEDED.
 *
 */
void sys_func::tempnam(string& tfn)
{
	string thrid_str = boost::lexical_cast<string>(boost::this_thread::get_id());

	// Use the prefix "SYS" to id the files.
	char *tempnamep = ::tempnam("", "SYS");
	if (tempnamep == (char *)NULL) {
		tfn = "";
		return;
	}

	/*
	 * If threads are supported on this platform,
	 * tack on the thread id to make the filename
	 * unique within the (multi-threaded) process.
	 * It is assumed that the OS will take care of
	 * making the files unique among processes.
	 */

	/*
	 * Copy the contents of what tempnam returned into
	 * our filename pointer. If threads are supported,
	 * tack on this thread's id.
	 */
	tfn = tempnamep;
	tfn += thrid_str;
}

sys_func::sys_func()
{
	GPP = gpp_ctx::instance();
}

sys_func::~sys_func()
{
}

}
}

