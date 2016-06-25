#if !defined(__CMD_PARSER_H__)
#define __CMD_PARSER_H__

#include "sg_internal.h"

namespace po = boost::program_options;

namespace ai
{
namespace sg
{

enum parser_result_t
{
	PARSE_SUCCESS = 0,
	PARSE_CERR,			/* Custom Error Message */
	PARSE_BBNATT,		/* No bulletin board attached */
	PARSE_MALFAIL,		/* Memory malloc failure */
	PARSE_NOQ,			/* No queues exist */
	PARSE_BADQ,			/* No such queue */
	PARSE_QERR,			/* Queue retrieval error */
	PARSE_NOSR,			/* No servers exist */
	PARSE_BADSR,		/* No such server */
	PARSE_SRERR,		/* Server retrieval error */
	PARSE_NOSC,			/* No services exist */
	PARSE_BADSC,		/* No such service */
	PARSE_SCERR,		/* Service retrieval error */
	PARSE_NOBBM,		/* No sgmngr exists for that machine */
	PARSE_BADCOMB,		/* Invalid combination of options */
	PARSE_ERRSUSP,		/* Error suspending resources */
	PARSE_NOGID,		/* Group id is not set */
	PARSE_NOSID,		/* Server id is not set */
	PARSE_NONAME,		/* Service name is not set */
	PARSE_BADMNAM,		/* No such machine */
	PARSE_NOTADM,		/* Must be administrator to execute that command */
	PARSE_ERRRES,		/* Error resuming (un-suspending) resources */
	PARSE_QUIT,			/* Break out of main loop */
	PARSE_LOAD,			/* NOT PRINTED - Load private bulletin board */
	PARSE_RELBB,		/* NOT PRINTED - Release private bulletin board */
	PARSE_PRIVMODE,		/* Command not allowed in private mode */
	PARSE_BBMHERE,		/* No sgmngr exists on this machine */
	PARSE_BBMDEAD,		/* sgmngr dead on requested machine */
	PARSE_QSUSP,		/* Queue suspended for sgmngr on requested machine */
	PARSE_SRNOTSPEC,	/* No server is specified */
	PARSE_SCPRMERR,		/* Error retrieving service parms */
	PARSE_SYNERR,		/* Syntax error on command line */
	PARSE_SEMERR,		/* Error accessing system semaphore */
	PARSE_MSGQERR,		/* Error accessing message queue */
	PARSE_SHMERR,		/* Error accessing shared memory */
	PARSE_DUMPERR,		/* Error dumping bulletin board */
	PARSE_RESNOTSPEC,	/* No resources are specified */
	PARSE_WIZERR,		/* Incorrect release code */
	PARSE_MIGFAIL,		/* sgclst migration failure */
	PARSE_CDBBMFAIL,	/* Can not create new sgclst */
	PARSE_NOBACKUP,		/* Backup node is not specified in SGPROFILE file */
	PARSE_NOTBACKUP,	/* sgadmin not running on backup node */
	PARSE_PCLEANFAIL, 	/* Can not clean up partitioned BB entries */
	PARSE_RECONNFAIL, 	/* Network reconnection failure */
	PARSE_BADGRP,		/* Invalid server group name    */
	PARSE_PCLSELF,		/* Should not pclean the current machine */
	PARSE_HELP
};

int const BOOT = 0x1;		/* valid in "boot" mode */
int const READ = 0x2;		/* valid in "read" mode */
int const WIZ = 0x4;		/* valid only in "wizard" mode */
int const SM = 0x8;			/* valid only in "SM" mode */
int const PRV = 0x10;		/* valid in "private" mode */
int const ADM = 0x20;		/* valid only if administrator */
int const CONF = 0x40;		/* valid in "conf" mode */
int const MASTER = 0x80;	/* valid in "master" mode */
int const PAGE = 0x100;		/* paginate output of this command */
int const MASTERNODE = 0200;/* valid in MASTER node only */

int const LOAD = 0;		/* The various settings changed by chg_lp() */
int const PRIO = 1;

class cmd_parser : public sg_manager
{
public:
	cmd_parser(int flags_);
	virtual ~cmd_parser();

	bool allowed() const;
	void show_err_msg() const;
	void show_desc() const;
	bool enable_page() const;
	virtual parser_result_t parse(int argc, char **argv) = 0;

	static void initialize();
	static int& get_global_flags();
	static bool& get_echo();
	static bool& get_verbose();
	static bool& get_page();
	static void sigintr(int signo);

protected:
	parser_result_t parse_command_line(int argc, char **argv);
	parser_result_t get_proc();
	parser_result_t get_mach();
	int gname2gid(const char *grpname) const;
	std::string gid2gname(int grpid) const;
	bool Ptest(int mid, int grpid, int cmid);

	po::options_description desc;
	po::positional_options_description pdesc;
	boost::shared_ptr<po::variables_map> vm;
	int flags;

	static std::string d_lmid;
	static int d_mid;
	static bool d_allmid;
	static bool d_dbbm_only;
	static std::string d_qname;
	static std::string d_grpname;
	static int d_grpid;
	static int d_svrid;
	static std::string d_svcname;
	static std::string d_usrname;
	static std::string d_cltname;

	static std::string c_lmid;
	static int c_mid;
	static bool c_allmid;
	static bool c_dbbm_only;
	static std::string c_qname;
	static std::string c_grpname;
	static int c_grpid;
	static int c_svrid;
	static std::string c_svcname;
	static std::string c_usrname;
	static std::string c_cltname;

	static bool echo;
	static bool verbose;
	static bool page;

	static int global_flags;
	static sg_proc_t d_proc;
	static std::string err_msg;

	static std::string c_uname;
	static int bbm_count;
	static boost::shared_array<sg_ste_t> auto_bbm_stes;
	static sg_ste_t *bbm_stes;
	static bool gotintr;
};

struct cmd_parser_t
{
	std::string name;
	std::string desc;
	boost::shared_ptr<cmd_parser> parser;
};

}
}

#endif

