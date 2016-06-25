#include "cleanupsvr.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

bool sgclear::gotintr = false;

sgclear::sgclear()
{
	grpid = -1;
	svrid = -1;
	notty = false;
	oldpid = 0;
	memset(stats, 0, sizeof(stats));
}

sgclear::~sgclear()
{
}

int sgclear::init(int argc, char **argv)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	GPP->set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("all,a", (_("clean all that need it")).str(SGLOCALE).c_str())
		("pgid,g", po::value<int>(&grpid), (_("clean process's pgid")).str(SGLOCALE).c_str())
		("prcid,p", po::value<int>(&svrid), (_("clean process's prcid")).str(SGLOCALE).c_str())
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

	if (vm.count("all"))
		aflag = true;
	else
		aflag = false;

	if (!aflag && (grpid == -1 || svrid == -1)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid option given, option 'all' or 'pgid' and 'prcid' must given")).str(SGLOCALE));
		return retval;
	}

	retval = 0;
	return retval;
}

int sgclear::run(int argc, char **argv)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_api& api_mgr = sg_api::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif
	scoped_bblock bblock(SGT, bi::defer_lock);

	setpgid(0, 0);

	int rtn = init(argc, argv);
	if (rtn < 0) {
		retval = rtn;
		return retval;
	}

	if (usignal::signal(SIGINT, SIG_IGN) != SIG_IGN)
		usignal::signal(SIGINT, onintr);
	else
		notty = true;

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

	// Only one sgclear process at a time.
	if (!api_mgr.sginit(PT_ADM, AT_CLEANUP, NULL)) {
		string temp;

		if (SGT->_SGT_error_code == SGEOS) {
			temp = (_("ERROR: Cannot be a sgclear process - native_code = {1}, errno {2}\n")
				% SGT->_SGT_native_code % strerror(errno)).str(SGLOCALE);
		} else {
			temp = (_("ERROR: Cannot be a sgclear process")).str(SGLOCALE);
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

	// save our PROC
	myproc = SGC->_SGC_proc;

	while (1) {
		// Switch back to our own queue.
		SGC->_SGC_proc = myproc;

		/*
		 * If this run is to cleanup *any*
		 * server, call morework() to prime.
		 */
		if (aflag) {
			if (!morework(bblock))
				break;

			// release lock set by morework()--we don't need it yet
			bblock.unlock();
			aflag = false;
		}

		/*
		 * Attempt to cleanup every server that needs
		 * cleaning up; if cleanup is not possible (for
		 * whatever reason), flag the problem and request
		 * manual intervention when we're done.
		 */
		rtn = do_clean();
		if (rtn) {
			// un-do the affects of our efforts
			SGC->_SGC_proc = myproc;
			cleanup();
		}

		stats[rtn]++;

		if (gotintr || !morework(bblock))
			break;

		bblock.unlock();
	}

	/*
         * Let tixe() unlock BB: this avoids a race
         * between checking for servers that need
         * restarting and restartsrv's termination.
         */
	if (gotintr || rtn < 0)
		stats[ERROR]++;
	SGC->_SGC_proc = myproc;
	tixe(bblock);
	return 0;
}

/*
 * Attempt to clean up the "current" server; the current server
 * is indicated by the srvgrp and srvid global variables.
 */
int sgclear::do_clean()
{
	int rmflag;		/* remove server's request queue? */
	sgid_t svrsgid;	/* SGID for the "current" server */
	sg_qte_t qte;
	sg_proc_t oldproc;
	sg_scte_t scte;
	sg_rte_t *res;
	sg_rte_t *endp;
	sg_rte_t *strtp;
	sg_rte_t *p;
	sg_rte_t *reg;
	bool hasrmq = false;

	message_pointer svcmsg;
	sg_msghdr_t *msghdr;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_rte& rte_mgr = sg_rte::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_svcdsp& svcdsp_mgr = sg_svcdsp::instance(SGT);
	int retval = ERROR;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	/*
	 * Fetch server table entry to see if server is really dead.
	 * Note that we don't have to lock the BB around the retrieve
	 * and update (to put our pid in the STE) since only the
	 * server that owns the entry updates it and that server is
	 * now dead.
	 */
	ste.grpid() = grpid;
	ste.svrid() = svrid;
	if (ste_mgr.retrieve(S_GRPID, &ste, &ste, &svrsgid) < 0) {
		logmsg(__FILE__, __LINE__, (_("ERROR: Could not retrieve process")).str(SGLOCALE));
		return retval;
	}
	ste = *ste_mgr.link2ptr(ste.hashlink.rlink);

	// Must be dead or cleaning
	if (!(ste.global.status & CLEANING)) {
		if (proc_mgr.alive(ste.svrproc())) {
			logmsg(__FILE__, __LINE__, (_("ERROR: Process to be cleaned up is still running")).str(SGLOCALE));
			return retval;
		}
		ste.global.status |= CLEANING;
	}

	// take over server table entry
	oldproc = ste.svrproc();
	oldpid = ste.pid();
	ste.pid() = SGC->_SGC_proc.pid;
	if (ste_mgr.update(&ste, 1, U_GLOBAL) < 0) {
		/* In an MP mode if an application server in the slave
		 * node crashed when the network is down, the cleanup server tries
		 * to do a global update to change the status of the server to
		 * CLEANING. Because the network is down, it doesn't get an ack back
		 * and the cleanup server goes away. If the network outage is brief,
		 * the sgclst eventually gets the update and the server state is set to
		 * CLEANING state. Even though the cleanup server is gone, the sgmngr
		 * doesn't clean the server table entry. To force the sgmngr to clean
		 * the entry in this circumstance, we do another global entry setting
		 * the server status to IDLE.
		*/
		if (SGT->_SGT_error_code == SGETIME) {
			ste.global.status &= ~CLEANING;
			ste.global.status |= IDLE;
			ste_mgr.update(&ste, 1, U_GLOBAL);
		}
		logmsg(__FILE__, __LINE__, (_("ERROR: Failed to update process")).str(SGLOCALE));
		ste.pid() = oldpid;
		oldpid = -1;
		return retval;
	}

	// now kill the old server; leave a core for debugging

	/*
         * Reuse of process id's could cause a lot of problems, a deadlock
         * situation being one of them. The problem that is handled here is
         * that if the cleanup server gets the pid of one of the servers that
         * just died, it would send a SIGIOT to itself thus killing itself.
         * So, we have to check that the pid of cleanup server is not the same
         * as the pid of the server that just died. This problem is seen in
         * AIX because it reuses the pid's more frequently than other OS.
         */
	if (myproc.pid != oldpid) {
		if (proc_mgr.alive(oldproc) && (ste.global.status & SVRKILLED) == 0) {
			GPP->write_log(__FILE__, __LINE__, (_("INFO: Process {1} still running - forcing termination (via SIGIOT)") % oldpid).str(SGLOCALE));
			if (kill(oldpid, SIGABRT) != 0) {
				if (errno != ESRCH) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to kill process {1} - {2}")
						% oldpid % strerror(errno)).str(SGLOCALE));
					return retval;
				}
			}

			/*
			 * Sleep to give the killed process a chance to die.
			 * It needs to be scheduled for the cpu in order to die
			 * and we want it to be dead before we continue.
			 */
			sleep(3);
		}
	}

	/*
	 * Process is dead, therefore turn off registry table
	 * entry restart bit, e.g. make DEAD.
	 */
	// find the slot first
	reg = NULL;
	res = SGC->_SGC_rtes + SGC->MAXACCSRS();
	endp = SGC->_SGC_rtes + SGC->MAXACCSRS() + MAXADMIN;
	if (bbp->bbmap.rte_use == -1)
		strtp = res;
	else
		strtp = rte_mgr.link2ptr(bbp->bbmap.rte_use);
	p = strtp;
	while (p < endp) {
		if (p->pid == oldpid) {
			/*
			 * In a multithreaded server, get the main thread
			 * in this loop, and go through the server contexts
			 * in a separate loop.
			 */

			// some issues, how to cleanup entries of all of threads
			if (p->rflags & SERVER_CTXT) {
				p++;
				continue;
			}

			/*
			 * Because of pid reuse, we have to make sure that the
			 * server we are trying to clean up is not the cleanup
			 * server.
			 */
			if (p->slot != SGC->MAXACCSRS() + AT_CLEANUP) {
				reg = p;
				break;
			}
		}
		if (p >= res) {
			p++;
			continue;
		}
		if (p->nextreg != -1)
			p = rte_mgr.link2ptr(p->nextreg);
		else
			p = res;
	}

	if (reg != NULL) {
		// turn off restart bit if it is on, mark dead
		reg->rflags &= ~PROC_REST;
		reg->rflags |= PROC_DEAD;
	}

	// Set up some globals for rcvrq()
	SGC->_SGC_proc.rqaddr = ste.rqaddr();
	SGC->_SGC_proc.rpaddr = ste.rpaddr();
	// put ourselves in server's group
	SGC->_SGC_proc.grpid = ste.grpid();
	SGC->_SGC_proc.svrid = ste.svrid();
	SGP->_SGP_ste = ste;

	qte.hashlink.rlink = SGP->_SGP_ste.queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &qte, &qte, NULL) < 0) {
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot find Queue Table Entry")).str(SGLOCALE));
		return retval;
	}
	SGC->_SGC_sssq = (qte.global.svrcnt == 1); //only server on queue?

	try {
		scoped_bblock bblock(SGT);

		string qname = (boost::format("%1%.%2%") % SGC->midkey(SGC->_SGC_proc.mid) % qte.paddr()).str();
		bi::message_queue msgq(bi::open_only, qname.c_str());

		if (msgq.get_num_msg() < qte.local.nqueued && qte.local.nqueued > 0) {
			qte.local.wkqueued -= qte.local.wkqueued / qte.local.nqueued;
			--qte.local.nqueued;
			if (qte_mgr.update(&qte, 1, U_LOCAL) < 0)
				return retval;
		}
	} catch (exception& ex) {
		hasrmq = true;
	}

	// create some buffer spaces
	svcmsg = sg_message::create(SGT);
	if (svcmsg == NULL)
		return retval;

	msghdr = *svcmsg;

	/*
	 * First take care of the current client(s) of the dead server,
	 * then deal with messages remaining on the queue.
	 * Dequeue all messages on this server's work queue.
	 */

	/*
	 * We have to save currclient's info in local, cause _tmsrvr_done() would be
	 * clean it up in SGP->_SGP_ste.local.currclient
	 */
	// also need to handle the stale message which is still in main thread
	if (SGP->_SGP_ste.local.currclient.pid) {
		/*
		 * status with SVCTIMEDOUT
		 * indicates timeout occured and errordetail should be
		 * appropriately set
		 */
		if (SGP->_SGP_ste.global.status & SVCTIMEDOUT) {
			msghdr->error_detail = SGED_SVCTIMEOUT;
			SGP->_SGP_ste.global.status &= ~SVCTIMEDOUT;
			ste.global.status &= ~(SVCTIMEDOUT | SVRKILLED);
		}

		msghdr->flags = SGP->_SGP_ste.local.curroptions;
		msghdr->rplymtype = SGP->_SGP_ste.local.currrtype;
		msghdr->rplyiter = SGP->_SGP_ste.local.currriter;
		msghdr->rplyto = SGP->_SGP_ste.local.currclient;
		msghdr->alt_flds.msgid = SGP->_SGP_ste.local.currmsgid;
		strncpy(msghdr->svc_name, SGP->_SGP_ste.local.currservice, sizeof(msghdr->svc_name));
		drop_msg(svcmsg);
		svcdsp_mgr.svr_done();
	}

	if (!hasrmq) {
		while (SGC->_SGC_sssq) {
			// only drain request queue if last one reading from queue
			msghdr->rcvr = SGC->_SGC_proc;
			msghdr->rplymtype = -SGP->_SGP_ste.global.maxmtype;
			if (!ipc_mgr.receive(svcmsg, SGSIGRSTRT | SGNOBLOCK))
				break;

			if (svcmsg->is_meta()) {
				/*
				 * Got a meta-message, can't handle it, send a
				 * reply then continue.
				 */
				drop_msg(svcmsg);
				continue;
			}
			// found a request, try to forward it.
			if (!svcmsg->is_rply()) {
				scoped_bblock bblock(SGT);

				if (!scte_mgr.getsvc(svcmsg, scte, 0)) {
					drop_msg(svcmsg);
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot find operation to which to forward request")).str(SGLOCALE));
					continue;
				}

				msghdr->rcvr = scte.global.svrproc;

				if (!qte_mgr.enqueue(&scte)) {
			 		drop_msg(svcmsg);
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot forward message, sending it back")).str(SGLOCALE));
					continue;
				}

				bblock.unlock();
				msghdr = *svcmsg;

				if (!ipc_mgr.send(svcmsg, msghdr->flags)) {
					bblock.lock();
					qte_mgr.dequeue(&scte, true);

			 		drop_msg(svcmsg);
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot forward message, sending it back")).str(SGLOCALE));
					continue;
				}
			} else {
				drop_msg(svcmsg);
			}
		}
	}

	// now drain the reply queue
	if(!hasrmq || (hasrmq && SGC->_SGC_proc.rqaddr != SGC->_SGC_proc.rpaddr)) {
		while(1) {
			msghdr = *svcmsg;
			msghdr->rcvr = SGC->_SGC_proc;
			msghdr->rplymtype = -SGP->_SGP_ste.global.maxmtype;

			if (!ipc_mgr.receive(svcmsg, SGREPLY | SGSIGRSTRT | SGNOBLOCK))
				break;

			drop_msg(svcmsg);
		}
	}

	// Done with this one--remove it.
	if ((rmflag = ste_mgr.remove(svrsgid)) < 0) {
		logmsg(__FILE__, __LINE__, (_("ERROR: Cannot remove process")).str(SGLOCALE));
	} else {
		if (rmflag == 1)
			proc_mgr.removeq(ste.rqaddr());
		if (ste.rqaddr() != ste.rpaddr())
			proc_mgr.removeq(SGC->_SGC_proc.rpaddr);
		logmsg(__FILE__, __LINE__, (_("INFO: process removed")).str(SGLOCALE));
	}

	oldpid = -1;
	retval = 0;
	return retval;
}

// Scan the server table to see if other servers need cleaning
bool sgclear::morework(scoped_bblock& bblock)
{
	int i;
	int j;
	int cnt;
	sgid_t *tp;
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	boost::shared_array<sgid_t> auto_sgids(new sgid_t[SGC->MAXSVRS()]);
	if (auto_sgids == NULL)
		return retval;

	sgid_t *sgids = auto_sgids.get();

	bblock.lock();

	/*
	 * NOTE: we leave the BB locked when we leave this
	 * routine. This is to prevent a race between checking for
	 * servers to cleanup and relinquishing the reserved
	 * registry table slot.
	 */
	ste.mid() = SGC->_SGC_proc.mid;
	if ((cnt = ste_mgr.retrieve(S_MACHINE, &ste, NULL, sgids)) < 0)
		return retval;

	/*
	 * NOTE: The processed array of tmid values is used to ensure that
	 * a single invocation of sgclear will attempt the cleanup of a
	 * particular server only once, preventing infinite recursion.
	 */
	for (i = 0, tp = sgids; i < cnt; i++, tp++) {
		for (j = 0; j < processed_sgids.size(); j++) {
			if (*tp == processed_sgids[j])
				break;
		}

		if (j < processed_sgids.size())
			continue;

		if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, tp) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("WARN: Process cleanup - process vanished")).str(SGLOCALE));
			continue;
		}

		if (ste.global.status & CLEANING) {
			grpid = ste.grpid();
			svrid = ste.svrid();
			processed_sgids.push_back(*tp);
			retval = true;
			return retval;
		}
	}

	return retval;
}

// return the message to sender or log an error to the central log file
void sgclear::drop_msg(message_pointer& msg)
{
	bool alive = true;
	sgc_ctx *SGC = SGT->SGC();
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	sg_msghdr_t& msghdr = *msg;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!msg->rplywanted() || !(alive = proc_mgr.alive(msghdr.rplyto))) {
		/*
		 * If no reply was wanted or the previous client
		 * is now gone, log a message to the central log file.
		 */
		if (alive) {
			logmsg(__FILE__, __LINE__, (_("WARN: Client process {1} - dropped message because process died, OPERATION={2}\n")
				% msghdr.rplyto.pid % msghdr.svc_name).str(SGLOCALE));
		} else {
			logmsg(__FILE__, __LINE__, (_("WARN: Client process {1} - dropped message because process and client died, OPERATION={2}\n")
				% msghdr.rplyto.pid % msghdr.svc_name).str(SGLOCALE));
		}
		return;
	}

	// Tell client no more servers
	msghdr.flags = SGNACK;
	msg->set_length(0);
	msghdr.sender = SGC->_SGC_proc;
	msghdr.error_code = SGESVCERR;
	msghdr.rcvr = msghdr.rplyto;

	sg_metahdr_t& metahdr = *msg;
	metahdr.protocol = SGPROTOCOL;
	if (!ipc_mgr.send(msg, SGREPLY | SGSIGRSTRT | SGNOBLOCK | SGNOTIME)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to reply to {1}")
			% msghdr.rplyto.pid).str(SGLOCALE));
	}
}

// Restore the "current" STE to its original owner.
void sgclear::cleanup()
{
	sg_ste& ste_mgr = sg_ste::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (oldpid > 0) {
		/*
		 * Reset server table entry. Again, there is
		 * no need to keep the BB locked around the
		 * retrieve and update since only the owner
		 * of an STE changes it. In this case, we
		 * *are* the owner.
		 */
		if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) < 0) {
			logmsg(__FILE__, __LINE__, (_("ERROR: Unable to reset process table entry")).str(SGLOCALE));
		} else {
			ste.pid() = oldpid;
			ste_mgr.update(&ste, 1, U_GLOBAL);
			oldpid = -1;
		}
	}
}

void sgclear::tixe(scoped_bblock& bblock)
{
	sg_api& api_mgr = sg_api::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	// Done with the BB; free it up.
	bblock.unlock();

	api_mgr.sgfini(PT_ADM);
	if (stats[ERROR] != 0)
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Automatic Process Recovery failure; \n\t\tProcess processes require manual intervention.")).str(SGLOCALE));
	exit(stats[ERROR]);
}

void sgclear::logmsg(const char* filename, int linenum, const string& msg)
{
	GPP->write_log(filename, linenum, msg + (_(", process {1}/{2}")
		% ste.grpid() % ste.svrid()).str(SGLOCALE));
}

// Called on keyboard interrupt.
void sgclear::onintr(int signo)
{
	usignal::signal(signo, SIG_IGN);
	gotintr = true;
}

}
}

using namespace ai::sg;

int main(int argc, char **argv)
{
	try {
		sgclear svr_mgr;

		return svr_mgr.run(argc, argv);
	} catch (exception& ex) {
		std::cerr << ex.what() << std::endl;
		return 1;
	}
}

