#include "common_parser.h"
#include "admin_server.h"

namespace bi = boost::interprocess;
namespace bio = boost::io;
namespace po = boost::program_options;
using namespace std;

namespace ai
{
namespace sg
{

h_parser::h_parser(int flags_, const std::vector<cmd_parser_t>& parsers_)
	: cmd_parser(flags_),
	  parsers(parsers_)
{
	desc.add_options()
		("topic,t", po::value<string>(&topic)->default_value(""), (_("produce information of specified topic, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;

	pdesc.add("topic", 1);
}

h_parser::~h_parser()
{
}

parser_result_t h_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return show_topic();
}

parser_result_t h_parser::show_topic() const
{
	std::vector<cmd_parser_t>::const_iterator iter;

	if (topic.empty()) {
		cout << (_("Allowed topics:\n")).str(SGLOCALE);

		int maxlen = 0;
		for (iter = parsers.begin(); iter != parsers.end(); ++iter) {
			const cmd_parser_t& item = *iter;
			if (item.parser->allowed()) {
				if (maxlen < item.name.length())
					maxlen = item.name.length();
			}
		}

		for (iter = parsers.begin(); iter != parsers.end(); ++iter) {
			const cmd_parser_t& item = *iter;
			if (item.parser->allowed()) {
				cout << "  " << item.name;
				int len = maxlen - item.name.length() + 4;
				if (len > 0)
					cout << setfill(' ') << setw(len) << ' ';
				cout << item.desc << "\n";
			}
		}

		return PARSE_SUCCESS;
	} else {
		int match = 0;
		vector<cmd_parser_t>::const_iterator item;
		for (iter = parsers.begin(); iter != parsers.end(); ++iter) {
			if (strncasecmp(iter->name.c_str(), topic.c_str(), topic.length()) == 0
				&& iter->parser->allowed()) {
				item = iter;
				match++;
			}
		}

		if (match == 0) {
			err_msg = (_("ERROR: Can't find given topic {1}") % topic).str(SGLOCALE);
			return PARSE_CERR;
		} else if (match > 1) {
			err_msg = (_("ERROR: Misleading topic {1}") % topic).str(SGLOCALE);
			return PARSE_CERR;
		}

		item->parser->show_desc();
		return PARSE_SUCCESS;
	}
}

q_parser::q_parser(int flags_)
	: cmd_parser(flags_)
{
}

q_parser::~q_parser()
{
}

parser_result_t q_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return PARSE_QUIT;
}

system_parser::system_parser(int flags_, const string& command_, admin_server *admin_mgr_)
	: cmd_parser(flags_),
	  command(command_),
	  admin_mgr(admin_mgr_)
{
}

system_parser::~system_parser()
{
}

parser_result_t system_parser::parse(int argc, char **argv)
{
	/*
	 * If we're not in boot, then we need to get the pid of the
	 * (D)sgmngr so we can notice if a complete shutdown occurred later.
	 */
	if (!(global_flags & BOOT)) {
		sg_scte_t vkey;
		sg_scte& scte_mgr = sg_scte::instance(SGT);

		/*
		 * If we're in shared memory mode, then a sgmngr exists, so
		 * get the entry called "..ADJUNCTBB". In MP mode, a sgclst
		 * exists, so get "..MASTERBB".
		 */
		if (global_flags & SM)
			strcpy(vkey.parms.svc_name, BBM_SVC_NAME);
		else
			strcpy(vkey.parms.svc_name, DBBM_SVC_NAME);

		vkey.mid() = ALLMID;

		// now retrieve the service table entry
		if (scte_mgr.retrieve(S_MACHINE, &vkey, &vkey, NULL) < 0) {
			masterpid = -1;
			return PARSE_SUCCESS;
		}

		mastermid = vkey.mid();
		masterpid = vkey.pid();
	}

	return do_system(command, argc, argv);
}

parser_result_t system_parser::do_system(const string& command, int argc, char **argv)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_api& api_mgr = sg_api::instance(SGT);

	/*
	 * Boot or stop the indicated resources. Also, depending on
	 * the mode (boot, stop, or config), we must detach, attach,
	 * reattach, etc.
	 */

	/*
	 * If we're calling tmshutdown, we need to detach first just in
	 * case this is a complete shutdown.
	 */
	if (command == "sgshut") {
		if(SGC->midnidx(SGC->_SGC_proc.mid) != SGC->midnidx(mastermid)) {
			err_msg = (_("ERROR: Cannot stop processes from non-master node")).str(SGLOCALE);
			return PARSE_CERR;
		}
		if (SGC->_SGC_amadm)
			api_mgr.sgfini(PT_ADM);
		else
			api_mgr.fini();
	} else if (command == "sgconfig") {
		// detach - just in case a new machine or group are added
		if (SGC->_SGC_amadm)
			api_mgr.sgfini(PT_ADM);
		else
			api_mgr.fini();
	}

	string str = command;
	for (int i = 1; i < argc; i++) {
		bool contain_space = false;
		int len = strlen(argv[i]);

		for (int j = 0; j < len; j++) {
			if (isspace(argv[i][j])) {
				contain_space = true;
				break;
			}
		}

		if (contain_space) {
			str += " \"";
			str += argv[i];
			str += "\"";
		} else {
			str += " ";
			str += argv[i];
		}
	}

	sys_func& func_mgr = sys_func::instance();
	int status = func_mgr.gp_status(::system(str.c_str()));
	if (status != 0)
		cerr << (_("ERROR: Can't execute {1} - status {2}, errno {3}") % command % status % errno).str(SGLOCALE) << std::endl;

	if (command == "sgboot") {
		if (status == 0 && (global_flags & BOOT)) {
			// in boot mode and boot succeeded - initialize
			SGC->detach();
			global_flags &= ~BOOT;

			cout << (_("Attaching to active bulletin board.\n")).str(SGLOCALE);
			admin_mgr->maininit();
		}
	} else if (command == "sgconfig") {
		// reinitialize
		admin_mgr->maininit();
	} else {
		/*
		 * If masterpid is now a dead process, that means that this
		 * is a complete shutdown, so we should detach.
		 */
		sg_proc_t proc;
		sg_proc& proc_mgr = sg_proc::instance(SGT);
		proc.pid = masterpid;
		proc.mid = mastermid;
		if (!proc_mgr.alive(proc)) {
			cout << "\n" << (_("Complete system shutdown.")).str(SGLOCALE);
			cout << (_("Returning to boot mode.\n")).str(SGLOCALE);
			global_flags |= BOOT;
		}

		// Either way, we need to reinitialize ourselves.
		admin_mgr->maininit();
	}

	return PARSE_SUCCESS;
}

}
}

