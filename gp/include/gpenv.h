#if !defined(__GPENV_H__)
#define __GPENV_H__

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include "gpp_ctx.h"
#include <boost/thread.hpp>

namespace ai
{
namespace sg
{

#if defined(_AIX)
typedef unsigned int _SOCK_SIZE_T;
#elif defined(__sparcv9) || defined(__hpux)
typedef int _SOCK_SIZE_T;
#elif defined(__linux__)
typedef socklen_t _SOCK_SIZE_T;
#else
typedef size_t _SOCK_SIZE_T;
#endif

#if defined(sun)

long const _TMPAGESIZE = 512L;
long const _TMSHMSEGSZ = 1024 * 1024;

#if defined(__sparcv9)
#define _TMLONG64
#endif

const char _TMPAGER[] = "more";
const char _TMPATH[] = "PATH";
const char _TMLDPATH[] = "LD_LIBRARY_PATH";
const char _SGCOMPILER[] = "CC";
const char _SHMPATH[] = "/dev/shm/";

#endif

#if defined(__hpux)

long const _TMPAGESIZE = 1024L;

#if defined(__ia64)
/* 64bit hpux default SHM size is 1GB */
long const _TMSHMSEGSZ = 1024 * 1024 * 1024;
#else
/* 32bit hpux default SHM size is 64MB */
long const _TMSHMSEGSZ = 64 * 1024 * 1024;
#endif

const char _TMPAGER[] = "pg -e";
const char _TMPATH[] = "PATH";
const char _TMLDPATH[] = "SHLIB_PATH";
const char _SGCOMPILER[] = "aCC";
const char _SHMPATH[] = "/dev/shm/";

#endif

#if defined(_AIX)

#if !defined(_POSIX_SOURCE)
typedef unsigned long uid_t;
typedef unsigned long gid_t;
typedef	unsigned long mode_t;
typedef char *caddr_t;
typedef	int pid_t;
typedef	long off_t;
#endif

#if !defined(_ALL_SOURCE)
// User message buffer template for msgsnd and msgrecv system calls.
struct msgbuf
{
	long mtype; // message type
	char mtext[1]; // message text
};

// derived types
typedef	unsigned char uchar;
typedef	unsigned short ushort;
typedef	unsigned int uint;
typedef unsigned long ulong;
typedef	unsigned char u_char;
typedef	unsigned short u_short;
typedef	unsigned int u_int;
typedef unsigned long u_long;
int const NBBY = 8; // number of bits in a byte
#endif

long const _TMPAGESIZE = 4096L;
int const _TMSHMSEGSZ = 256 * 1024 * 1024;

#if defined(__64BIT__)
#define _TMLONG64
#endif

const char _TMPAGER[] = "pg -e";
const char _TMPATH[] = "PATH";
const char _TMLDPATH[] = "LIBPATH";
const char _SGCOMPILER[] = "xlC";
const char _SHMPATH[] = "/dev/shm/";

#endif

#if defined(__linux__)

typedef uid_t iuid_t;
typedef gid_t igid_t;

long const _TMPAGESIZE = 512L;
long const _TMSHMSEGSZ = 16 * 1024 * 1024;

#if defined(__LP64__)
#define _TMLONG64
#endif

#define _TML_ENDIAN 1

const char _TMPAGER[] = "more";
const char _TMPATH[] = "PATH";
const char _TMLDPATH[] = "LD_LIBRARY_PATH";
const char _SGCOMPILER[] = "g++";
const char _SHMPATH[] = "/dev/shm/";

#endif

typedef size_t TMNUMPTR;
typedef	int _TMXDRINT;
typedef	unsigned int _TMXDRUINT;
typedef	int _TMXDRINT;
typedef	unsigned int _TMXDRUINT;

int const _TMNETSIG = SIGIO;
int const _TMNONBLOCK = O_NDELAY;
long const _TMIOSIZE = _TMPAGESIZE * 4000;
int const _TMNSIGNALS = 32;

#if defined(PID_MAX)
#define _TMMAXPID PID_MAX
#else
#define _TMMAXPID 30000
#endif

#if defined(_TMLONG64)
int const _TMALIGNPTR = 8;
int const _TMALIGNSEM = 8;
long const _TMALIGNIO = 8L;
#else
int const _TMALIGNSEM = 4;
long const _TMALIGNIO = 4L;
#endif

#if !defined(__u_char_defined)
typedef unsigned int u_int;
typedef unsigned short u_short;
typedef unsigned long u_long;
#endif

int const INIT_ENV_SIZE = 40;

class gpenv
{
public:
	static gpenv& instance();

	bool read_env(const std::string& filename, const std::string& label = "");
	void expand_env(std::string& dst_str, const std::string& src_str);
	int getargv_env(int argc, char **argv);
	void expand_var(std::string& dst_str, const std::string& src_str);
	void putenv(const char *env);
	char * getenv(const char *env);

private:
	gpenv();
	~gpenv();

	gpp_ctx *GPP;

	static gpenv _instance;
	static boost::mutex env_mutex;
};

}
}

#endif

