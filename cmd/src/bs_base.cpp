#include "bs_base.h"
#include "boot_switch.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

int bs_base::bsflags = BS_VERBOSE;
bool bs_base::gotintr = false;
bool bs_base::handled = false;
bool bs_base::hurry = false;

bs_base::bs_base()
{
	GPP = gpp_ctx::instance();
	SGP = sgp_ctx::instance();
	SGT = sgt_ctx::instance();

	prompt = true;
	opflags = 0;
	byid = false;
	bootmin = -1;
	boottimeout = 60;
	who = 0;

	dbbm_mid = BADMID;
	dbbm_act = false;
	exit_code = 0;

	master_proc.clear();
}

bs_base::~bs_base()
{
}

void bs_base::init(int argc, char **argv)
{
	sg_tditem_t item;
	string bbm_optarg;
	string grpname_optarg;
	int grpid_optarg;
	string lmid_optarg;
	int svrid;
	string svrname_optarg;
	string kill_str;
	sgc_ctx *SGC = SGT->SGC();
	gpenv& env_mgr = gpenv::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_bbparms_t& bbparms = config_mgr.get_bbparms();
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}, argv={2}") % argc % argv).str(SGLOCALE), NULL);
#endif

	SGC->_SGC_proc.rqaddr = -1;

	GPP->set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));

	if (GPP->_GPP_procname == "sgboot") {
		bsflags |= BS_BOOTING;

		if (env_mgr.getenv("SGROOT") == NULL)
			fatal((_("ERROR: SGROOT environment variable not set")).str(SGLOCALE));

		desc.add_options()
			("help", (_("produce help message")).str(SGLOCALE).c_str())
			("admin,A", (_("boot all admin processes")).str(SGLOCALE).c_str())
			("mngr,B", po::value<string>(&bbm_optarg), (_("boot sgmngr on a particular host id")).str(SGLOCALE).c_str())
			("process,P", (_("boot all processes")).str(SGLOCALE).c_str())
			("master,M", (_("boot the master node")).str(SGLOCALE).c_str())
			("pgname,G", po::value<string>(&grpname_optarg), (_("boot a particular process group")).str(SGLOCALE).c_str())
			("pgid,g", po::value<int>(&grpid_optarg)->default_value(-1), (_("boot a particular process group id")).str(SGLOCALE).c_str())
			("prcid,p", po::value<int>(&svrid)->default_value(-1), (_("boot a particular process id")).str(SGLOCALE).c_str())
			("prcname,n", po::value<string>(&svrname_optarg), (_("boot by process name")).str(SGLOCALE).c_str())
			("hostid,h", po::value<string>(&lmid_optarg), (_("boot processes on a particular host id")).str(SGLOCALE).c_str())
			("min,m", po::value<int>(&bootmin), (_("min processes to be booted")).str(SGLOCALE).c_str())
			("yes,y", (_("verify full boot")).str(SGLOCALE).c_str())
			("quiet,q", (_("quiet mode")).str(SGLOCALE).c_str())
		;
	} else {
		desc.add_options()
			("help", (_("produce help message")).str(SGLOCALE).c_str())
			("admin,A", (_("shutdown all admin processes")).str(SGLOCALE).c_str())
			("mngr,B", po::value<string>(&bbm_optarg), (_("shutdown sgmngr on a particular host id")).str(SGLOCALE).c_str())
			("process,P", (_("shutdown all processes")).str(SGLOCALE).c_str())
			("master,M", (_("shutdown the master node")).str(SGLOCALE).c_str())
			("client,c", (_("ignore client")).str(SGLOCALE).c_str())
			("exclude,E", (_("do not shutdown sgtgws")).str(SGLOCALE).c_str())
			("pgname,G", po::value<string>(&grpname_optarg), (_("shutdown a particular process group")).str(SGLOCALE).c_str())
			("pgid,g", po::value<int>(&grpid_optarg)->default_value(-1), (_("shutdown a particular process group id")).str(SGLOCALE).c_str())
			("prcid,p", po::value<int>(&svrid)->default_value(-1), (_("shutdown a particular process id")).str(SGLOCALE).c_str())
			("prcname,n", po::value<string>(&svrname_optarg), (_("shutdown by process name")).str(SGLOCALE).c_str())
			("hostid,h", po::value<string>(&lmid_optarg), (_("shutdown processes on a particular host id")).str(SGLOCALE).c_str())
			("wait,w", po::value<int>(&delay_time)->default_value(0), (_("wait time before kill")).str(SGLOCALE).c_str())
			("kill,k", po::value<string>(&kill_str), (_("kill via SIGTERM or SIGKILL")).str(SGLOCALE).c_str())
			("timeout,t", po::value<int>(&SGP->_SGP_dbbm_resp_timeout)->default_value(6), (_("sgclst response timeout")).str(SGLOCALE).c_str())
			("yes,y", (_("verify full boot")).str(SGLOCALE).c_str())
			("quiet,q", (_("quiet mode")).str(SGLOCALE).c_str())
		;
	}

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		exit(1);
	}

	if (vm.count("yes"))
		prompt = false;
	if (vm.count("admin")) {
		if ((opflags & BF_ADMIN) || (bsflags & BS_PARTITION))
			fatal("option -A conflict");
		opflags |= BF_ADMIN;
		bsflags |= BS_ALLADMIN;
	}
	if (vm.count("process")) {
		if ((opflags & BF_SERVER) || (bsflags & BS_PARTITION))
			fatal("option -P conflict");
		opflags |= BF_SERVER;
		bsflags |= BS_ALLSERVERS;
	}
	if (vm.count("master")) {
		if (bsflags & (BS_ALLADMIN | BS_PARTITION))
			fatal("option -M conflict");

		// need to boot sgclst
		opflags |= BF_DBBM;
		item.ptype = BF_DBBM;
		item.ctype = CT_MCHID;
		item.flags = 0;
		tdlist.push_back(item);

		opflags |= BF_BBM;
		item.ptype = BF_BBM;
		item.ctype = CT_LOC;
		item.ct_str() = bbparms.master[0];
		item.flags = 0;
		tdlist.push_back(item);
	}
	if (vm.count("client"))
		bsflags |= BS_IGCLIENTS;
	if (vm.count("exclude"))
		bsflags |= BS_EXCLUDE;
	if (vm.count("noexec"))
		bsflags |= BS_NOEXEC;
	if (vm.count("quiet"))
		bsflags &= ~BS_VERBOSE;

	if (!bbm_optarg.empty()) {
		if (bsflags & (BS_ALLADMIN | BS_PARTITION))
			fatal("option -B conflict");
		opflags |= BF_BBM;
		item.ptype = BF_BBM;
		item.ctype = CT_LOC;
		item.ct_str() = bbm_optarg;
		item.flags = 0;
		tdlist.push_back(item);
	}

	dbbm_mid = SGC->lmid2mid(bbparms.master[0]);

	if (!grpname_optarg.empty()) {
		if ((bsflags & (BS_ALLSERVERS | BS_PARTITION)) || grpid_optarg != -1)
			fatal("option -G conflict");
		opflags |= BF_SERVER;
		item.ptype = BF_SERVER;
		item.ctype = CT_GRPNAME;
		item.ct_str() = grpname_optarg;
		item.flags = 0;
		tdlist.push_back(item);
	}

	if (grpid_optarg != -1) {
		if (bsflags & (BS_ALLSERVERS | BS_PARTITION))
			fatal("option -g conflict");
		opflags |= BF_SERVER;
		item.ptype = BF_SERVER;
		item.ctype = CT_SVRGRP;
		item.ct_int() = grpid_optarg;
		item.flags = 0;
		tdlist.push_back(item);
	}

	if (svrid != -1) {
		if (bsflags & (BS_ALLSERVERS | BS_PARTITION))
			fatal("option -p conflict");
		opflags |= BF_SERVER;
		byid = true;
		item.ptype = BF_SERVER;
		item.ctype = CT_SVRID;
		item.ct_int() = svrid;
		item.flags = 0;
		tdlist.push_back(item);
	}

	if (!lmid_optarg.empty()) {
		if (bsflags & (BS_ALLSERVERS | BS_PARTITION))
			fatal("option -h conflict");
		opflags |= BF_SERVER;
		item.ptype = BF_SERVER;
		item.ctype = CT_LOC;
		item.ct_str() = lmid_optarg;
		item.flags = 0;
		tdlist.push_back(item);
	}

	if (!svrname_optarg.empty()) {
		if (bsflags & (BS_ALLSERVERS | BS_PARTITION))
			fatal("option -n conflict");
		opflags |= BF_SERVER;
		item.ptype = BF_SERVER;
		item.ctype = CT_SVRNM;
		item.ct_str() = svrname_optarg;
		item.flags = 0;
		tdlist.push_back(item);
	}

	if (kill_str.empty())
		kill_signo = 0;
	else if (kill_str == "SIGTERM" || atoi(kill_str.c_str()) == SIGTERM)
		kill_signo = SIGTERM;
	else if (kill_str == "SIGKILL" || atoi(kill_str.c_str()) == SIGKILL)
		kill_signo = SIGKILL;
	else
		fatal("option -k invalid");

	if (bsflags & BS_PARTITION) {
		int my_mid = SGC->getmid();
		BOOST_FOREACH(const sg_tditem_t& v, tdlist) {
			if (v.ptype != BF_BBM)
				continue;

			int part_mid = SGC->lmid2mid(v.ct_str().c_str());
			if (SGC->midnidx(my_mid) != SGC->midnidx(part_mid))
				fatal((_("ERROR: -P option cannot specify remote HOSTID")).str(SGLOCALE));
		}
	}

	if (!opflags && !(bsflags & BS_PARTITION)) {
		string line;

		/*
		 * -y will turn off the prompt flag. Likewise if the user
		 * is running from a shell, no prompt is issued. And obviously
		 * a prompt for verification is not necessary when we are using
		 * noexec mode.
		 */

		if (prompt && isatty(0) && !(bsflags & BS_NOEXEC)) {
			if (bsflags & BS_BOOTING)
				std::cerr << (_("Boot all admin and server processes? (y/n): ")).str(SGLOCALE) << std::flush;
			else
				std::cerr << (_("Shutdown all admin and server processes? (y/n): ")).str(SGLOCALE) << std::flush;

			while (cin >> line) {
				if (toupper(line[0]) == 'Y' || toupper(line[0]) == 'N')
					break;
				std::cerr << (_("answer y or n: ")).str(SGLOCALE) << std::flush;
			}

			if (toupper(line[0]) != 'Y') {
				std::cerr << "\n";
				if (bsflags & BS_BOOTING)
					std::cerr << (_("Use more options to specify which processes to boot.")).str(SGLOCALE);
				else
					std::cerr << (_("Use more options to specify which processes to shutdown.")).str(SGLOCALE);
				std::cerr << "\n\n";
				std::cerr << desc << std::endl;
				exit(1);
			}
		}

		if ((bsflags & BS_VERBOSE)) {
			char *p = gpenv::instance().getenv("SGPROFILE");
			if (p == NULL || p[0] != '/') {
				std::cerr << (_("ERROR: SGPROFILE environment variable must be an absolute pathname.\n")).str(SGLOCALE);
				exit(1);
			}
			if (bsflags & BS_BOOTING)
				std::cerr << (_("Booting all admin and server processes in ")).str(SGLOCALE) << p << "\n";
			else
				std::cerr << (_("Shutting down all admin and server processes in ")).str(SGLOCALE) << p << "\n";
		}
		opflags = BF_ADMIN | BF_SERVER;
		bsflags |= BS_ALLSERVERS | BS_ALLADMIN;
	}

	if (bsflags & BS_BOOTING)
		SGP->_SGP_amsgboot = true;
}

void bs_base::run()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_rte& rte_mgr = sg_rte::instance(SGT);
	sg_api& api_mgr = sg_api::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	SGC->get_bbparms();
	sg_bbparms_t& bbparms = SGC->_SGC_getbbparms_curbbp;
	bool attached = false;
	bool check_dbbm = false;

	if (bsflags & BS_BOOTING)
		SGC->set_bbtype("PRIVATE");
	else
		SGC->set_bbtype("MSG");

	// Only allow one shutdown process to run at a time.
	if (!(bsflags & BS_BOOTING)) {
		if (api_mgr.sginit(PT_ADM, AT_SHUTDOWN, NULL)) {
			if (bsflags & BS_PARTITION) {
				api_mgr.fini();
				fatal((_("ERROR: -P option not allowed on non-partitioned node")).str(SGLOCALE));
			}
			attached = true;
		} else if (bsflags & BS_PARTITION) {
			opflags |= BF_SERVER|BF_ADMIN;
			if (!SGC->set_bbtype("MP"))
				fatal((_("ERROR: Could not reset model to MP for partitioned shutdown")).str(SGLOCALE));

			sg_init_t init_info;
			uid_t uid = geteuid();
			struct passwd * passwd = getpwuid(uid);
			if (passwd == NULL)
				fatal((_("ERROR: Could not find entry for user")).str(SGLOCALE));
			strncpy(init_info.usrname, passwd->pw_name, MAX_IDENT);
			init_info.usrname[MAX_IDENT] = '\0';
			strcpy(init_info.cltname, "sysclient");

			if (!api_mgr.sginit(PT_ADMCLT, AT_SHUTDOWN, &init_info))
				fatal((_("ERROR: Could not attach to local BB for partitioned shutdown")).str(SGLOCALE));

			if (rpc_mgr.cdbbm(SGC->_SGC_proc.mid, DR_TEMP) < 0)
				fatal((_("ERROR: Could not bring up temp sgclst for partitioned shutdown")).str(SGLOCALE));

			api_mgr.fini();

			SGC->set_bbtype("MSG");
			if (api_mgr.sginit(PT_ADM, AT_SHUTDOWN, NULL)) {
				attached = true;
				/*
				 * First use the dbbm_mid to suspend all
				 * local sggws(s).
				 */
				dbbm_mid = SGC->idx2mid(SGC->midnidx(SGC->_SGC_proc.mid), SGC->midpidx(ALLMID));
				if (rpc_mgr.nwsuspend(&dbbm_mid, 1) == -1)
					fatal((_("ERROR: Failed to suspend local sggws(s) for partitioned shutdown")).str(SGLOCALE));
				dbbm_mid = SGC->_SGC_proc.mid;
				dbbm_act = true;
			} else {
				fatal((_("ERROR: Failed to attach to temp sgclst for partitioned shutdown")).str(SGLOCALE));
			}
		} else {
			/*
			 * _tminit failed, not BS_PARTITION, in MP mode:
			 * check if sgclst is still hanging around and shut
			 * it down, if shutting down the sgclst or all
			 * admin servers.
			 */
			if (opflags & (BF_ADMIN | BF_DBBM)) {
				check_dbbm = true;
				SGC->set_bbtype("PRIVATE");
			}
		}
	}

	if ((bsflags & BS_BOOTING) || check_dbbm) {
		/*
		 * Verify whether or not the caller
		 * is an authorized administrator for
		 * this configuration.
		 */

		SGC->_SGC_slot = bbparms.maxaccsrs;

		if (rte_mgr.suser(bbparms)) {
			/*
			 * Now set the auth type to "SGBOOT", this indicates to
			 * tmbbattch that this is boot and only the BB
			 * should be created and checked.  The BB is created
			 * is dynamically allocated memory.  In this way, tmboot
			 * sets up a private bulletin board containing the node
			 * and node/pe tables needed later.
			 */
			SGC->_SGC_proc.grpid = CLST_PGID;
			SGC->_SGC_proc.svrid = MNGR_PRCID;
			if (SGC->set_atype("SGBOOT") && SGC->attach()) {
				/* make a fake tmproc for rstartp() */
				SGC->_SGC_proc.pid = getpid();
				SGC->_SGC_proc.mid = SGC->getmid();

				attached = true;
				/*
				 * Create request and reply queue so that we
				 * can communicate with the (D)sgmngr.
				 */
				SGC->_SGC_proc.rqaddr = proc_mgr.getq(SGC->_SGC_proc.mid, 0, SGC->_SGC_max_num_msg, SGC->_SGC_max_msg_size, SGC->_SGC_perm);
				if (SGC->_SGC_proc.rqaddr < 0)
					fatal((_("ERROR: Can't create queue")).str(SGLOCALE));
				SGC->_SGC_proc.rpaddr = SGC->_SGC_proc.rqaddr;

				/*
				 * Make sure LAN mode was specified if there
				 * is more than one
				 * node in the _tmnodes table.
				 */
				int node_cnt = 0;
				for (int nidx = 0; !(SGC->_SGC_ntes[nidx].flags & NT_UNUSED) && nidx < SGC->MAXNODES(); nidx++)
					node_cnt++;

				if (node_cnt > 1 && !(SGC->_SGC_bbp->bbparms.options & SGLAN))
					fatal((_("ERROR: LAN mode not specified for multiple node application")).str(SGLOCALE));
			}
		}
	}

	if (!attached) {
		if (bsflags & BS_BOOTING)
			fatal((_("ERROR: cannot create internal BB tables")).str(SGLOCALE));
		else
			fatal((_("ERROR: cannot attach to BB")).str(SGLOCALE));
	}

	if (!(bsflags & BS_ANYNODE)) {
		int master[MAX_MASTERS];
		int local;
		int i;

		for (i = 0; i < MAX_MASTERS; i++)
			master[i] = SGC->lmid2mid(bbparms.master[i]);

		/*
		 * If BS_ANYNODE is not set, then we must at least be on one
		 * of the master lmids. If not, there is no sense calling
		 * _tmfmaster to check where the sgclst is.
		 */
		for (local = 0; local < MAX_MASTERS; local++) {
			if (SGC->midnidx(SGC->_SGC_proc.mid) == SGC->midnidx(master[local]))
				break;
		}

		if (local == MAX_MASTERS)
			fatal((_("must run on master node")).str(SGLOCALE));

		// Check the local mid first for efficiency
		if (SGC->fmaster(master[local], NULL) == NULL){
			for (i = 0; i < MAX_MASTERS; i++) {
				if (i == local)
					continue;
				if (SGC->fmaster(master[i], NULL) == NULL)
					continue;
			}

			if (i == MAX_MASTERS) {
				// Inactive ?, must be booting and on CM
				if (!(bsflags & BS_BOOTING) || local != 0)
					fatal((_("master node not booted")).str(SGLOCALE));

				dbbm_mid = master[0];
				dbbm_act = false;
			} else {
				dbbm_mid = master[i];
				dbbm_act = true;
			}
		} else {
			dbbm_mid = master[local];
			dbbm_act = true;
		}

		if (SGC->midnidx(SGC->_SGC_proc.mid) != SGC->midnidx(dbbm_mid))
			fatal((_("must run on master node")).str(SGLOCALE));
	}

	// check for bad options
	int badoptions = 0;
	BOOST_FOREACH(sg_tditem_t& v, tdlist) {
		switch (v.ctype) {
		case CT_LOC:
		case CT_NODE:
			{
				sg_mchent_t *mchentp;
				if ((mchentp = config_mgr.find_by_lmid(v.ct_str())) == NULL) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Bad HOSTID ({1}) in command line option(s)") % v.ct_str()).str(SGLOCALE));
					badoptions++;
					break;
				}
				// Convert LMIDs to MIDs at this point for efficiency
				if (v.ctype == CT_LOC) {
					v.ctype = CT_MCHID;
					v.ct_mid() = SGC->lmid2mid(mchentp->lmid);
				}
				break;
			}
		case CT_GRPNAME:
			{
				sg_sgte_t sgkey;
				if (v.ct_str().length() >= sizeof(sgkey.parms.sgname)) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Process group name ({1}) too long") % v.ct_str()).str(SGLOCALE));
					badoptions++;
					break;
				}
				strcpy(sgkey.parms.sgname, v.ct_str().c_str());
				if (sg_sgte::instance(SGT).retrieve(S_GRPNAME, &sgkey, &sgkey, NULL) < 0) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid process group name ({1})") % v.ct_str()).str(SGLOCALE));
					badoptions++;
					break;
				}
				v.ctype = CT_SVRGRP;
				v.ct_int() = sgkey.parms.grpid;
				break;
			}
		case CT_MCHID:
			if (v.ptype == BF_DBBM)
				v.ct_mid() = dbbm_mid;
			break;
	    default:	/* Just fall out, we don't validate others */
			break;
		}
	}

	if (badoptions)
		fatal((_("ERROR: Bad command line options! Check the LOG file\n")).str(SGLOCALE));

	/*
	 * If we are shutting down, get the (D)sgmngr's pid, depending on
	 * the model, so that we know whether to unregister, or not.
	 */
	if (!(bsflags & BS_BOOTING)) {
		sg_ste_t ste;
		sg_ste& ste_mgr = sg_ste::instance(SGT);

		ste.svrid() = MNGR_PRCID;
		ste.grpid() = CLST_PGID;
		if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) != 1)
			warning((_("ERROR: can't get sgclst/sgmngr's table entry\n")).str(SGLOCALE));
		else
			master_proc = ste.svrproc();
	}

	// Do signal processing setup
	if (usignal::signal(SIGINT, SIG_IGN) != SIG_IGN)
		usignal::signal(SIGINT, bs_base::onintr);
	if (usignal::signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		usignal::signal(SIGQUIT, bs_base::onintr);
	usignal::signal(SIGHUP, bs_base::onintr);

	int ndone = 0;

	/*
	 * If we are running in the role of tmboot(1),
	 * then start admin processes first, and then
	 * server processes. If we are running in the
	 * role of tmshutdown(1), then stop servers first,
	 * and then any admin processes specified.
	 */
	if (bsflags & BS_BOOTING) {
		int cnt;
		sg_rpc_rply_t *rply = sg_agent::instance(SGT).send(SGC->_SGC_proc.mid, OT_CHECKALIVE, NULL, 0);
		if (rply == NULL || rply->rtn != 1)
			std::cerr << (_("WARN: can't contact local sgagent process\n")).str(SGLOCALE);

		// don't create zombies
		usignal::signal(SIGCHLD, SIG_IGN);

		if (opflags & BF_ADMIN) {
			if ((cnt = do_admins()) > 0)
				ndone += cnt;
		} else {	/* assume all sgmngr's are present */
			bsflags |= BS_HAVEDBBM;
			bitmap.clear();
			bitmap.resize(MAXBBM, true);
			init_node(true);
		}
		if (!gotintr && (opflags & BF_SERVER)) {
			if ((cnt = do_servers()) > 0)
				ndone += cnt;
		}
	} else {
		int cnt;

		if (opflags & BF_SERVER) {
			if ((cnt = do_servers()) > 0)
				ndone += cnt;
		}
		if (!gotintr && (opflags & BF_ADMIN)) {
			if (!gotintr || ignintr()) {
				if ((cnt = do_admins()) > 0)
					ndone += cnt;
			}
		}
	}

	/* all done */
	if ((bsflags & BS_VERBOSE)) {
		if ((bsflags & BS_NOEXEC))
			ndone = 0;

		if (bsflags & BS_BOOTING)
			std::cerr << (ndone == 1 ? (_("{1} process started.\n") % ndone).str(SGLOCALE) : (_("{1} processes started.\n") % ndone).str(SGLOCALE));
		else
			std::cerr << (ndone == 1 ? (_("{1} process stopped.\n") % ndone).str(SGLOCALE) : (_("{1} processes stopped.\n") % ndone).str(SGLOCALE));
		if (((bsflags & BS_PROCINTS) || gotintr) && !(bsflags & BS_NOEXEC))
			std::cerr << (_("INFO: See LOG for complete process status\n")).str(SGLOCALE);
	}

	if (!(bsflags & BS_BOOTING)) {
		struct stat buf;
		if ((master_proc.pid && !proc_mgr.alive(master_proc))
			|| ((!SGC->remote(master_proc.mid)
				&& !proc_mgr.stat(boost::lexical_cast<string>(master_proc.rqaddr), buf))
					|| !(buf.st_mode & 0222))) {
			SGC->qcleanup(true);
			SGC->detach();
		} else if (SGC->_SGC_amadm) {
			api_mgr.sgfini(PT_ADM);
		}
	} else {
		if (SGC->_SGC_proc.rqaddr > 0)
			proc_mgr.removeq(SGC->_SGC_proc.rqaddr);
	}

	exit(exit_code);
}

void bs_base::init_node(bool value)
{
	sgc_ctx *SGC = SGT->SGC();
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("value={1}") % value).str(SGLOCALE), NULL);
#endif

	if (SGC->_SGC_ntes == NULL)
		return;

	for (int i = 0; i < SGC->MAXNODES(); i++) {
		if (value)
			SGC->_SGC_ntes[i].flags |= NP_ACCESS;
		else
			SGC->_SGC_ntes[i].flags &= ~NP_ACCESS;
	}
}

void bs_base::set_node(int mid, bool value)
{
	sgc_ctx *SGC = SGT->SGC();
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("mid={1}, value={2}") % mid % value).str(SGLOCALE), NULL);
#endif

	if (value)
		SGC->_SGC_ntes[SGC->midnidx(mid)].flags |= NP_ACCESS;
	else
		SGC->_SGC_ntes[SGC->midnidx(mid)].flags &= ~NP_ACCESS;
}

void bs_base::sort()
{
	sgc_ctx *SGC = SGT->SGC();
	bool has_dbbm = false;
	sg_tditem_t dbbm_item;
	deque<sg_tditem_t> first_queue;
	deque<sg_tditem_t> last_queue;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	for (vector<sg_tditem_t>::iterator iter = tdlist.begin(); iter != tdlist.end(); ++iter) {
		switch (iter->ptype) {	/* process type */
		case BF_DBBM:
			if (dbbm_act) {	/* sgclst active already */
				iter->ctype = CT_DONE;	/* mark as done */
				bsflags |= BS_HAVEDBBM;
			}
			has_dbbm = true;
			dbbm_item = *iter;
			break;
		case BF_BBM:
			/* At this point ctype should be CT_MCHID */
			if (iter->ctype != CT_MCHID) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Illegal ctype")).str(SGLOCALE));
				exit(1);
			}

			if (!SGC->innet(iter->ct_mid()))
				break;	/* skip non-network BBMs */

			// BBM需要展开，如果与DBBM在同一主机，则展开为BBM和GWS，
			// 否则为BSGWS，BBM和GWS
			if (SGC->midnidx(dbbm_mid) == SGC->midnidx(iter->ct_mid())) {
				sg_tditem_t item = *iter;
				item.ptype = BF_GWS;
				item.flags = 0;
				first_queue.push_front(item);
				item.ptype = BF_BBM;
				item.flags = 0;
				first_queue.push_front(item);
			} else {
				sg_tditem_t item = *iter;
				item.ptype = BF_BSGWS;
				item.flags = CF_HIDDEN;
				first_queue.push_back(item);
				item.ptype = BF_BBM;
				item.flags = 0;
				first_queue.push_back(item);
				item.ptype = BF_GWS;
				item.flags = 0;
				first_queue.push_back(item);
			}
			break;
		default:
			last_queue.push_back(*iter);
			break;
		}
	}

	tdlist.clear();
	if (has_dbbm)
		tdlist.push_back(dbbm_item);

	BOOST_FOREACH(const sg_tditem_t& item, first_queue) {
		tdlist.push_back(item);
	}

	BOOST_FOREACH(const sg_tditem_t& item, last_queue) {
		tdlist.push_back(item);
	}
}


/*
 * Return an indication of whether or not the required ADMIN
 * processes are present on the indicated machine.
 */
bool bs_base::bbmok(int mid)
{
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("mid={1}") % mid).str(SGLOCALE), &retval);
#endif

	if (mid >= 0 && (bsflags & BS_HAVEDBBM)) {
		retval = bitmap.test(SGC->midpidx(mid));
		return retval;
	}

	return retval;
}

/*
 * Return an indication of whether or not the passed
 * entry is in a permissible location (as specified by
 * the user).
 */
bool bs_base::locok(const sg_svrent_t& svrent)
{
	sg_ste_t ste;
	sg_sgte_t sgte;
	bool anyloc = true;
	bool retrieved = false;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (bsflags & BS_ALLSERVERS) {
		retval = true;
		return retval;
	}

	// check primary location only
	if (bsflags & BS_BOOTING) {
		sgte.parms.grpid = svrent.sparms.svrproc.grpid;
		if (sg_sgte::instance(SGT).retrieve(S_GROUP, &sgte, &sgte, NULL) < 0)
			return retval;

		BOOST_FOREACH(const sg_tditem_t& v, tdlist) {
			if (v.ptype != BF_SERVER || v.ctype != CT_MCHID)
				continue;

			anyloc = false;
			if (v.ct_mid() == sgte.parms.midlist[sgte.parms.curmid]) {
				retval = true;
				return retval;
			}
		}
	} else {
		BOOST_FOREACH(const sg_tditem_t& v, tdlist) {
			if (v.ptype != BF_SERVER)
				continue;

			switch (v.ctype) {
			case CT_MCHID:
				anyloc = false;
				if (!retrieved) {
					ste.svrproc() = svrent.sparms.svrproc;
					if (sg_ste::instance(SGT).retrieve(S_GRPID, &ste, &ste, NULL) < 0) {
						/*
						 * Maybe server is not running,
						 * so just ignore the error.
						 */
						break;
					}

					retrieved = true;
				}

				// The real check is here
				if (ste.mid() == v.ct_mid()) {
					retval = true;
					return retval;
				}

				break;
			}
		}
	}

	retval = anyloc;
	return retval;
}

/*
 * Return an indication of whether or not the passed
 * entry is in a permissible server group (as specified
 * by the user).
 */
bool bs_base::grpok(const sg_svrent_t& svrent)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif
	bool anygrp = opflags & BF_SERVER;

	if (bsflags & BS_ALLSERVERS) {
		retval = true;
		return retval;
	}

	BOOST_FOREACH(const sg_tditem_t& v, tdlist) {
		if (v.ptype != BF_SERVER || v.ctype != CT_SVRGRP)
			continue;

		anygrp = false;
		if (v.ct_int() == svrent.sparms.svrproc.grpid) {
			retval = true;
			return retval;
		}
	}

	retval = anygrp;
	return retval;
}

/*
 * Return an indication of whether or not the passed
 * entry has a permissible server id (as specified
 * by the user).
 */
bool bs_base::idok(const sg_svrent_t& svrent)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif
	bool anyid = opflags & BF_SERVER;

	if (bsflags & BS_ALLSERVERS) {
		retval = true;
		return retval;
	}

	BOOST_FOREACH(const sg_tditem_t& v, tdlist) {
		if (v.ptype != BF_SERVER || v.ctype != CT_SVRID)
			continue;

		anyid = false;
		if (v.ct_int() == svrent.sparms.svrproc.svrid) {
			retval = true;
			return retval;
		}
	}

	retval = anyid;
	return retval;
}

/*
 * Return an indication of whether or not the passed
 * entry is a permissible aout (as specified by the user).
 */
bool bs_base::aoutok(const sg_svrent_t& svrent)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif
	bool anyaout = opflags & BF_SERVER;

	if (bsflags & BS_ALLSERVERS) {
		retval = true;
		return retval;
	}

	bf::path path(svrent.sparms.rqparms.filename);
	string aout = path.filename().native();
	BOOST_FOREACH(const sg_tditem_t& v, tdlist) {
		if (v.ptype != BF_SERVER || v.ctype != CT_SVRNM)
			continue;

		anyaout = false;
		if (v.ct_str() == aout) {
			retval = true;
			return retval;
		}
	}

	retval = anyaout;
	return retval;
}

bool bs_base::seqok(const sg_svrent_t& svrent)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif
	bool anyseq = opflags & BF_SERVER;

	if (bsflags & BS_ALLSERVERS) {
		retval = true;
		return retval;
	}

	BOOST_FOREACH(const sg_tditem_t& v, tdlist) {
		if (v.ptype != BF_SERVER || v.ctype != CT_SEQ)
			continue;

		anyseq = false;
		if (v.ct_int() == svrent.sparms.sequence) {
			retval = true;
			return retval;
		}
	}

	retval = anyseq;
	return retval;
}

/*
 * Error processing:
 *
 *	Print a fatal error message and execute the user-specified
 *	command line in place of this process. Set up includes:
 *
 *	+ Spawning a new process;
 *	+ Closing any open files except stdin, stdout, and stderr;
 *	+ Resetting all signals to their default disposition;
 *	+ And finally, providing the command with the current environment.
 */

void bs_base::do_uerror()
{
	sgc_ctx *SGC = SGT->SGC();
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	GPP->write_log(__FILE__, __LINE__, (_("ERROR: Fatal error encountered")).str(SGLOCALE));

	if (SGC->_SGC_proc.rqaddr > 0) {
		sg_proc& proc_mgr = sg_proc::instance(SGT);
		proc_mgr.removeq(SGC->_SGC_proc.rqaddr);
	}

	exit(exit_code);
}

/*
 * Print a warning message and continue.
 */
void bs_base::warning(const string& msg)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	if ((bsflags & BS_VERBOSE))
		std::cerr << (_("WARN: ")).str(SGLOCALE) << msg << std::endl;

	GPP->write_log((_("WARN: {1}") % msg).str(SGLOCALE));
	exit_code = 1;
}

void bs_base::fatal(const string& msg)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	if ((bsflags & BS_VERBOSE))
		std::cerr << (_("FATAL: ")).str(SGLOCALE) << msg << std::endl;

	GPP->write_log((_("FATAL: {1}") % msg).str(SGLOCALE));

	if (!(bsflags & BS_BOOTING)) {
		sg_api::instance(SGT).sgfini(PT_ADM);
	}

	sgc_ctx *SGC = SGT->SGC();
	if (SGC->_SGC_proc.rqaddr > 0) {
		sg_proc& proc_mgr = sg_proc::instance(SGT);
		proc_mgr.removeq(SGC->_SGC_proc.rqaddr);
	}

	exit(1);
}

bool bs_base::ignintr()
{
	string line;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	bsflags |= BS_PROCINTS;
	if (isatty(0) && !(bsflags & BS_NOEXEC)) {
		std::cerr << "\n" << (_("INFO: *Interrupt* Want to Continue? (y/n):")).str(SGLOCALE) << std::flush;
		while (cin >> line) {
			if (toupper(line[0]) == 'Y' || toupper(line[0]) == 'N')
				break;
			std::cerr << (_("answer y or n: ")).str(SGLOCALE) << std::flush;
		}

		if (toupper(line[0]) == 'Y') {
			gotintr = false;
			retval = true;
			return retval;
		}
	}

	handled = true;
	return retval;
}

void bs_base::onintr(int signo)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("signo={1}") % signo).str(SGLOCALE), NULL);
#endif

	gotintr = true;
}

void bs_base::onalrm(int signo)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("signo={1}") % signo).str(SGLOCALE), NULL);
#endif

	hurry = true;
}

}
}

