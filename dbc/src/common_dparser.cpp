#include "common_dparser.h"
#include "dbc_admin.h"

namespace bi = boost::interprocess;
namespace bio = boost::io;
namespace po = boost::program_options;
using namespace std;

namespace ai
{
namespace sg
{

h_dparser::h_dparser(int flags_, const std::vector<cmd_dparser_t>& parsers_)
	: cmd_dparser(flags_),
	  parsers(parsers_)
{
	desc.add_options()
		("topic,t", po::value<string>(&topic)->default_value(""), (_("produce information of specified topic, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;

	pdesc.add("topic", 1);
}

h_dparser::~h_dparser()
{
}

dparser_result_t h_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	return show_topic();
}

dparser_result_t h_dparser::show_topic() const
{
	std::vector<cmd_dparser_t>::const_iterator iter;

	if (topic.empty()) {
		cout << (_("Allowed topics:\n")).str(SGLOCALE);

		int maxlen = 0;
		for (iter = parsers.begin(); iter != parsers.end(); ++iter) {
			const cmd_dparser_t& item = *iter;
			if (item.parser->allowed()) {
				if (maxlen < item.name.length())
					maxlen = item.name.length();
			}
		}

		for (iter = parsers.begin(); iter != parsers.end(); ++iter) {
			const cmd_dparser_t& item = *iter;
			if (item.parser->allowed()) {
				cout << "  " << item.name;
				int len = maxlen - item.name.length() + 4;
				cout << setfill(' ') << setw(len) << ' ';
				cout << item.desc << "\n";
			}
		}

		return DPARSE_SUCCESS;
	} else {
		int match = 0;
		vector<cmd_dparser_t>::const_iterator item;
		for (iter = parsers.begin(); iter != parsers.end(); ++iter) {
			if (strncasecmp(iter->name.c_str(), topic.c_str(), topic.length()) == 0
				&& iter->parser->allowed()) {
				item = iter;
				match++;
			}
		}

		if (match == 0) {
			err_msg = (_("ERROR: Can't find given topic {1}") % topic).str(SGLOCALE);
			return DPARSE_CERR;
		} else if (match > 1) {
			err_msg = (_("ERROR: Misleading topic {1}") % topic).str(SGLOCALE);
			return DPARSE_CERR;
		}

		item->parser->show_desc();
		return DPARSE_SUCCESS;
	}
}

q_dparser::q_dparser(int flags_)
	: cmd_dparser(flags_)
{
}

q_dparser::~q_dparser()
{
}

dparser_result_t q_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	return DPARSE_QUIT;
}

system_dparser::system_dparser(int flags_, const string& command_, dbc_admin *admin_mgr_)
	: cmd_dparser(flags_),
	  command(command_),
	  admin_mgr(admin_mgr_)
{
}

system_dparser::~system_dparser()
{
}

dparser_result_t system_dparser::parse(int argc, char **argv)
{
	return do_system(command, argc, argv);
}

dparser_result_t system_dparser::do_system(const string& command, int argc, char **argv)
{
	if (command == "boot") {
		return do_boot();
	} else if (command == "shut") {
		return do_shut();
	} else {
		err_msg = (_("ERROR: unknown command {1} given.") % command).str(SGLOCALE);
		return DPARSE_CERR;
	}

	return DPARSE_SUCCESS;
}

dparser_result_t system_dparser::do_boot()
{
	int i;
	string cmd = admin_mgr->get_cmd();
	string command = "dbc_server";
	for (i = 0; i < cmd.length() && !isspace(cmd[i]); i++)
	        ;

	command += cmd.substr(i);
	if (command.find("-b") == string::npos)
		command += " -b";

	sys_func& sys_mgr = sys_func::instance();
	int status = sys_mgr.gp_status(::system(command.c_str()));
	if (status != 0) {
		err_msg = (_("ERROR: Failure return from {1}, status {2}, {3}") % command % status % strerror(errno)).str(SGLOCALE);
		return DPARSE_CERR;
	}

	sleep(1);
	admin_mgr->maininit();

	if (global_flags & BOOT) {
		err_msg = (_("ERROR: Failed to boot processes.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	cout << "\n" << (_("INFO: DBC processes booted successfully.\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

dparser_result_t system_dparser::do_shut()
{
	dbc_rte_t *rte = DBCT->_DBCT_rtes + RTE_SLOT_SERVER;

	int timeout = 60;
	while (1) {
		if (kill(rte->pid, SIGTERM) == -1) {
			cout << "\n" << (_("INFO: DBC processes shutdown successfully.\n")).str(SGLOCALE);
			global_flags |= BOOT;
			return DPARSE_SUCCESS;
		}

		if (timeout-- > 0)
			sleep(1);
	}

	err_msg = "ERROR: Failed to stop DBC servers.";
	return DPARSE_CERR;
}

}
}

