#if !defined(__CMD_DPARSER_H__)
#define __CMD_DPARSER_H__

#include "dbc_internal.h"

namespace po = boost::program_options;

namespace ai
{
namespace sg
{

using namespace ai::scci;
class dbc_admin;

enum dparser_result_t
{
	DPARSE_SUCCESS = 0,
	DPARSE_CERR,
	DPARSE_LONGARG,
	DPARSE_BADCMD,
	DPARSE_BADOPT,
	DPARSE_ARGCNT,
	DPARSE_DUPARGS,
	DPARSE_NOTADM,
	DPARSE_QUIT,
	DPARSE_LOAD,
	DPARSE_SYNERR,
	DPARSE_SEMERR,
	DPARSE_MSGQERR,
	DPARSE_SHMERR,
	DPARSE_DUMPERR,
	DPARSE_WIZERR,
	DPARSE_HELP
};

// The following are flags used in the "flags" field of command structures.
int const BOOT = 0x1;			/* valid in "boot" mode */
int const READ = 0x2;			/* valid in "read" mode */
int const WIZ = 0x4;			/* valid only in "wizard" mode */
int const ADM = 0x8;			/* valid only if administrator */
int const SDC = 0x10;			/* RUN IN SDC mode */
int const PAGE = 0x20;			/* paginate output of this command */

class cmd_dparser : public dbc_manager
{
public:
	cmd_dparser(int flags_);
	virtual ~cmd_dparser();

	bool allowed() const;
	void show_err_msg() const;
	void show_desc() const;
	bool enable_page() const;
	virtual dparser_result_t parse(int argc, char **argv) = 0;

	static void initialize();
	static int& get_global_flags();
	static bool& get_echo();
	static bool& get_verbose();
	static bool& get_page();
	static std::string& get_user_name();
	static std::string& get_password();
	static void sigintr(int signo);

protected:
	dparser_result_t parse_command_line(int argc, char **argv);

	po::options_description desc;
	po::positional_options_description pdesc;
	boost::shared_ptr<po::variables_map> vm;
	int flags;

	static bool echo;
	static bool verbose;
	static bool page;
	static std::string user_name;
	static std::string password;

	static int global_flags;
	static std::string err_msg;

	static bool gotintr;
};

struct cmd_dparser_t
{
	std::string name;
	std::string desc;
	boost::shared_ptr<cmd_dparser> parser;
};

}
}

#endif

