#include "admin_server.h"
#include "common_parser.h"
#include "bb_parser.h"
#include "br_parser.h"
#include "chg_parser.h"
#include "prt_parser.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

admin_server::admin_server()
	: global_flags(cmd_parser::get_global_flags()),
	  echo(cmd_parser::get_echo()),
	  verbose(cmd_parser::get_verbose()),
	  page(cmd_parser::get_page())
{
	cmd_parser_t item;

	item.name = "help";
	item.desc = (_("print information of topics")).str(SGLOCALE);
	item.parser.reset(new h_parser(READ | BOOT | CONF | PRV | PAGE, parsers));
	parsers.push_back(item);

	item.name = "default";
	item.desc = (_("set default parameters available for current session")).str(SGLOCALE);
	item.parser.reset(new d_parser(READ | BOOT | CONF | PRV | PAGE));
	parsers.push_back(item);

	item.name = "echo";
	item.desc = (_("toggle echo option")).str(SGLOCALE);
	item.parser.reset(new option_parser("echo", READ | BOOT | CONF | PRV, echo));
	parsers.push_back(item);

	item.name = "verbose";
	item.desc = (_("toggle verbose option")).str(SGLOCALE);
	item.parser.reset(new option_parser("verbose", READ | BOOT | CONF | PRV, verbose));
	parsers.push_back(item);

	item.name = "page";
	item.desc = (_("toggle paginate option")).str(SGLOCALE);
	item.parser.reset(new option_parser("page", READ | BOOT | CONF | PRV, page));
	parsers.push_back(item);

	item.name = "quit";
	item.desc = (_("exit command line")).str(SGLOCALE);
	item.parser.reset(new q_parser(READ | BOOT | CONF | PRV));
	parsers.push_back(item);

	item.name = "exit";
	item.desc = (_("exit command line")).str(SGLOCALE);
	item.parser.reset(new q_parser(READ | BOOT | CONF | PRV));
	parsers.push_back(item);

	item.name = "advertise";
	item.desc = (_("advertise an operation")).str(SGLOCALE);
	item.parser.reset(new adv_parser(ADM | PAGE));
	parsers.push_back(item);

	item.name = "unadvertise";
	item.desc = (_("unadvertise an operation")).str(SGLOCALE);
	item.parser.reset(new una_parser(ADM | PAGE));
	parsers.push_back(item);

	item.name = "bbclean";
	item.desc = (_("cleanup dead entries")).str(SGLOCALE);
	item.parser.reset(new bbc_parser(ADM | PAGE));
	parsers.push_back(item);

	item.name = "bbinternals";
	item.desc = (_("shared shared memory internals")).str(SGLOCALE);
	item.parser.reset(new bbi_parser(ADM | PAGE));
	parsers.push_back(item);

	item.name = "bbmaps";
	item.desc = (_("shared shared memory mappings")).str(SGLOCALE);
	item.parser.reset(new bbm_parser(WIZ | PRV | SM | PAGE));
	parsers.push_back(item);

	item.name = "bbparms";
	item.desc = (_("show shared memory parameters")).str(SGLOCALE);
	item.parser.reset(new bbp_parser(WIZ | PRV | PAGE));
	parsers.push_back(item);

	item.name = "bbrelease";
	item.desc = (_("leave shared memory")).str(SGLOCALE);
	item.parser.reset(new bbr_parser(WIZ | PRV | PAGE));
	parsers.push_back(item);

	item.name = "bbstats";
	item.desc = (_("show system statistics")).str(SGLOCALE);
	item.parser.reset(new bbs_parser(READ | PRV | PAGE));
	parsers.push_back(item);

	item.name = "boot";
	item.desc = (_("startup framework")).str(SGLOCALE);
	item.parser.reset(new system_parser(BOOT, "sgboot", this));
	parsers.push_back(item);

	item.name = "shut";
	item.desc = (_("shutdown framework")).str(SGLOCALE);
	item.parser.reset(new system_parser(ADM, "sgshut", this));
	parsers.push_back(item);

	item.name = "changecost";
	item.desc = (_("change cost for given parameters")).str(SGLOCALE);
	item.parser.reset(new lp_parser(ADM | PAGE, LOAD));
	parsers.push_back(item);

	item.name = "changepriority";
	item.desc = (_("change priority for given parameters")).str(SGLOCALE);
	item.parser.reset(new lp_parser(ADM | PAGE, PRIO));
	parsers.push_back(item);

	item.name = "config";
	item.desc = (_("change configuration")).str(SGLOCALE);
	item.parser.reset(new system_parser(ADM, "sgconfig", this));
	parsers.push_back(item);

	item.name = "dump";
	item.desc = (_("dump distributed shared memory to file")).str(SGLOCALE);
	item.parser.reset(new du_parser(READ | PAGE));
	parsers.push_back(item);

	item.name = "dumpmem";
	item.desc = (_("dump local shared memory to file")).str(SGLOCALE);
	item.parser.reset(new dumem_parser(READ | BOOT | CONF | PRV));
	parsers.push_back(item);

	item.name = "loadmem";
	item.desc = (_("load file to local shared memory")).str(SGLOCALE);
	item.parser.reset(new loadmem_parser(READ | BOOT | WIZ | CONF | PRV));
	parsers.push_back(item);

	item.name = "master";
	item.desc = (_("change master to backup node")).str(SGLOCALE);
	item.parser.reset(new m_parser(MASTER));
	parsers.push_back(item);

	item.name = "migrategroup";
	item.desc = (_("migrate to backup group")).str(SGLOCALE);
	item.parser.reset(new migg_parser(ADM | PAGE));
	parsers.push_back(item);

	item.name = "migratemach";
	item.desc = (_("migrate to backup node")).str(SGLOCALE);
	item.parser.reset(new migm_parser(ADM | PAGE));
	parsers.push_back(item);

	item.name = "pclean";
	item.desc = (_("cleanup partitioned nodes")).str(SGLOCALE);
	item.parser.reset(new pcl_parser(ADM));
	parsers.push_back(item);

	item.name = "printclient";
	item.desc = (_("print client information")).str(SGLOCALE);
	item.parser.reset(new pclt_parser(READ | PAGE));
	parsers.push_back(item);

	item.name = "printgroup";
	item.desc = (_("print group information")).str(SGLOCALE);
	item.parser.reset(new pg_parser(READ | PAGE));
	parsers.push_back(item);

	item.name = "printnet";
	item.desc = (_("print network information")).str(SGLOCALE);
	item.parser.reset(new pnw_parser(READ | PAGE));
	parsers.push_back(item);

	item.name = "printqueue";
	item.desc = (_("print queue information")).str(SGLOCALE);
	item.parser.reset(new pq_parser(READ | PRV | PAGE));
	parsers.push_back(item);

	item.name = "printprocess";
	item.desc = (_("print process information")).str(SGLOCALE);
	item.parser.reset(new psr_parser(READ | PRV | PAGE));
	parsers.push_back(item);

	item.name = "printoperation";
	item.desc = (_("print operation information")).str(SGLOCALE);
	item.parser.reset(new psc_parser(READ | PRV | PAGE));
	parsers.push_back(item);

	item.name = "reconnect";
	item.desc = (_("reconnect to remote node")).str(SGLOCALE);
	item.parser.reset(new rco_parser(READ));
	parsers.push_back(item);

	item.name = "resume";
	item.desc = (_("resume connections")).str(SGLOCALE);
	item.parser.reset(new status_parser(ADM | PAGE, ~SUSPENDED));
	parsers.push_back(item);

	item.name = "suspend";
	item.desc = (_("suspend connections")).str(SGLOCALE);
	item.parser.reset(new status_parser(ADM | PAGE, SUSPENDED));
	parsers.push_back(item);

	item.name = "processparms";
	item.desc = (_("print process parameters")).str(SGLOCALE);
	item.parser.reset(new srp_parser(READ | PRV | PAGE));
	parsers.push_back(item);

	item.name = "operationparms";
	item.desc = (_("print operation information")).str(SGLOCALE);
	item.parser.reset(new scp_parser(READ | PRV | PAGE));
	parsers.push_back(item);

	item.name = "stats";
	item.desc = (_("print statistics")).str(SGLOCALE);
	item.parser.reset(new stats_parser(READ | PAGE));
	parsers.push_back(item);

	item.name = "stopagent";
	item.desc = (_("stop agent")).str(SGLOCALE);
	item.parser.reset(new stopl_parser(BOOT | PRV));
	parsers.push_back(item);

	oldbbtype = NULL;
	exit_code = 0;
}

admin_server::~admin_server()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_api& api_mgr = sg_api::instance(SGT);

	if (SGC->_SGC_amadm)
		api_mgr.sgfini(PT_ADM);
	else
		api_mgr.fini();
}

int admin_server::run()
{
	usignal::signal(SIGINT, cmd_parser::sigintr);
	usignal::signal(SIGHUP, cmd_parser::sigintr);
	usignal::signal(SIGTERM, cmd_parser::sigintr);
	usignal::signal(SIGPIPE, SIG_IGN);

	while (1) {
		if (!(global_flags & CONF))
			maininit();

		cmd_parser::initialize();

		if (process() == PARSE_QUIT)
			break;
	}

	return exit_code;
}

parser_result_t admin_server::process()
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

		vector<cmd_parser_t>::iterator iter;
		boost::shared_ptr<cmd_parser> parser;
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
					usignal::signal(SIGINT, cmd_parser::sigintr);
				}
			} BOOST_SCOPE_EXIT_END
			parser_result_t result = parser->parse(argc, argv.get());

			// rest the output/error streams.
			cout.clear();
			cerr.clear();

			switch (result) {
			case PARSE_SUCCESS:
				break;
			case PARSE_CERR:
				parser->show_err_msg();
				break;
			case PARSE_BBNATT:
				cout << (_("ERROR: No bulletin board attached\n")).str(SGLOCALE);
				break;
			case PARSE_MALFAIL:
				cout << (_("ERROR: Memory malloc failure\n")).str(SGLOCALE);
				break;
			case PARSE_NOQ:
				cout << (_("ERROR: No queues exist\n")).str(SGLOCALE);
				break;
			case PARSE_BADQ:
				cout << (_("ERROR: No such queue\n")).str(SGLOCALE);
				break;
			case PARSE_QERR:
				cout << (_("ERROR: Queue retrieval error\n")).str(SGLOCALE);
				break;
			case PARSE_NOSR:
				cout << (_("ERROR: No processes exist\n")).str(SGLOCALE);
				break;
			case PARSE_BADSR:
				cout << (_("ERROR: No such process\n")).str(SGLOCALE);
				break;
			case PARSE_SRERR:
				cout << (_("ERROR: Process retrieval error\n")).str(SGLOCALE);
				break;
			case PARSE_NOSC:
				cout << (_("ERROR: No operations exist\n")).str(SGLOCALE);
				break;
			case PARSE_BADSC:
				cout << (_("ERROR: No such operation\n")).str(SGLOCALE);
				break;
			case PARSE_SCERR:
				cout << (_("ERROR: Operation retrieval error\n")).str(SGLOCALE);
				break;
			case PARSE_NOBBM:
				cout << (_("ERROR: No sgmngr exists for that node\n")).str(SGLOCALE);
				break;
			case PARSE_BADCOMB:
				cout << (_("ERROR: Invalid combination of options\n")).str(SGLOCALE);
				break;
			case PARSE_ERRSUSP:
				cout << (_("ERROR: Error suspending resources\n")).str(SGLOCALE);
				break;
			case PARSE_NOGID:
				cout << (_("ERROR: Group id is not set\n")).str(SGLOCALE);
				break;
			case PARSE_NOSID:
				cout << (_("ERROR: Process id is not set\n")).str(SGLOCALE);
				break;
			case PARSE_NONAME:
				cout << (_("ERROR: Operation name is not set\n")).str(SGLOCALE);
				break;
			case PARSE_BADMNAM:
				cout << (_("ERROR: No such node\n")).str(SGLOCALE);
				break;
			case PARSE_NOTADM:
				cout << (_("ERROR: Must be administrator to execute that command\n")).str(SGLOCALE);
				break;
			case PARSE_ERRRES:
				cout << (_("ERROR: Error resuming (un-suspending) resources\n")).str(SGLOCALE);
				break;
			case PARSE_QUIT:
				return PARSE_QUIT;
			case PARSE_LOAD:
				return PARSE_LOAD;
			case PARSE_RELBB:
				cout << (_("INFO: Reattaching to regular bulletin board.\n")).str(SGLOCALE);
				global_flags &= ~PRV;
				return PARSE_RELBB;
			case PARSE_PRIVMODE:
				cout << (_("ERROR: Command not allowed in private mode\n")).str(SGLOCALE);
				break;
			case PARSE_BBMHERE:
				cout << (_("ERROR: No sgmngr exists on this node\n")).str(SGLOCALE);
				break;
			case PARSE_BBMDEAD:
				cout << (_("ERROR: sgmngr dead on requested node\n")).str(SGLOCALE);
				break;
			case PARSE_QSUSP:
				cout << (_("ERROR: Queue suspended for sgmngr on requested node\n")).str(SGLOCALE);
				break;
			case PARSE_SRNOTSPEC:
				cout << (_("ERROR: No process is specified\n")).str(SGLOCALE);
				break;
			case PARSE_SCPRMERR:
				cout << (_("ERROR: Error retrieving operation parms\n")).str(SGLOCALE);
				break;
			case PARSE_SYNERR:
				cout << (_("ERROR: Syntax error on command line\n")).str(SGLOCALE);
				break;
			case PARSE_SEMERR:
				cout << (_("ERROR: Error accessing system semaphore\n")).str(SGLOCALE);
				break;
			case PARSE_MSGQERR:
				cout << (_("ERROR: Error accessing message queue\n")).str(SGLOCALE);
				break;
			case PARSE_SHMERR:
				cout << (_("ERROR: Error accessing shared memory\n")).str(SGLOCALE);
				break;
			case PARSE_DUMPERR:
				cout << (_("ERROR: Error dumping bulletin board\n")).str(SGLOCALE);
				break;
			case PARSE_RESNOTSPEC:
				cout << (_("ERROR: No resources are specified\n")).str(SGLOCALE);
				break;
			case PARSE_WIZERR:
				cout << (_("ERROR: Incorrect release code\n")).str(SGLOCALE);
				break;
			case PARSE_MIGFAIL:
				cout << (_("ERROR: sgclst migration failure\n")).str(SGLOCALE);
				break;
			case PARSE_CDBBMFAIL:
				cout << (_("ERROR: Can not create new sgclst\n")).str(SGLOCALE);
				break;
			case PARSE_NOBACKUP:
				cout << (_("ERROR: Backup node is not specified in SGPROFILE file\n")).str(SGLOCALE);
				break;
			case PARSE_NOTBACKUP:
				cout << (_("ERROR: sgadmin not running on backup node\n")).str(SGLOCALE);
				break;
			case PARSE_PCLEANFAIL:
				cout << (_("ERROR: Can not clean up partitioned BB entries\n")).str(SGLOCALE);
				break;
			case PARSE_RECONNFAIL:
				cout << (_("ERROR: Network reconnection failure\n")).str(SGLOCALE);
				break;
			case PARSE_BADGRP:
				cout << (_("ERROR: Invalid process group name\n")).str(SGLOCALE);
				break;
			case PARSE_PCLSELF:
				cout << (_("ERROR: Should not pclean the current node\n")).str(SGLOCALE);
				break;
			case PARSE_HELP:
				result = PARSE_SUCCESS;
				break;
			default:
				break;
			}

			if (result != PARSE_SUCCESS)
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

	return PARSE_QUIT;
}

void admin_server::maininit()
{
	sg_bbparms_t bbparms;
	bool admintry = false;
	int master_mid;
	bool ldbal = false;
	uid_t uid;
	bool administrator;
	sgc_ctx *SGC = SGT->SGC();
	sg_api& api_mgr = sg_api::instance(SGT);

	global_flags |= ADM;

	// get parameters for instantiation; need to get bb type
AGAIN_LABEL:
	if (!SGC->get_bbparms()) {
		cerr << (_("ERROR: Error while obtaining the Bulletin Board parameters\n")).str(SGLOCALE);
		exit(1);
	}

	bbparms = SGC->_SGC_getbbparms_curbbp;

	if (oldbbtype != NULL) {
		int *bbtypeptr = &SGC->_SGC_bbasw->bbtype;
		int savebbtype = SGC->_SGC_bbasw->bbtype;

		SGC->_SGC_bbasw->bbtype &= BBT_MASK;	/* turn off extra stuff */
		if (!SGC->set_bbtype(oldbbtype)) {
			cerr << (_("ERROR: Reinitialization after boot failed to set the Bulletin Board Model\n")).str(SGLOCALE);
			exit(1);
		}
		*bbtypeptr = savebbtype;
	} else {
		oldbbtype = SGC->_SGC_bbasw->name;
	}

	ldbal = false;
	if (strcmp(SGC->_SGC_ldbsw->name, "AUTO") == 0) // get load balancing info after setting up
		ldbal = true;

	// only join the application as administrator if allowed to do so
	uid = geteuid();
	administrator = (uid == 0 || uid == bbparms.uid);
	if (!administrator && !(global_flags & READ)) {
		admintry = true;
		global_flags |= READ;
		global_flags &= ~ADM;
	}

	// try to join as the application administrator
	if (!(global_flags & READ)) {
		admintry = true;
		if (!api_mgr.sginit(PT_ADM, AT_ADMIN, NULL)) {
			// now try to become a client
			global_flags |= READ;
			global_flags &= ~ADM;
		}
	}

	// try to join the application, but not as the administrator
	if (global_flags & READ) {
		sg_init_t init_info;
		struct passwd  *pwd;
		int proctype;
		/*
		 * Do this if _tminit above fails OR -r option
		 * entered on command line.  We also need to set
		 * tmbbtype appropriately so that we attach to shared
		 * memory instead of creating it ourselves.
		 */
		if (!SGC->set_bbtype("MP")) {
			cerr << (_("ERROR: Error setting the Bulleting Board Model to MP\n")).str(SGLOCALE);
			exit(1);
		}

		if ((pwd = getpwuid(uid)) == NULL) {
			cerr << (_("ERROR: Could not find entry for user ")).str(SGLOCALE) << uid << ".\n";
			exit(1);
		}
		strncpy(init_info.usrname, pwd->pw_name, MAX_IDENT);
		init_info.usrname[MAX_IDENT] = '\0';
		strcpy(init_info.cltname, "sysclient");
		init_info.passwd[0] = '\0';
		init_info.grpname[0] = '\0';
		init_info.flags = 0;
		if (administrator)
			proctype = PT_ADMCLT;
		else
			proctype = PT_SYSCLT;

		if (!api_mgr.sginit(proctype, AT_ADMIN, &init_info)) {
			if (SGT->_SGT_error_code == SGEPERM || SGT->_SGT_error_code == SGENOENT) {
				cerr << (_("ERROR: can't become a client - init() failed - ")).str(SGLOCALE) << SGT->strerror() << "\n";
				exit(1);
			}

			/*
			 * Now the system either is not up or is partitioned
			 * because tmadmin can't be a administrator or a client
			 */

			/*
			 * We're going to try to enter boot mode. First we must
			 * set up the private BB for node/nodepe table mappings
			 */
			if (!SGC->set_bbtype("SGBOOT") || !SGC->set_atype("SGBOOT") || !SGC->attach()) {
				cerr << (_("ERROR: Could not setup internal tables to enter boot mode.\n")).str(SGLOCALE);
				exit(1);
			}

			SGC->_SGC_proc.pid = getpid();
			SGC->_SGC_proc.mid = SGC->getmid();

			// Check if the current machine is a master machine.
			const char *master_lmid = bbparms.master[bbparms.current];
			if ((master_mid = SGC->lmid2mid(master_lmid)) == BADMID) {
				cerr << (_("ERROR: Failure to obtain the Master Node identifier.\n")).str(SGLOCALE);
				exit(1);
			}

			const char *master_pmid = SGC->mid2pmid(master_mid);
			string c_uname = GPP->uname();
			if (c_uname != master_pmid) {
				cerr << (_("ERROR: The boot mode is only available on the MASTER processor.\n")).str(SGLOCALE);
				exit(1);
			}

			// Nothing exists, so enter boot mode.
			if (!(global_flags & BOOT)) {
				cerr << (_("No bulletin board exists. ")).str(SGLOCALE);
				cerr << (_("Entering boot mode.\n")).str(SGLOCALE);
			}
			global_flags |= BOOT;

			// If we tried to become an admin
			if (admintry) {
				global_flags &= ~READ;
				global_flags |= ADM;
			}

			return;
		}

		strcpy(SGC->_SGC_crte->cltname, "sgadmin");

		// no dynamic commands (ie, advertise) allowed
		cerr << (_("WARN: Cannot become administrator.")).str(SGLOCALE);
		cerr << (_("Limited set of commands available.\n")).str(SGLOCALE);
	}

	if (global_flags & BOOT) {
		cerr << "\n" << (_("Attaching to active bulletin board.\n")).str(SGLOCALE);
		global_flags &= ~BOOT;
	}

	/*
	 * Update current master index here. since sgadmin creates its own
	 * BB in memory and since the current master index in
	 * SGC->_SGC_bbp->bbparms.current so far has the value 0 which is the default
	 * set by get_bbparms(). This value should be updated to match the
	 * acting master location, especially after sgclst is migrated.
	 */
	master_mid = SGC->lmid2mid(SGC->_SGC_bbp->bbparms.master[SGC->_SGC_bbp->bbparms.current]);
	if (!(global_flags & READ) && SGC->fmaster(master_mid, NULL) == NULL) {
		// try config backup node
		int i;
		for (i = 0; i < MAX_MASTERS; i++) {
			if (i == SGC->_SGC_bbp->bbparms.current)
				continue;

			master_mid = SGC->lmid2mid(SGC->_SGC_bbp->bbparms.master[i]);
			if (master_mid == BADMID)
				continue;

			if (SGC->fmaster(master_mid, NULL) != NULL)
				break;
		}

		if (i == MAX_MASTERS) {
			cout << (_("ERROR: Can not find sgclst on master and backup nodes.\n")).str(SGLOCALE);
			/*
			 * This probably means that we are executing on
			 * a machine remote from the administrator and
			 * tlisten is not running on the master machine or
			 * we just got partitioned.  In this case, don't
			 * allow tmadmin to run as the administrator.
			 * Disconnect and run as a read-only client.
			 */
		 	api_mgr.sgfini(PT_ADM);
			admintry = false;
			global_flags |= READ;
			global_flags &= ~ADM;
			goto AGAIN_LABEL;
		} else {
			SGC->_SGC_bbp->bbparms.current = i;
		}
	}
}

}
}

