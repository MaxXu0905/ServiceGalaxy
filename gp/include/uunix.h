#if !defined(__UUNIX_H__)
#define __UUNIX_H__

#include <string>

namespace ai
{
namespace sg
{

enum UUNIX_ERR_ENUM
{
	UUNIXMIN = 0,
	UCLOSE = 1,
	UCREAT = 2,
	UEXEC = 3,
	UFCTNL = 4,
	UFORK = 5,
	ULSEEK = 6,
	UMSGCTL = 7,
	UMSGGET = 8,
	UMSGSND = 9,
	UMSGRCV = 10,
	UOPEN = 11,
	UPLOCK = 12,
	UREAD = 13,
	USEMCTL = 14,
	USEMGET = 15,
	USEMOP = 16,
	USHMCTL = 17,
	USHMGET = 18,
	USHMAT = 19,
	USHMDT = 20,
	USTAT = 21,
	UWRITE = 22,
	USBRK = 23,
	USYSMUL = 24,
	UWAIT = 25,
	UKILL = 26,
	UTIME = 27,
	UMKDIR = 28,
	ULINK = 29,
	UUNLINK = 30,
	UUNAME = 31,
	UNLIST = 32,
	UUNIXMAX= 33
};

int const GPMINVAL = 0;			/* minimum error message */
int const GPEABORT = 1;
int const GPEBADDESC = 2;
int const GPEBLOCK = 3;
int const GPEINVAL = 4;
int const GPELIMIT = 5;
int const GPENOENT = 6;
int const GPEOS = 7;
int const GPEPERM = 8;
int const GPEPROTO = 9;
int const GPESVCERR = 10;
int const GPESVCFAIL = 11;
int const GPESYSTEM = 12;
int const GPETIME = 13;
int const GPETRAN = 14;
int const GPGOTSIG = 15;
int const GPERMERR = 16;
int const GPERELEASE = 17;
int const GPEMATCH = 18;
int const GPEDIAGNOSTIC = 19;
int const GPESVRSTAT = 20;		/* bad server status */
int const GPEBBINSANE = 21;		/* BB is insane */
int const GPENOBRIDGE = 22;		/* no BRIDGE is available */
int const GPEDUPENT = 23;		/* duplicate table entry */
int const GPECHILD = 24;		/* child start problem */
int const GPENOMSG = 25;		/* no message */
int const GPENOBBM = 26;		/* no sgmngr */
int const GPENODBBM = 27;		/* no sgmngr */
int const GPENOGWS = 28;		/* no sggws */
int const GPEAPPINIT = 29;		/* svrinit() failure */
int const GPENEEDPCLEAN = 30;	/* sgmngr failed to boot.. pclean may help */
int const GPMAXVAL = 31;		/* maximum error message */

class uunix
{
public:
	void err(const std::string& s);
	static const char * strerror(int err);

private:
	static const char *unix_msg[UUNIXMAX + 1];
	int uunix_err;
};

}
}

#endif

