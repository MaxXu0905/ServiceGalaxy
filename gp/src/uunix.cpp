#include <cerrno>
#include <cstdio>
#include <unistd.h>
#include "machine.h"
#include "uunix.h"

using namespace std;

namespace ai
{
namespace sg
{

const char * uunix::unix_msg[] = {
	"",
	"close",
	"creat",
	"exec",
	"fctnl",
	"fork",
	"lseek",
	"msgctl",
	"msgget",
	"msgsnd",
	"msgrcv",
	"open",
	"plock",
	"read",
	"semctl",
	"semget",
	"semop",
	"shmctl",
	"shmget",
	"shmat",
	"shmdt",
	"stat",
	"write",
	"sbrk",
	"sysmulti",
	"wait",
	"kill",
	"time",
	"mkdir",
	"link",
	"unlink",
	"uname",
	"nlist"
};

void uunix::err(const string& s)
{
	string buffer;

	if (!s.empty()) {
		buffer = s;
		buffer += ": ";
	}

	if (uunix_err < UUNIXMAX && uunix_err > UUNIXMIN) {
		buffer += unix_msg[uunix_err];
		buffer += (_(" system call error")).str(SGLOCALE);
	} else {
		buffer += (_(" unknown system call error")).str(SGLOCALE);
	}

	if (errno > 0) {
		::perror(buffer.c_str());
	} else {
		// this shouldn't happen
		::write(2, buffer.c_str(), buffer.length());
		::write(2, "\n", 1);
	}
}

const char * uunix::strerror(int err)
{
	if (err <= UUNIXMIN || err >= UUNIXMAX) // not valid
		return "";

	return unix_msg[err];
}

}
}

