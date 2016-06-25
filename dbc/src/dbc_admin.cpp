#include "dbc_admin.h"
#include "common_dparser.h"
#include "bb_dparser.h"
#include "prt_dparser.h"
#include "sql_dparser.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;
namespace bio = boost::io;
using namespace std;
using namespace ai::sg;
using namespace ai::scci;

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

int main(int argc, char **argv)
{
	try {
		dbc_admin instance;
		return instance.run(argc, argv);
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		return 1;
	}
}

namespace ai
{
namespace sg
{

dbc_admin::dbc_admin()
	: global_flags(cmd_dparser::get_global_flags()),
	  echo(cmd_dparser::get_echo()),
	  verbose(cmd_dparser::get_verbose()),
	  page(cmd_dparser::get_page()),
	  user_name(cmd_dparser::get_user_name()),
	  password(cmd_dparser::get_password()),
	  api_mgr(dbc_api::instance(DBCT))
{
	dbc_db = NULL;
	DBCP->_DBCP_is_admin = true;

	cmd_dparser_t item;

	item.name = "help";
	item.desc = (_("print information of topics")).str(SGLOCALE);
	item.parser.reset(new h_dparser(READ | SDC | PAGE | BOOT, parsers));
	parsers.push_back(item);

	item.name = "echo";
	item.desc = (_("toggle echo option")).str(SGLOCALE);
	item.parser.reset(new option_dparser("echo", READ | SDC | BOOT, echo));
	parsers.push_back(item);

	item.name = "verbose";
	item.desc = (_("toggle verbose option")).str(SGLOCALE);
	item.parser.reset(new option_dparser("verbose", READ | SDC | BOOT, verbose));
	parsers.push_back(item);

	item.name = "page";
	item.desc = (_("toggle paginate option")).str(SGLOCALE);
	item.parser.reset(new option_dparser("page", READ | SDC | BOOT, page));
	parsers.push_back(item);

	item.name = "quit";
	item.desc = (_("exit command line")).str(SGLOCALE);
	item.parser.reset(new q_dparser(READ | SDC | BOOT));
	parsers.push_back(item);

	item.name = "exit";
	item.desc = (_("exit command line")).str(SGLOCALE);
	item.parser.reset(new q_dparser(READ | SDC | BOOT));
	parsers.push_back(item);

	item.name = "bbclean";
	item.desc = (_("cleanup dead entries")).str(SGLOCALE);
	item.parser.reset(new bbc_dparser(READ | SDC | ADM));
	parsers.push_back(item);

	item.name = "bbparms";
	item.desc = (_("show shared memory parameters")).str(SGLOCALE);
	item.parser.reset(new bbp_dparser(READ | SDC | PAGE));
	parsers.push_back(item);

	item.name = "boot";
	item.desc = (_("startup dbc server")).str(SGLOCALE);
	item.parser.reset(new system_dparser(BOOT | ADM, "boot", this));
	parsers.push_back(item);

	item.name = "shut";
	item.desc = (_("shutdown dbc server")).str(SGLOCALE);
	item.parser.reset(new system_dparser(ADM, "shut", this));
	parsers.push_back(item);

	item.name = "desc";
	item.desc = (_("show description of a table")).str(SGLOCALE);
	item.parser.reset(new desc_dparser(READ | SDC | PAGE));
	parsers.push_back(item);

	item.name = "select";
	item.desc = (_("execute select SQL statement")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC | PAGE, "select", this));
	parsers.push_back(item);

	item.name = "insert";
	item.desc = (_("execute insert SQL statement")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC, "insert", this));
	parsers.push_back(item);

	item.name = "update";
	item.desc = (_("execute update SQL statement")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC, "update", this));
	parsers.push_back(item);

	item.name = "delete";
	item.desc = (_("execute delete SQL statement")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC, "delete", this));
	parsers.push_back(item);

	item.name = "truncate";
	item.desc = (_("execute truncate SQL statement")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC, "truncate", this));
	parsers.push_back(item);

	item.name = "create";
	item.desc = (_("execute create SQL statement")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC, "create", this));
	parsers.push_back(item);

	item.name = "drop";
	item.desc = (_("execute drop SQL statement")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC, "drop", this));
	parsers.push_back(item);

	item.name = "alter";
	item.desc = (_("execute alter SQL statement")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC, "alter", this));
	parsers.push_back(item);

	item.name = "enable";
	item.desc = (_("enable index of table")).str(SGLOCALE);
	item.parser.reset(new enable_dparser(READ | SDC));
	parsers.push_back(item);

	item.name = "disable";
	item.desc = (_("disable index of table")).str(SGLOCALE);
	item.parser.reset(new disable_dparser(READ | SDC));
	parsers.push_back(item);

	item.name = "commit";
	item.desc = (_("commit current transaction")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC, "commit", this));
	parsers.push_back(item);

	item.name = "rollback";
	item.desc = (_("rollback current transaction")).str(SGLOCALE);
	item.parser.reset(new sql_dparser(READ | SDC, "rollback", this));
	parsers.push_back(item);

	item.name = "reload";
	item.desc = (_("reload given table(s)")).str(SGLOCALE);
	item.parser.reset(new reload_dparser(READ | SDC));
	parsers.push_back(item);

	item.name = "dump";
	item.desc = (_("dump given table to file")).str(SGLOCALE);
	item.parser.reset(new dump_dparser(READ | SDC));
	parsers.push_back(item);

	item.name = "dumpdb";
	item.desc = (_("dump given table to database")).str(SGLOCALE);
	item.parser.reset(new dumpdb_dparser(READ | SDC));
	parsers.push_back(item);

	item.name = "extend";
	item.desc = (_("extend shared memory usage")).str(SGLOCALE);
	item.parser.reset(new extend_dparser(ADM | SDC));
	parsers.push_back(item);

	item.name = "tep";
	item.desc = (_("print a summary of given table's parameters")).str(SGLOCALE);
	item.parser.reset(new tep_dparser(READ | SDC));
	parsers.push_back(item);

	item.name = "show";
	item.desc = (_("show data for session, parameter, table, sequence, or stat")).str(SGLOCALE);
	item.parser.reset(new show_dparser(READ | SDC | PAGE));
	parsers.push_back(item);

	item.name = "kill";
	item.desc = (_("kill an active session")).str(SGLOCALE);
	item.parser.reset(new kill_dparser(READ | SDC | PAGE));
	parsers.push_back(item);

	item.name = "set";
	item.desc = (_("set parameters for current session")).str(SGLOCALE);
	item.parser.reset(new set_dparser(READ | SDC));
	parsers.push_back(item);

	item.name = "forget";
	item.desc = (_("forget a transaction")).str(SGLOCALE);
	item.parser.reset(new forget_dparser(READ | SDC));
	parsers.push_back(item);

	item.name = "check";
	item.desc = (_("check table consistency for given index")).str(SGLOCALE);
	item.parser.reset(new check_dparser(READ | SDC));
	parsers.push_back(item);

	exit_code = 0;
}

dbc_admin::~dbc_admin()
{
	delete dbc_db;

	if (global_flags & SDC) {
		sg_api& sg_mgr = sg_api::instance(SGT);

		sg_mgr.fini();
	}
}

int dbc_admin::run(int argc, char *argv[])
{
	string dbc_key;
	int dbc_version;
	string sdc_key;
	int sdc_version;
	string sginfo;

	GPP->set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("version,v", (_("print current DBC version")).str(SGLOCALE).c_str())
		("readonly,r", (_("run in readonly mode")).str(SGLOCALE).c_str())
		("sginfo,i", po::value<string>(&sginfo), (_("specify login info")).str(SGLOCALE).c_str())
		("dbckey,d", po::value<string>(&dbc_key), (_("specify DBC's configuration key")).str(SGLOCALE).c_str())
		("dbcversion,y", po::value<int>(&dbc_version)->default_value(-1), (_("specify DBC's configuration version")).str(SGLOCALE).c_str())
		("sdckey,s", po::value<string>(&sdc_key), (_("specify SDC's configuration key")).str(SGLOCALE).c_str())
		("sdcversion,z", po::value<int>(&sdc_version)->default_value(-1), (_("specify SDC's configuration version")).str(SGLOCALE).c_str())
		("username,u", po::value<string>(&user_name)->default_value("system"), (_("specify DBC user name")).str(SGLOCALE).c_str())
		("password,p", po::value<string>(&password)->default_value("system"), (_("specify DBC password")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			::exit(0);
		} else if (vm.count("version")) {
			std::cout << (_("DBC version ")).str(SGLOCALE) << DBC_RELEASE << std::endl;
			std::cout << (_("Compiled on ")).str(SGLOCALE) << __DATE__ << " " << __TIME__ << std::endl;
			::exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		::exit(1);
	}

	if (vm.count("readonly"))
		global_flags |= READ;
	else
		global_flags &= ~READ;

	if (!dbc_key.empty() || !sdc_key.empty()) {
		DBCP->_DBCP_proc_type = DBC_SLAVE;
		global_flags |= SDC;

		sg_init_t init_info;
		if (!parse(init_info, sginfo)) {
			std::cout << (_("ERROR: Invalid sginfo given\n")).str(SGLOCALE);
			::exit(1);
		}

		sg_api& sg_mgr = sg_api::instance(SGT);
		if (!sg_mgr.init(&init_info)) {
			std::cout << (_("ERROR: Can't login to ServiceGalaxy domain\n")).str(SGLOCALE);
			::exit(1);
		}

		if (!DBCP->get_config(dbc_key, dbc_version, sdc_key, sdc_version)) {
			std::cout << (_("ERROR: Can't get DBC/SDC config\n")).str(SGLOCALE);
			::exit(1);
		}

		sdc_config& config_mgr = sdc_config::instance(DBCT);
		set<sdc_config_t>& config_set = config_mgr.get_config_set();
		for (set<sdc_config_t>::const_iterator iter = config_set.begin(); iter != config_set.end(); ++iter)
			mids.insert(iter->mid);

		// 获取Master节点列表
		if (!DBCP->get_mlist(SGT)) {
			std::cout << (_("ERROR: Can't get master nodes\n")).str(SGLOCALE);
			::exit(1);
		}
	}

	usignal::signal(SIGINT, cmd_dparser::sigintr);
	usignal::signal(SIGHUP, cmd_dparser::sigintr);
	usignal::signal(SIGTERM, cmd_dparser::sigintr);
	usignal::signal(SIGPIPE, SIG_IGN);

	while (1) {
		if (!(global_flags & BOOT))
			maininit();

		cmd_dparser::initialize();

		if (process() == DPARSE_QUIT)
			break;
	}

	return exit_code;
}

dparser_result_t dbc_admin::process()
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sys_func& func_mgr = sys_func::instance();
	_gp_pinfo *pHandle;

	string prev_cmd;
	boost::char_separator<char> sep(" \t\b");

	// rest the output/error streams.
	cout.clear();
	cerr.clear();

	cout << "> " << flush;
	while (std::getline(cin, cmd)) {
		if (cmd == " ")
			cmd = prev_cmd;

		if (echo)
			cout << cmd << "\n";

		vector<string> token_vector;
		tokenizer tokens(cmd, sep);

		for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
			token_vector.push_back(*iter);

		if (token_vector.empty()) {
			cout << "> " << flush;
			continue;
		}

		vector<cmd_dparser_t>::iterator iter;
		boost::shared_ptr<cmd_dparser> parser;
		int match = 0;
		for (iter = parsers.begin(); iter != parsers.end(); ++iter) {
			if (!iter->parser->allowed())
				continue;

			string::const_iterator iter1 = iter->name.begin();
			string::const_iterator iter2 = token_vector[0].begin();
			while (iter1 != iter->name.end()) {
				if (*iter1 == *iter2) {
					if (++iter2 == token_vector[0].end())
						break;
				}
				++iter1;
			}

			if (iter1 != iter->name.end()) {
				parser = iter->parser;
				if (iter->name.length() == token_vector[0].length()) {
					match = 1;
					break;
				}
				match++;
			}
		}

		if (match == 0) {
			cout << (_("ERROR: Unknown command: ")).str(SGLOCALE) << cmd << endl;
			cout << "> " << flush;
			exit_code = 1;
			continue;
		} else if (match > 1) {
			cout << (_("ERROR: Misleading command: ")).str(SGLOCALE) << cmd << endl;
			cout << "> " << flush;
			exit_code = 1;
			continue;
		}

		int argc = token_vector.size();
		boost::scoped_array<char *> argv(new char *[argc]);

		for (int i = 0; i < argc; i++)
			argv[i] = const_cast<char *>(token_vector[i].c_str());

		try {
			// call the proper routine
			bool paginate = false;
			if (page && parser->enable_page()) {
				usignal::signal(SIGINT, SIG_IGN);
				paginate = func_mgr.page_start(&pHandle);
			}
			BOOST_SCOPE_EXIT((&func_mgr)(&paginate)(&pHandle)) {
				if (paginate) {
					func_mgr.page_finish(&pHandle);
					usignal::signal(SIGINT, cmd_dparser::sigintr);
				}
			} BOOST_SCOPE_EXIT_END
			dparser_result_t result = parser->parse(argc, argv.get());

			// rest the output/error streams.
			cout.clear();
			cerr.clear();

			switch (result) {
			case DPARSE_SUCCESS:
				break;
			case DPARSE_CERR:
				parser->show_err_msg();
				break;
			case DPARSE_LONGARG:
				cout << (_("ERROR: Argument too long\n")).str(SGLOCALE);
				break;
			case DPARSE_BADCMD:
				cout << (_("ERROR: No such command\n")).str(SGLOCALE);
				break;
			case DPARSE_BADOPT:
				cout << (_("ERROR: Invalid option\n")).str(SGLOCALE);
				break;
			case DPARSE_ARGCNT:
				cout << (_("ERROR: Too many arguments\n")).str(SGLOCALE);
				break;
			case DPARSE_DUPARGS:
				cout << (_("ERROR: Duplicate arguments\n")).str(SGLOCALE);
				break;
			case DPARSE_NOTADM:
				cout << (_("ERROR: Must be administrator to execute that command")).str(SGLOCALE);
				break;
			case DPARSE_QUIT:
				return DPARSE_QUIT;
			case DPARSE_LOAD:
				return DPARSE_LOAD;
			case DPARSE_SYNERR:
				cout << (_("ERROR: Syntax error on command line")).str(SGLOCALE);
				break;
			case DPARSE_SEMERR:
				cout << (_("ERROR: Error accessing system semaphore")).str(SGLOCALE);
				break;
			case DPARSE_MSGQERR:
				cout << (_("ERROR: Error accessing message queue.")).str(SGLOCALE);
				break;
			case DPARSE_SHMERR:
				cout << (_("ERROR: Error accessing shared memory")).str(SGLOCALE);
				break;
			case DPARSE_DUMPERR:
				cout << (_("ERROR: Error dumping bulletin board")).str(SGLOCALE);
				break;
			case DPARSE_WIZERR:
				cout << (_("ERROR: Incorrect release code.")).str(SGLOCALE);
				break;
			case DPARSE_HELP:
				result = DPARSE_SUCCESS;
				break;
			default:
				break;
			}

			if (result != DPARSE_SUCCESS)
				exit_code = 1;
		} catch (exception& ex) {
			// rest the output/error streams.
			cout.clear();
			cerr.clear();

			cout << ex.what() << endl;
			exit_code = 1;
		}

		rpc_mgr.setraddr(NULL);
		cout << "> " << flush;
	}

	return DPARSE_QUIT;
}

void dbc_admin::maininit()
{
	delete dbc_db;
	api_mgr.disconnect();
	api_mgr.logout();

	if (!api_mgr.login()) {
		global_flags |= BOOT;
		cout << (_("WARN: dbc_server is not alive, start in normal mode.\n")).str(SGLOCALE);
		return;
	}

	if (!api_mgr.connect(user_name, password)) {
		global_flags |= BOOT;
		cout << (_("ERROR: permission denied, start in normal mode.\n")).str(SGLOCALE);
		return;
	}

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	if (bbparms.proc_type != DBC_STANDALONE && DBCP->_DBCP_proc_type == DBC_STANDALONE) {
		global_flags &= ~ADM;
		cout << (_("INFO: run in readonly mode since DBC is running in cluster, but dbc_admin is in standalone.")).str(SGLOCALE) << "\n";
	}

	if (global_flags & READ) {
		global_flags &= ~ADM;
	} else {
		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		if (rte_mgr.get_rte() != DBCT->_DBCT_rtes + RTE_SLOT_ADMIN)
			global_flags &= ~ADM;
		else
			global_flags |= ADM;
	}

	map<string, string> conn_info;
	conn_info["user_name"] = user_name;
	conn_info["password"] = password;

	dbc_db = new Dbc_Database();
	dbc_db->connect(conn_info);
	global_flags &= ~BOOT;
}

string& dbc_admin::get_cmd()
{
	return cmd;
}

Dbc_Database * dbc_admin::get_db()
{
	return dbc_db;
}

std::set<int>& dbc_admin::get_mids()
{
	return mids;
}

const std::string& dbc_admin::get_user_name() const
{
	return user_name;
}

const std::string& dbc_admin::get_password() const
{
	return password;
}

bool dbc_admin::parse(sg_init_t& init_info, const string& sginfo)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, (_("sginfo={1}") % sginfo).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance();
	boost::escaped_list_separator<char> sep('\\', ';', '\"');
	boost::tokenizer<boost::escaped_list_separator<char> > tok(sginfo, sep);

	memset(&init_info, '\0', sizeof(init_info));

	for (boost::tokenizer<boost::escaped_list_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter) {
		string str = *iter;

		string::size_type pos = str.find('=');
		if (pos == string::npos)
			continue;

		string key(str.begin(), str.begin() + pos);
		string value(str.begin() + pos + 1, str.end());

		if (key.empty())
			continue;

		if (key[0] == '@') {
			char text[4096];
			common::decrypt(value.c_str(), text);
			value = text;
			key.assign(key.begin() + 1, key.end());
		}

		if (strcasecmp(key.c_str(), "usrname") == 0) {
			if (value.length() >= sizeof(init_info.usrname)) {
				api_mgr.write_log(__FILE__, __LINE__, (_("ERROR: usrname on sginfo too long")).str(SGLOCALE));
				return retval;
			}
			memcpy(init_info.usrname, value.c_str(), value.length() + 1);
		} else if (strcasecmp(key.c_str(), "cltname") == 0) {
			if (value.length() >= sizeof(init_info.cltname)) {
				api_mgr.write_log(__FILE__, __LINE__, (_("ERROR: cltname on sginfo too long")).str(SGLOCALE));
				return retval;
			}
			memcpy(init_info.cltname, value.c_str(), value.length() + 1);
		} else if (strcasecmp(key.c_str(), "passwd") == 0) {
			if (value.length() >= sizeof(init_info.passwd)) {
				api_mgr.write_log(__FILE__, __LINE__, (_("ERROR: passwd on sginfo too long")).str(SGLOCALE));
				return retval;
			}
			memcpy(init_info.passwd, value.c_str(), value.length() + 1);
		} else if (strcasecmp(key.c_str(), "grpname") == 0) {
			if (value.length() >= sizeof(init_info.grpname)) {
				api_mgr.write_log(__FILE__, __LINE__, (_("ERROR: grpname on sginfo too long")).str(SGLOCALE));
				return retval;
			}
			memcpy(init_info.grpname, value.c_str(), value.length() + 1);
		}
	}

	retval = true;
	return retval;
}

}
}

