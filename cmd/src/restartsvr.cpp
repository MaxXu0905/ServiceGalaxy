#include "restartsvr.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

bool sgrecover::gotintr = false;

sgrecover::sgrecover()
{
	grpid = -1;
	svrid = -1;
	notty = false;
	oldpid = 0;
	newpid = 0;
	memset(stats, 0, sizeof(stats));
	called_once = false;
}

sgrecover::~sgrecover()
{
}

int sgrecover::init(int argc, char **argv)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	GPP->set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("pgid,g", po::value<int>(&grpid)->required(), (_("restart process's pgid")).str(SGLOCALE).c_str())
		("prcid,p", po::value<int>(&svrid)->required(), (_("restart process's prcid")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			GPP->write_log((_("INFO: {1} exit normally in help mode.") % GPP->_GPP_procname).str(SGLOCALE));
			retval = 0;
			return retval;
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		SGT->_SGT_error_code = SGEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: {1} start failure, {2}.") % GPP->_GPP_procname % ex.what()).str(SGLOCALE));
		return retval;
	}

	retval = 0;
	return retval;
}

int sgrecover::run(int argc, char **argv)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_api& api_mgr = sg_api::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif
	sig_action action(SIGCHLD, SIG_IGN, SA_NOCLDWAIT);

	setpgid(0, 0); // make immune to signals sent to caller

	int rtn = init(argc, argv);
	if (rtn < 0) {
		retval = rtn;
		return retval;
	}

	if (usignal::signal(SIGINT, SIG_IGN) != SIG_IGN)
		usignal::signal(SIGINT, onintr);
	else
		notty = true;

	if (usignal::signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		usignal::signal(SIGQUIT, onintr);

	// Must make sure that the administrator is executing this command
	if (!SGC->get_bbparms()) {
		GPP->write_log((_("ERROR: Unable to read CLUSTER section of SGPROFILE file")).str(SGLOCALE));
		exit(1);
	}

	sg_bbparms_t& bbparms = SGC->_SGC_getbbparms_curbbp;

	if (bbparms.uid != geteuid()) {
		// Not the Application administrator
		string temp = (_("ERROR: Execute permission denied, not application administrator")).str(SGLOCALE);
		if (isatty(0)) {
			if (notty)
				GPP->write_log(__FILE__, __LINE__, temp);
			else
				std::cerr << temp << std::endl;
		}
		exit(1);
	}

	// Only one sgrecover process at a time.
	if (!api_mgr.sginit(PT_ADM, AT_RESTART, NULL)) {
		string temp;

		if (SGT->_SGT_error_code == SGEOS) {
			temp = (_("ERROR: Cannot be a sgrecover process - {1}, {2}\n")
				% SGT->strnative() % strerror(errno)).str(SGLOCALE);
		} else {
			temp = (_("ERROR: Cannot be a sgrecover process")).str(SGLOCALE);
		}

		// Complain about DUPENT only if we were started by hand.
		if (isatty(0) || SGT->_SGT_error_code != SGENOENT) {
			if (notty)
				GPP->write_log(__FILE__, __LINE__, temp);
			else
				std::cerr << temp << std::endl;
		}
		exit(1);
	}

	/*
	 * Check here to see if the sgmngr died while it had the BB locked.
	 * If it did, then we need to "unlock" it and go on. We're only
	 * taking care of the simplest case here. If a more complicated
	 * case occurs with the sgmngr AND other application processes dying
	 * at the same time, restarsvr may not be able to recover. This
	 * situation is actually a sgmngr bug and should be treated as such.
	 */
	sg_rte_t *bbmrte = SGC->_SGC_rtes + SGC->MAXACCSRS() + AT_BBM; // sgmngr's slot
	if (bbmrte->pid == SGC->_SGC_bbp->bblock_pid) {
		// The sgmngr had/has the BB locked, now see if its dead.
		sg_proc_t dummy = SGC->_SGC_proc;
		dummy.pid = bbmrte->pid;
		if (!proc_mgr.alive(dummy)) {
			// Its dead and had it locked, free up the lock.
			SGC->_SGC_bbp->bblock_pid = SGC->_SGC_proc.pid;
			bbmrte->lock_type = 0;
			new (&SGC->_SGC_bbp->bb_mutex) bi::interprocess_upgradable_mutex();
		}
	}

	// Need to sort restartable servers by sequence number, especially for migration

	// Set up svrlist from sgconfig, sorted by sequence no
	if (get_sequenced_svrs() < 0) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Getting process information from SGPROFILE")).str(SGLOCALE));
		exit(ERROR);
	}

	while (1) {
		{
			// Setup restartable server list, from BB and server passed in command line, bblock first
			scoped_bblock bblock(SGT);

			if ((rtn = get_restartable_svrs(svrid, grpid)) < 0) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Reading process information from BB")).str(SGLOCALE));
				break;
			} else if (rtn >= 0) {
				if (rtn == 0) // empty restartable list
					break;
			}
		}

		// Sort the restartable server list, based on the two lists above
		if (sort_restartable_by_sequence() < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Sequencing processes to be restarted")).str(SGLOCALE));
			break;
		}

		int cursor = 0;
		while (!gotintr && morework(cursor)) {
			/*
			 * Attempt to restart every server that needs
			 * restarting; if restart is not possible (for
			 * whatever reason), schedule a cleanup run
			 * when we're done.
			 */
			if (svrid >= 0) {
				switch(rtn = do_restart()) {
				case FAILURE:
				case ERROR:
					// un-do the affects of our efforts
					cleanup(rtn);
				}

 				stats[rtn]++;
			}
		}
	}

	/*
	 * Let tixe() unlock BB: this avoids a race
	 * between checking for servers that need
	 * restarting and restarsvr's termination.
	 */
	if (gotintr || rtn < 0)
		stats[ERROR]++;
	tixe();
	return 0;
}

/*
 * Attempt to restart the "current" server; the current
 * server is indicated by the grpid and svrid global variables.
 */
int sgrecover::do_restart()
{
	sg_qte_t qte;
	sg_svrent_t svrent;
	int qid;
	sg_mchent_t mchent;
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_config& config_mgr = sg_config::instance(SGT);

	/*
	 * Fetch server table entry to find out the name
	 * of the restart control file. Note that we don't
	 * have to lock the BB around the retrieve and the
	 * update (which will be done shortly) since only
	 * the server that owns the entry updates it and
	 * that server is now dead.
	 */
	newpid = -1;
	oldpid = -1;
	ste.grpid() = grpid;
	ste.svrid() = svrid;
	int retval = SGP->admin_grp(grpid) ? ERROR : FAILURE;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) < 0) {
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart a process")).str(SGLOCALE));
		return retval;
	}

	if ((ste.global.status & SHUTDOWN) && !(ste.global.status & MIGRATING))
		return retval;

	oldpid = ste.pid();

	qte.hashlink.rlink = ste.queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &qte, &qte, NULL) < 0) {
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart a process")).str(SGLOCALE));
		return retval;
	}

	// Must be (dead&RESTARTABLE) or restarting
	bool restartable = (qte.parms.options & RESTARTABLE);

	if (!(ste.global.status & RESTARTING) && !(ste.global.status & MIGRATING)) {
		if (proc_mgr.alive(ste.svrproc()) || !restartable) {
			logmsg(__FILE__, __LINE__, (_("ERROR: Process to be restarted, {1} is still running") % ste.pid()).str(SGLOCALE));
			return retval;
		}
		if (ste.global.gen >= qte.parms.maxgen) {
			time_t now = time(0);
			if (((now - ste.global.ctime) < qte.parms.grace) && qte.parms.grace) {
				logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart a process because the MAXGEN number reached")).str(SGLOCALE));
				return retval;
			}
			ste.global.gen = 0;
			ste.global.ctime = now;
		}
		ste.global.status |= RESTARTING;
		if (ste_mgr.update(&ste, 1, U_LOCAL) < 0) {
			logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart the process")).str(SGLOCALE));
			return retval;
		}
	}

	if (ste.global.status & MIGRATING) {
		// need to create queue for the first migrating server to remote node
		svrent.sparms.svrproc.grpid = ste.grpid();
		svrent.sparms.svrproc.svrid = ste.svrid();

		//  find permission of  the queue
		if (!config_mgr.find(svrent)) {
			logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart the process")).str(SGLOCALE));
			return retval;
		}

		size_t max_num_msg;
		size_t max_msg_size;
		int perm;
		if (svrent.sparms.max_num_msg > 0)
			max_num_msg = svrent.sparms.max_num_msg;
		else
			max_num_msg = SGC->_SGC_max_num_msg;

		if (svrent.sparms.max_msg_size > 0)
			max_msg_size = svrent.sparms.max_msg_size;
		else
			max_msg_size = SGC->_SGC_max_msg_size;

		if (svrent.sparms.rqperm > 0)
			perm = svrent.sparms.rqperm;
		else
			perm = SGC->_SGC_perm;

		ste.rpaddr() = 0;
		if ((qid = proc_mgr.getq(ste.mid(), 0, max_num_msg, max_msg_size, perm)) >= 0) {
			// first server migrating to remote node
			qte.parms.paddr = qid;
			if (qte_mgr.update(&qte, 1, U_GLOBAL) < 0) {
				logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart the process")).str(SGLOCALE));
				return retval;
			}
			/*
			 * If we created the Q, then set both request and
			 * reply queues in the server entry to it.  This is
			 * necessary so that cleanupsrv works correctly in the
			 * case of a failure.  If we didn't create the queue
			 * then we should set the request to qid and the reply
			 * to 0 so we don't inadvertently drain legitimate
			 * requests on cleanup.
			 */
			ste.rpaddr() = qid;
		}
		ste.rqaddr() = qid;
		/*
		 * The following update is precautionary to make cleanup
		 * cases work correctly.  Therefore, don't check error rtn.
		 */
		ste_mgr.update(&ste, 1, U_LOCAL);

		// from now on, like a restarting server
		ste.global.status |= RESTARTING;
	}

	// Set up original invocation environment.
	if (rstrt_setenv() < 0)
		return retval;

	// now kill the old server, leave a core for debugging
	// ...... if it is not MIGRATING case
	if (!(ste.global.status & MIGRATING))  {
		if (proc_mgr.alive(ste.svrproc()) && ste.pid() != getpid()
			&& (ste.global.status & SVRKILLED) == 0) {
			logmsg(__FILE__, __LINE__, (_("INFO: Process process, {1} is still running; forcing termination via SIGIOT")
				% ste.pid()).str(SGLOCALE));
			if (kill(ste.pid(), SIGABRT) != 0) {
				if (errno != ESRCH) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot kill the process, {1} {2}")
						% ste.pid() % strerror(errno)).str(SGLOCALE));
					return retval;
				}
			}
		}
	}

	mchent.mid = ste.mid();
	if (!config_mgr.find(mchent)) {
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart a process - cannot get information for node {1}")
			% ste.mid()).str(SGLOCALE));
		return retval;
	}

	{
		scoped_bblock bblock(SGT);

		/*
		 * brackets tmrques and tmuques in tmbblock/tmbbunlock section
		 * to avoid updating QTE with stale QTE information
		 */
		if (qte_mgr.retrieve(S_BYRLINK, &qte, &qte, NULL) < 0) {
			logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart a process")).str(SGLOCALE));
			return retval;
		}

		try {
			/*
			 * When restarsvr is used for master migration, no previous message
			 * queue exists.  (Also, if the server had failed becuase its queue
			 * was manually removed, no queue would exist.)
			 * Therefore, it is not an error if msgctl() fails.
			 */
			string qname = (boost::format("%1%.%2%") % SGC->midkey(SGC->_SGC_proc.mid) % qte.paddr()).str();
			bi::message_queue msgq(bi::open_only, qname.c_str());

			if (msgq.get_num_msg() < qte.local.nqueued && qte.local.nqueued > 0) {
				qte.local.wkqueued -= qte.local.wkqueued / qte.local.nqueued;
				--qte.local.nqueued;
				if (qte_mgr.update(&qte, 1, U_LOCAL) < 0)
					return retval;
			}
		} catch (exception& ex) {
			GPP->write_log(__FILE__, __LINE__, (_("WARN: The message queue is removed. The process cannot be restarted")).str(SGLOCALE));
		}

		sg_ste_t *step = ste_mgr.link2ptr(ste.hashlink.rlink);
		step->global.status &= ~SVRKILLED;
	}

	int i = proc_mgr.sync_proc(cmdv, mchent, &ste, SP_SYNC, newpid, ignintr);
	switch (i) {
	case BSYNC_FORKERR:
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart the process - fork error")).str(SGLOCALE));
		return retval;
	case BSYNC_EXECERR:
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart a process - exec() failed")).str(SGLOCALE));
		return retval;
	case BSYNC_PIPERR:
		logmsg(__FILE__, __LINE__, (_("ERROR: Pipe error, assume failed")).str(SGLOCALE));
		return retval;
	case BSYNC_TIMEOUT:
		logmsg(__FILE__, __LINE__, (_("ERROR: On restart, process process ID {1} failed to initialize") % newpid).str(SGLOCALE));
		return retval;
	case BSYNC_OK:
		break;
	case BSYNC_NOBBM:
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart the process - no sgmngr")).str(SGLOCALE));
		return retval;
	case BSYNC_NODBBM:
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart the process - no sgclst available")).str(SGLOCALE));
		return retval;
	case BSYNC_APPINIT:
		logmsg(__FILE__, __LINE__, (_("ERROR: Application initialization failure")).str(SGLOCALE));
		return retval;
	default:
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot restart a process - unknown process creation error: {1}")
			% i).str(SGLOCALE));
		return retval;
	}

	ste.pid() = newpid;
	logmsg(__FILE__, __LINE__, (_("INFO: A process has restarted: {1}") % ste.pid()).str(SGLOCALE));

	oldpid = -1;
	retval = SUCCESS;
	return retval;
}

/* Returns next server to be rebooted through
 * the svrid & grpid globals.
 * Servers are returned in the following order
 * sgclst, sgmngr, sggws, all other servers ordered
 * by position in the res_svrs
 */
bool sgrecover::morework(int& cursor)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("cursor={1}") % cursor).str(SGLOCALE), &retval);
#endif

	if (cursor < res_svrs.size()) {
		svrid = res_svrs[cursor].svrid;
		grpid = res_svrs[cursor].grpid;
		res_svrs[cursor].seq_no = RST_SEQNO_PROCESSED;
		cursor++;
		retval = true;
		return retval;
	}

	svrid = -1;
	grpid = -1;
	return retval;
}

int sgrecover::start_bsgws()
{
	sg_svrent_t svrent;
	sg_mchent_t mchent;
	string clopt;
	sgc_ctx *SGC = SGT->SGC();
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_config& config_mgr = sg_config::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	strcpy(svrent.sparms.rqparms.filename, "sghgws");
	svrent.sparms.svrproc.grpid = grpid;
	svrent.sparms.svrproc.svrid = svrid;
	svrent.sparms.svrproc.mid = SGC->_SGC_proc.mid;
	strcpy(svrent.bparms.clopt, "-A");
	if ((clopt = SGT->formatopts(svrent, NULL)).empty())
		return retval;

	if ((cmdv = SGT->setargv(svrent.sparms.rqparms.filename, clopt)) == NULL)
		return retval;

	mchent.mid = SGC->_SGC_proc.mid;
	if (!config_mgr.find(mchent))
		return retval;

	pid_t pid;
	int rtn = proc_mgr.sync_proc(cmdv, mchent, NULL, SP_SYNC, pid, ignintr);
	switch (rtn) {
	case BSYNC_OK:
	case BSYNC_DUPENT:
		retval = 0;
		break;
	case BSYNC_PIPERR:
	case BSYNC_FORKERR:
	case BSYNC_EXECERR:
	case BSYNC_NOBBM:
	case BSYNC_NODBBM:
	case BSYNC_APPINIT:
	default:
		break;
	}

	return retval;
}

int sgrecover::rstrt_setenv()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (SGP->admin_grp(grpid)) // bbm/dbbm process
		retval = setadmenv();
	else // server
		retval = setsvrenv();

	return retval;
}

// Recreate the dbbm server's invocation environment
int sgrecover::setadmenv()
{
	string clopt;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	admin.sparms.svrproc.grpid = grpid;
	admin.sparms.svrproc.svrid = svrid;
	admin.sparms.svrproc.mid = ste.global.svrproc.mid;

	sg_config& config_mgr = sg_config::instance(SGT);
	if (!config_mgr.find(admin)) {
		logmsg(__FILE__, __LINE__, (_("ERROR: Restart error - retrieving process parameters")).str(SGLOCALE));
		return retval;
	}

	if (svrid == GWS_PRCID) {
		if (start_bsgws() == -1) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to start sghgws")).str(SGLOCALE));
			return retval;
		}
	}

	// Set up the argument vector
	if ((clopt = SGT->formatopts(admin, &ste)).empty()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Command line option formation error for process {1}")
			% admin.sparms.rqparms.filename).str(SGLOCALE));
		return retval;
	}

	if ((cmdv = SGT->setargv(admin.sparms.rqparms.filename, clopt)) == NULL) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Too many arguments for process {1}")
			% admin.sparms.rqparms.filename).str(SGLOCALE));
		return retval;
	}

	retval = 0;
	return retval;
}

// Recreate the server's invocation environment
int sgrecover::setsvrenv()
{
	sg_svrent_t svrent;
	string clopt;
	sg_config& config_mgr = sg_config::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	// Have to examine every entry in the servers section
	svrent.sparms.svrproc = ste.global.svrproc;
	if (!config_mgr.find(svrent)) {
		logmsg(__FILE__, __LINE__, (_("ERROR: Restart error - retrieving process parameters")).str(SGLOCALE));
		return retval;
	}

	/*
	 * Set up the argument vector and add GRPID & SVRID
	 * to the invocation environment if needed.
	 */
	if ((clopt = SGT->formatopts(svrent, &ste)).empty()) {
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Command line option formation error for process {1}")
			% svrent.sparms.rqparms.filename).str(SGLOCALE));
		return retval;
	}

	if ((cmdv = SGT->setargv(svrent.sparms.rqparms.filename, clopt)) == NULL) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Too many arguments for process {1}")
			% svrent.sparms.rqparms.filename).str(SGLOCALE));
		return retval;
	}

	retval = 0;
	return retval;
}

/*
 * Restore the "current" STE to its original owner, and,
 * possibly, mark the entry for cleaning up.
 */
void sgrecover::cleanup(int errcode)
{
	sg_rte_t *reg;
	sg_rte_t *res;
	sg_rte_t * endp;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_rte& rte_mgr = sg_rte::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("errcode={1}") % errcode).str(SGLOCALE), NULL);
#endif

	/*
	 * First find the server table entry for this server because we'll
	 * want to update it.  Then find the associated registry table entry
	 * since it also needs updated to reflect the failed restart.
	 */
	if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) < 0) {
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot retrieve information for restarting process")).str(SGLOCALE));
		return;
	}

	// Now find the registry table slot associated with this server.
	res = SGC->_SGC_rtes + SGC->MAXACCSRS();
	endp = SGC->_SGC_rtes + SGC->MAXACCSRS() + MAXADMIN;
	if (bbp->bbmap.rte_use == -1)
		reg = res;
	else
		reg = rte_mgr.link2ptr(bbp->bbmap.rte_use);
	while (reg < endp) {
		if (reg->pid == oldpid || reg->pid == ste.pid() || reg->pid == newpid)
			break;

		if (reg >= res)
			reg++;
		else if (reg->nextreg != -1)
			reg = rte_mgr.link2ptr(reg->nextreg);
		else
			reg = res;
	}
	if (reg == endp)
		reg = NULL;

	switch (errcode) {
	case FAILURE:
		/*
		 * If the server that failed to restart is marked IDLE and the
		 * RESTARTING bit is now off, then it must have gotten far
		 * enough to look like a registered server to the sgmngr;
		 * therefore, to avoid a race condition to take care of this
		 * server, let the sgmngr do with it what it may and continue on.
		 * If the sgmngr happens to mark it restarting while we're still
		 * running, we will give it another try.
		 * No need to log a message here, the failure was logged above
		 * and we don't have any pertinent information to add, just
		 * return.
		 */
		if ((ste.global.status & IDLE) && !(ste.global.status & RESTARTING))
			return;

		/*
		 * Some type of transient failure; mark the
		 * current server as CLEANING, to be handled
		 * by a subsequent cleanupsrv(_TCARG,1) run.
		 *
		 * Again, we don't have to lock the BB around the
		 * retrieve and update, since we are the only process
		 * changing this STE.
		 */
		logmsg(__FILE__, __LINE__, (_("INFO: Cannot restart process, scheduling for cleanup")).str(SGLOCALE));
		if (oldpid > 0) {
			ste.pid() = oldpid;

			/*
			 * The following code to reset the TMRTE pid used to
			 * be within the following if statement, but this
			 * caused problems when the server in question failed
			 * to "restart" during migration and left an
			 * orphaned registry table entry.  This was due to
			 * a violation of the principle that the TMSTE and TMRTE
			 * pids always be kept in sync for servers.
			 */
			if (reg != NULL)
				reg->pid = oldpid;
		}

		/*
		 * If server is alive don't turn off registry bit, let
		 * cleanupsrv handle it.
		 */
		if (!(ste.global.status & MIGRATING) && !proc_mgr.alive(ste.svrproc())) {
			if (reg == NULL) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot find registry table entry for restarting process")).str(SGLOCALE));
			} else {
				reg->rflags &= ~PROC_REST;
				reg->rflags |= PROC_DEAD;
			}
		}
		ste.global.status &= ~(IDLE | RESTARTING | MIGRATING);
		ste.global.status |= CLEANING;
		break;
	case ERROR:
		/*
		 * An administrative process (sggws|sgmngr) has failed to
		 * restart.  We need to clear status and registry table bits
		 * so that a retry will be done on the next clean cycle but
		 * no more tries on this cycle.
		 */
		logmsg(__FILE__, __LINE__, (_("ERROR: failed to restart administrative process, rescheduling restart")).str(SGLOCALE));
		if (oldpid > 0)
			ste.pid() = oldpid;
		ste.global.status &= ~(RESTARTING | MIGRATING | CLEANING);
		ste.global.status |= IDLE;
		if (reg != NULL) {
			reg->rflags &= ~PROC_REST;
			if (oldpid > 0)
				reg->pid = oldpid;
		}
		break;
	}
	if (ste_mgr.update(&ste, 1, U_LOCAL) < 0) {
		GPP->write_log((_("ERROR: Cannot update process information for restarting process {1}/{2}")
			% ste.grpid() % ste.svrid()).str(SGLOCALE));
	}
}

int sgrecover::get_sequenced_svrs()
{
	sg_config& config_mgr = sg_config::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	for (sg_config::svr_iterator iter = config_mgr.svr_begin(); iter != config_mgr.svr_end(); ++iter) {
		int j = iter->sparms.svridmax - iter->sparms.svrproc.svrid + 1;
		for (int i = 0; i < j; i++) {
			svr_info_t item;
			item.grpid = iter->sparms.svrproc.grpid;
			item.svrid = iter->sparms.svrproc.svrid + i;
			item.seq_no = iter->sparms.sequence;
			seq_svrs.push_back(item);
		}
	}

	// Sort by sequence numbers
	std::sort(seq_svrs.begin(), seq_svrs.end());
	retval = 0;
	return retval;
}

/* cmdsid,cmdgid are svrid & grpid passed in
 * command line of restarsvr. They are added
 * to the rsl always
 */
int sgrecover::get_restartable_svrs(int cmdsid, int cmdgid)
{
	int cnt;
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("cmdsid={1}, cmdgid={2}") % cmdsid % cmdgid).str(SGLOCALE), &retval);
#endif

	// 首先情况重启列表
	res_svrs.clear();

	boost::shared_array<sgid_t> auto_sgids(new sgid_t[SGC->MAXSVRS()]);
	if (auto_sgids == NULL)
		return retval;

	sgid_t *sgids = auto_sgids.get();
	sgid_t *tp;
	int i;

	// Get all servers from BB
	ste.mid() = SGC->_SGC_proc.mid;
	if ((cnt = ste_mgr.retrieve(S_MACHINE, &ste, NULL, sgids)) < 0)
		return retval;

	// Walk through server entries in BB and fill up restartable server list
	bool found = false;
	for (i = 0, tp = sgids; i < cnt; i++, tp++) {
		if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, tp) < 0)
			continue;

		if ((ste.global.status & MIGRATING)
			|| ((ste.global.status & RESTARTING) && !proc_mgr.alive(ste.svrproc()))) {
			svr_info_t item;
			item.grpid = ste.grpid();
			item.svrid = ste.svrid();
			item.seq_no = RST_SEQNO_DEFAULT;
			res_svrs.push_back(item);

			if (!found && item.grpid == cmdgid && item.svrid == cmdsid)
				found = true;
		}
	}

	if (!found && !called_once) {
		/* server passed in through command line not marked restartable,
		 * could be sgmngr for example, add to list
		 */
		if (cmdsid != -1 && cmdgid != -1) {
			svr_info_t item;
			item.grpid = cmdgid;
			item.svrid = cmdsid;
			item.seq_no = RST_SEQNO_DEFAULT;
			res_svrs.push_back(item);
		}
		called_once = true;
	}

	retval = res_svrs.size();
	return retval;
}

int sgrecover::sort_restartable_by_sequence()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	/* Walk through restartable server list & fill in sequence
	 * numbers from the sorted server list or based on special
	 * server type (sgclst, sgmngr, sggws)
	 */
	BOOST_FOREACH(svr_info_t& res_item, res_svrs) {
		int gid = res_item.grpid;
		int sid = res_item.svrid;
		int seq_no = RST_SEQNO_DEFAULT;

		if (SGP->admin_grp(gid)) {
			if (sid == GWS_PRCID)
				seq_no = RST_SEQNO_GWS;
			else if (gid == CLST_PGID && sid == MNGR_PRCID)
				seq_no = RST_SEQNO_DBBM;
			else if (gid != CLST_PGID && sid == MNGR_PRCID)
				seq_no = RST_SEQNO_BBM;
		} else {
			BOOST_FOREACH(svr_info_t& seq_item, seq_svrs) {
				if (seq_item.grpid == gid && seq_item.svrid == sid) {
					seq_no = seq_item.seq_no;
					break;
				}
			}
		}

		if (seq_no == RST_SEQNO_DEFAULT) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not identify sequence for pgid={1}, prcid={2}")
				% gid % sid).str(SGLOCALE));
			return retval;
		}

		res_item.seq_no = seq_no;
	}

	// Sort restartable server list by sequence number
	std::sort(res_svrs.begin(), res_svrs.end());
	retval = 0;
	return retval;
}

void sgrecover::tixe()
{
	sg_qte_t qte;
	sg_api& api_mgr = sg_api::instance(SGT);
	sg_viable& viable_mgr = sg_viable::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	// Re-instate default disposition of SIGCHLD.
	usignal::signal(SIGCHLD, SIG_DFL);

	if (stats[FAILURE] != 0) {
		int spawn_flags = SPAWN_CLEANUPSRV;
		/*
		 * Start a cleanupsrv process with
		 * no particular server indicated.
		 * Also, hand-craft a qte which will
		 * make _tmclnupsvr() complain about
		 * cleaning up a restartable server.
		 */
		qte.parms.options = RESTARTABLE;
		if (viable_mgr.clnupsvr(&qte, NULL, spawn_flags) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot start the sgclear process: {1}") % errno).str(SGLOCALE));
			stats[ERROR]++;
		}
	}

	if (stats[ERROR] != 0)
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Automatic process recovery failed, require manual intervention")).str(SGLOCALE));

	// Done with the BB; free it up.
	api_mgr.sgfini(PT_ADM);
	exit(stats[ERROR]);
}

void sgrecover::logmsg(const char *filename, int linenum, const string& msg)
{
	GPP->write_log(filename, linenum, msg + (_(", process {1}/{2}")
		% ste.grpid() % ste.svrid()).str(SGLOCALE));
}

// Called on keyboard interrupt.
void sgrecover::onintr(int signo)
{
	usignal::signal(signo, SIG_IGN);
	gotintr = true;
}

bool sgrecover::ignintr()
{
	string line;

	if (isatty(0)) {
		std::cerr << "\n" << (_("INFO: *Interrupt* Want to Continue? (y/n):")).str(SGLOCALE) << std::flush;
		while (cin >> line) {
			if (toupper(line[0]) == 'Y' || toupper(line[0]) == 'N')
				break;
			std::cerr << (_("answer y or n: ")).str(SGLOCALE) << std::flush;
		}

		if (toupper(line[0]) == 'Y') {
			gotintr = false;
			return true;
		}
	}

	return false;
}

}
}

using namespace ai::sg;

int main(int argc, char **argv)
{
	try {
		sgrecover svr_mgr;

		return svr_mgr.run(argc, argv);
	} catch (exception& ex) {
		std::cerr << ex.what() << std::endl;
		return 1;
	}
}

