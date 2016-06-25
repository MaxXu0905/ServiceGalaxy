#include "cmd_dparser.h"

namespace ai
{
namespace sg
{

bool cmd_dparser::echo = false;
bool cmd_dparser::verbose = false;
bool cmd_dparser::page = true;
std::string cmd_dparser::user_name;
std::string cmd_dparser::password;

int cmd_dparser::global_flags = 0;
std::string cmd_dparser::err_msg;

bool cmd_dparser::gotintr = false;

cmd_dparser::cmd_dparser(int flags_)
	: desc((_("Allowed options")).str(SGLOCALE).c_str()),
	  flags(flags_)
{
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
	;
}

cmd_dparser::~cmd_dparser()
{
}

bool cmd_dparser::allowed() const
{
	// Check that we're in wizard mode if the command requires it.
	if ((global_flags & WIZ) && (flags & WIZ))
		return false;

	if (global_flags & SDC) {
		if (flags & SDC)
			return true;
		else
			return false;
	}

	// if we're in BOOT mode, check if this command is valid
	if (global_flags & BOOT) {
		if (flags & BOOT)
			return true;
		else
			return false;
	}

	if (!(global_flags & ADM)) {
		if (flags & ADM)
			return false;
		else if (flags & READ)
			return true;
		else
			return false;
	}

	return true;
}

void cmd_dparser::show_err_msg() const
{
	if (!err_msg.empty())
		std::cout << err_msg << std::endl;
}

void cmd_dparser::show_desc() const
{
	std::cout << desc << std::endl;
}

bool cmd_dparser::enable_page() const
{
	return (flags & PAGE);
}

void cmd_dparser::initialize()
{
	if (isatty(0) && isatty(1))
		page = true;
	else
		page = false;
}

int& cmd_dparser::get_global_flags()
{
	return global_flags;
}

bool& cmd_dparser::get_echo()
{
	return echo;
}

bool& cmd_dparser::get_verbose()
{
	return verbose;
}

bool& cmd_dparser::get_page()
{
	return page;
}

std::string& cmd_dparser::get_user_name()
{
	return user_name;
}

std::string& cmd_dparser::get_password()
{
	return password;
}

void cmd_dparser::sigintr(int signo)
{
	gotintr = true;
}

dparser_result_t cmd_dparser::parse_command_line(int argc, char **argv)
{
	vm.reset(new po::variables_map());

	try {
		po::store(po::command_line_parser(argc, argv).options(desc).positional(pdesc).run(), *vm);

		if (vm->count("help")) {
			show_desc();
			err_msg = "";
			return DPARSE_HELP;
		}

		po::notify(*vm);
	} catch (exception& ex) {
		err_msg = ex.what();
		return DPARSE_CERR;
	}

	return DPARSE_SUCCESS;
}

}
}

