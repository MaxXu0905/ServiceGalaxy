#if !defined(__SHM_ADMIN_H__)
#define __SHM_ADMIN_H__

#include "shm_malloc.h"
#include "sys_func.h"
#include "machine.h"

namespace ai
{
namespace sg
{

class shm_admain;

int const FALSEOPT = 0;
int const TRUEOPT = 1;
int const ALLOPT = 2;
int const OFFOPT = 4;

int const PRV = 020;			/* valid in "private" mode */
int const PAGE = 0400;			/* paginate output of this command */

int const SHMQUIT = 101;		/* Break out of main loop */

int const MAXARGS = 20;		/* Maximum number of command line arguments */
int const ARGLEN = 256;		/* Maximum len of each argument */

int const SHMCERR = 109;		/* Custom Error Message */
int const SHMLONGARG = 110;	/* Argument too long */
int const SHMBADCMD = 111;	/* No such command */
int const SHMBADOPT = 112;	/* Invalid option */
int const SHMARGCNT = 113;	/* Too many arguments */
int const SHMDUPARGS = 114;	/* Duplicate arguments */
int const SHMSYNERR = 115;	/* Syntax error on command line */

struct cmds_t
{
	const char *desc_msg;
	const char *name;
	const char *alias;
	const char *prms_msg;
	int flags;
	int (shm_admin::*proc)();
};

class shm_admin
{
public:
	shm_admin();
	~shm_admin();
	
	int run(int argc, char *argv[]);

	int shmae();
	int shmah();
	int shmav();
	int shmahex();
	int shmaq();
	int shmam();
	int shmaf();
	int shmara();
	int shmacc();
	int shmadc();
	int shmaph();
	int shmapu();
	int shmapf();
	int shmapc();
	int shmapk();
	int shmapd();
	int shmaid();
	int shmal();
	int shmaul();
	int shmad();
	int shmac();
	int shmarm();

private:
	int process();
	int scan(const string& s);
	int off_on(const char *name, bool& flag);
	bool validcmd(const cmds_t *ct);
	int determ();
	void clean_lock(spinlock_t& lock);

	static void intr(int sig);
	static const char *shm_emsgs(int err_num);
	
	bool echo;				/* echo input command lines */
	bool private_flag;			/* remove shared memory after exit */
	bool verbose;				/* print detailed output - current */
	bool d_verbose;				/* print detailed output - default */
	bool hex;				/* print data in hexadecimal mode */
	bool paginate;				/* paginate output */
	int numargs;				/* number of command line args */
	string cmdline;				/* input command line */
	char cmdargs[MAXARGS][ARGLEN + 3];	/* command line arguments */

	_gp_pinfo *pHandle;			/*this is a generix pointer which is set by 
						  call to _gp_page_start and freed by call to
						  _gp_page_finish. */

	string shmcerr;				/* "custom" error message */
	int oldargs;				/* number of command line args */
	string prevcmd;				/* previous command line */
	string prevsys;				/* previous system command */

	short c_sopt;			/* size set */
	short c_aopt;			/* address set */
	short c_iopt;			/* block index set */
	short c_Oopt;			/* block offset set */
	short c_oopt;			/* offset set */
	short c_topt;			/* type set */
	short c_hopt;			/* hash_size set */
	short c_dopt;			/* data_size set */
	short c_lopt;			/* data_len set */
	short c_copt;			/* com_key set */
	short c_Sopt;			/* svc_key set */
	short c_yopt;			/* verbose set */
	short c_vopt;			/* value set */

	int c_size;
	long c_address;
	int c_block_index;
	int c_block_offset;
	int c_offset;
	shm_locktype_enum c_type;
	int c_hash_size;
	int c_data_size;
	int c_data_len;
	string c_com_key;
	string c_svc_key;
	string c_value;

	shm_malloc mgr;
	
	gpp_ctx *GPP;
	gpt_ctx *GPT;
	ostringstream fmt;

	static cmds_t cmds[];
	static int maxcmds;
	static int gotintr;
};

}
}

#endif


