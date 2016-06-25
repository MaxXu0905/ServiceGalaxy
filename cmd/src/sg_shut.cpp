#include "sg_shut.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

sg_shut::sg_shut()
{
}

sg_shut::~sg_shut()
{
}

int sg_shut::do_admins()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_config& config_mgr = sg_config::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (bsflags & BS_VERBOSE)
		std::cerr << "\n" << (_("Shutting down admin processes ...")).str(SGLOCALE) << "\n\n";

	find_dbbm();
	if (bsflags & BS_ALLADMIN) { // shutting down everything
		// Shutdown every admin process in the ADMIN section.
		dbbm_listed = true;

		for (sg_config::mch_iterator iter = config_mgr.mch_begin(); iter != config_mgr.mch_end(); ++iter)
			get_process("sgmngr", SGC->lmid2mid(iter->lmid));
	} else { // only partial shutdown -B(s) and conditional -D
		BOOST_FOREACH(const sg_tditem_t& v, tdlist) {
			if (v.ctype == CT_MCHID && v.ptype == BF_BBM) {
				if (!get_process("sgmngr", v.ct_mid()))
					continue;
			}
			if (v.ctype == CT_MCHID && v.ptype == BF_DBBM && !dbbm_listed) {
				/*
				 * the tdlist gives us conditional shutdowns
				 * of the sgclst.  We need to see if the sgclst
				 * actually resides on any of the PEs we are
				 * shutting down and then we should put it
				 * in our process list only once.
				 */

				if (v.ct_mid() == dbbm_proc.mid)
					dbbm_listed = true;
			}
		}
	}

	retval = stopadms();
	return retval;
}

int sg_shut::do_servers()
{
	sgc_ctx *SGC = SGT->SGC();
	vector<sg_svrent_t> svrents;
	sg_svrent_t svrent;
	int i;
	int j;
	int svrcnt = 0;
	int mincnt;
	sg_sgte_t sgte;
	sg_ste_t ste;
	sgid_t sgid;
	int ret = -1;
	sg_proc_t pproc;
	int cnt = 0;
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_bbparms_t& bbparms = config_mgr.get_bbparms();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (bsflags & BS_VERBOSE)
		std::cerr << "\n" << (_("Shutting down server processes ...")).str(SGLOCALE) << "\n\n";

	SGT->_SGT_error_code = 0;

	boost::shared_array<sg_grpinfo_t> grpinfo(new sg_grpinfo_t[bbparms.maxsgt]);
	if (grpinfo == NULL) {
		warning((_("ERROR: Cannot shutdown processes, memory allocation failure")).str(SGLOCALE));
		if (!(bsflags & BS_NOERROR))
			do_uerror();

		retval = 0;
		return retval;
	}
	grpinfo[0].grpid = 0;

	/*
	 * Add server entries for the application servers based on information
	 * in the *SERVERS section.
	 */
	for (sg_config::svr_iterator iter = config_mgr.svr_begin(); iter != config_mgr.svr_end() && !gotintr; ++iter) {
		if ((bsflags & BS_EXCLUDE) && strcmp(iter->sparms.rqparms.filename, "sgtgws") == 0)
			continue;

		if (!locok(*iter) || !grpok(*iter) || !aoutok(*iter) || !seqok(*iter))
			continue;

		svrent = *iter;
		j = iter->sparms.svridmax - iter->sparms.svrproc.svrid + 1;

		mincnt = svrcnt;
		for (i = 0; i < j; i++){
			if (!idok(svrent)){
				svrent.sparms.svrproc.svrid++;
				continue;
			}

			ste.grpid() = svrent.sparms.svrproc.grpid;
			ste.svrid() = svrent.sparms.svrproc.svrid;
			if (ste_mgr.retrieve(S_GRPID, &ste, &ste, 0) < 0) {
				svrent.sparms.svrproc.svrid++;
				continue;
			}

			svrcnt++;
			svrents.push_back(svrent);
			svrent.sparms.svrproc.svrid++;
		}

		if (mincnt == svrcnt)
			continue;

		/*
		 * Create gaps in sequence numbers. The gaps must be
		 * large because the TMS numbers must be ordered within
		 * their group by the sequence number created in addtms.
		 * These sequence numbers are negative values -1 to
		 * -tmscnt in the system.
		 */
		for (i = 0; i != SGC->MAXSGT(); i++) {
			if (grpinfo[i].grpid == 0) {
				grpinfo[i].grpid = svrents[mincnt].sparms.svrproc.grpid;
				grpinfo[i].seq = svrents[mincnt].sparms.sequence;
				grpinfo[i + 1].grpid = 0;
				break;
			} else if (grpinfo[i].grpid == svrents[mincnt].sparms.svrproc.grpid) {
				if (grpinfo[i].seq > svrents[mincnt].sparms.sequence)
					grpinfo[i].seq = svrents[mincnt].sparms.sequence;
				break;
			}
		}
	}

	if (gotintr || (SGT->_SGT_error_code != 0 && SGT->_SGT_error_code != SGENOENT)) {
		exit_code = 1;
		retval = 0;
		return retval;
	}

	std::sort(svrents.begin(), svrents.end());

	BOOST_FOREACH(const sg_svrent_t& v, svrents) {
		sg_ulist_t item;
		item.proc = v.sparms.svrproc;
		item.sgid = BADSGID;
		ulist.push_front(item);
	}

	{
		// Suspend all affected servers first.
		scoped_usignal sig;
		setsvstat(SHUTDOWN | SUSPENDED, STATUS_OP_OR);
	}

retry:
	for (list<sg_ulist_t>::iterator iter = ulist.begin(); iter != ulist.end(); ++iter) {
		if (gotintr)
			break;

		/* Get rid of any too-late replies from a prior
		 * stopproc(), before doing tmrsvrs() (which in
		 * MSG mode is _tmmbrsvrs(), which does a vulnerable
		 * message send/recv with sgclst).
		 */
		drain_queue();

		if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, &iter->sgid) < 0) {
			if (!(bsflags & BS_ALLSERVERS)) {
				warning((_("WARN: Can't find Process Group = {1} Id = {2}") % iter->proc.grpid % iter->proc.svrid).str(SGLOCALE));
			}
			continue;
		}

		/* If the cleanupsrv got to it and deleted the server entry
		 * then PRmid is set to BADMID in _tmnullproc(_TCARG).
		 * Also, avoid trying to shutdown a server that is being
		 * cleaned up, because termination of the cleanupsrv
		 * process could result!
		 */
		if (ste.mid() == BADMID || (ste.global.status & CLEANING))
			continue;

		/*
		 *  If delay timing is active (-w option), set a
		 *  non-deferred  alarm to limit the wait
		 *  for a server to shutdown.
		 */
		hurry = false;
		sig_action action;
		if (delay_time && !(bsflags & BS_NOEXEC)) {
			// set up alarm
			action.install(SIGALRM, onalrm, 0, false);
			usignal::setsig(SIGALRM);
			alarm(delay_time);
		}

		if (kill_signo == 0) {
			// "-k" was not specified, so try for a normal shutdown
			ret = stopproc(S_GRPID, ste);
			/* on error stopproc returns -2 if it could not stop the
			 * process, -1 on 'unknown' error or a value of gt zero
			 * indicating the process is terminated/stopped.
			 */
			if(ret > 0)     /* process stopped */
				cnt += ret;
			else if (ret == -2) {
				// Update status of server as 'not shutdown'.
				rpc_mgr.ustat(&sgid, 1, ~(SHUTDOWN | SUSPENDED), STATUS_OP_AND);
			}
			// else ret == -1 , i.e some other errors
		}

		if (delay_time && !(bsflags & BS_NOEXEC))
			alarm(0);

		// If option "-n" is used, signal TERM/KILL should not be sent here.
		if ((kill_signo || (ret <= 0 && delay_time)) && !(bsflags & BS_NOEXEC)) {
			/*
			 *  The alarm went off, or "-k" was
			 *  specified, so now try to kill
			 *  first with SIGTERM and then SIGKILL.
			 */
			if (bsflags & BS_VERBOSE) {
				const char *lmid;

				if (!kill_signo)
					std::cerr << (_("timeout\n")).str(SGLOCALE);

				if ((lmid = SGC->mid2lmid(ste.mid())) == NULL) {
					warning((_("WARN: Bad Host ID, {1} in bulletin board") % ste.mid()).str(SGLOCALE));
					continue;
				}

				std::cerr << (_("\tProcess Id = {1} Group Id = {2} Node = {3}:\t") %ste.svrid() %ste.grpid() % lmid).str(SGLOCALE);
 			}

			pproc.mid = ste.mid();
			pproc.pid = ste.pid();
			if (!kill_signo || (kill_signo == SIGTERM)) {
				if (!proc_mgr.nwkill(pproc, SIGTERM, 10)) {
					GPP->write_log((_("INFO: prcid = {1} pgid = {2} SIGTERM send failed - {3}")
						% ste.svrid() % ste.grpid() % SGT->strerror()).str(SGLOCALE));
				} else {
					GPP->write_log((_("INFO: prcid = {1} pgid = {2} SIGTERM sent")
						% ste.svrid() % ste.grpid()).str(SGLOCALE));
					cnt++;
				}

				if (bsflags & BS_VERBOSE) {
					if (kill_signo)
						std::cerr << (_("SIGTERM\n")).str(SGLOCALE);
					else
						std::cerr << (_("SIGTERM ... ")).str(SGLOCALE);
				}
			}
			/*
			 *  Give server a chance to catch SIGTERM and
			 *  shutdown gracefully.
			 */
			if (delay_time)
				sleep(delay_time);

			if (!kill_signo || kill_signo == SIGKILL) {
				if (!proc_mgr.nwkill(pproc, SIGKILL, 10)) {
					/* If _tmnwkill() failed because process
					 * no longer exists, this is success! --
					 * we don't need to report that the
					 * SIGKILL send failed. */
					if (!(SGT->_SGT_error_code == SGEOS && errno == ESRCH)) {
						GPP->write_log((_("INFO: prcid = {1} pgid = {2} SIGKILL send failed {3}")
							% ste.svrid() % ste.grpid() % SGT->strerror()).str(SGLOCALE));
					}
				} else {
					/* After kill the server, mark the server
					 * to be unrestartable and set flag
					 * CLEANING so that sgmngr can clear the
					 * server's ipc resource.
					 */
					rpc_mgr.ustat(&sgid, 1, ~RESTARTABLE, STATUS_OP_AND);
					rpc_mgr.ustat(&sgid, 1, CLEANING, STATUS_OP_OR);
					GPP->write_log((_("INFO: prcid = {1} pgid = {2} SIGKILL sent")
						% ste.svrid() % ste.grpid()).str(SGLOCALE));
					if (kill_signo)
						cnt++;
				}
				if (bsflags & BS_VERBOSE)
					std::cerr << (_("SIGKILL\n")).str(SGLOCALE);
			}
		}
	}

	/* reset the hurry flag to 0 so that tmshutdown -w will not
	 * cause the machine to be partitioned
	 */
	hurry = false;

	// order of tests important
	if (handled || kill_signo) {
		goto done;
	} else if (gotintr) {
		if (ignintr())
			goto retry;
		else
			goto done;
	}

	/*
	 * reset gotintr, so that ignoring interrupt can
	 * be distinguish from not to continue.
	 */
	gotintr = false;

done:
	if (delay_time && !(bsflags & BS_NOEXEC))
		alarm(0);

	if (gotintr && !(bsflags & BS_NOEXEC)) {
		// cancel system shutdown
		scoped_usignal sig;
		setsvstat(~(SHUTDOWN | SUSPENDED), STATUS_OP_AND);
		std::cerr << "\n" << (_("INFO: System Shutdown Canceled\n")).str(SGLOCALE);
	}

	// make one final effort to discard late replies
	drain_queue();
	retval = cnt;
	return retval;
}

void sg_shut::find_dbbm()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ste_t ste;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	ste.grpid() = CLST_PGID;
	ste.svrid() = MNGR_PRCID;
	drain_queue();
	if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) < 0)
		fatal((_("ERROR: No sgclst exists")).str(SGLOCALE));

	dbbm_proc.proctype = BF_DBBM;
	dbbm_proc.bridge = SGC->innet(dbbm_proc.mid);
	dbbm_proc.mid = ste.mid();
	dbbm_proc.grpid = CLST_PGID;
	dbbm_proc.svrid = MNGR_PRCID;
}

bool sg_shut::get_process(const string& proc_name, int mid)
{
	sg_process_t item;
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("proc_name={1}, mid={2}") % proc_name % mid).str(SGLOCALE), &retval);
#endif

	if (mid == BADMID) {
		exit_code = 1;
		return retval;
	}

	item.hopt = false;
	if (proc_name == "sgmngr")
		item.proctype = BF_BBM;
	else
		item.proctype = BF_DBBM;

	// set machine location
	item.mid = mid;
	item.grpid = SGC->mid2grp(mid);

	if (item.proctype == BF_DBBM)
		item.grpid = CLST_PGID;
	item.svrid = MNGR_PRCID;

	// see if a bridge is on that machine
	item.bridge = SGC->innet(item.mid);

	procs.push_back(item);
	retval = true;
	return retval;
}

int sg_shut::stopadms()
{
	int cnt = 0;
	int bridges;	/* doing BBMs with bridges? */
	bool others_done;
					/* have we done all the BBMs on other nodes
					 * first?  This is important because with
					 * multiple bridges per node, we shouldn't
					 * shutdown any local bridges because we may
					 * lose contact of a remote node depending on
					 * the order of shutdown done on the remote
					 * node.
					 */
	bool susp_done;	/* shutdown the suspended bridges first */
	sgc_ctx *SGC = SGT->SGC();
	sg_process_t ourself;
	bool ourself_set = false;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	getlanstat();

retry:
	bridges = false;
	others_done = false;
	susp_done = false;

	while (!procs.empty()) {
		for (list<sg_process_t>::iterator iter = procs.begin(); iter != procs.end();) {
			if (gotintr)
				break;

			if (!bridges && !iter->bridge) {
				/*
			 	 * first time through, only shutdown BBMs
			 	 * that are on PEs that do not have bridge
				 * processes.
			  	 */
				cnt += killadm(*iter);
				iter = procs.erase(iter);
			} else if (bridges) {
				// process BBMs with bridges
				if (!susp_done) {
					// shutdown the suspended bridges first
					if (gws_suspended(iter->mid)) {
						cnt += killadm(*iter);
						iter = procs.erase(iter);
					} else {
						// skip this one
						++iter;
					}
					continue;
				}

				if (!SGC->remote(iter->mid) && !others_done) {
					/*
					 * Do bridge BBMs on our node last so that
					 * we can communicate with the BBMs on the
					 * other nodes.
					 */
					if (!ourself_set) {
						/*
					 	 * Defer doing ourself until the end,
					 	 * e.g. we need to communicate with
					 	 * the rest of the world still.  Note
					 	 * may be more than 1 bridge per
					 	 * node, only need one.
					 	 */
						ourself_set = true;
					 	ourself = *iter;
					 	iter = procs.erase(iter);
						/*
						 * don't NULL next pointer
						 * in ourself as it must be
						 * traversed.
						 */
					} else {
						// skip second bridge on our node
						++iter;
					}
				} else {
					/*
					 * At this point we are shutting down
					 * a sgmngr on a PE which has a bridge
					 * process and we need to make sure
					 * that we don't partition the
					 * network.
					 */
					if (!partition(*iter))
						cnt += killadm(*iter);

					iter = procs.erase(iter);
				}
			} else { // skip the procss for now
				++iter;
			}
		}

		if (bridges && susp_done) {
			// all remote bridges shutdown
			others_done = true;
		}
		if (bridges) {
			// suspended bridges done
			susp_done = true;
		}
		bridges = true;

		if (!handled && gotintr) {     /* order of tests important */
			if (ignintr())
				goto retry;
			else
				goto end;
		}
	}

	// now do ourselves and the sgclst
	if (ourself_set) {
		if (!partition(ourself))
			cnt += killadm(ourself);

		if (!handled && gotintr) { // order of tests important
			if (ignintr())
				goto retry;
			else
				goto end;
		}
		/*
		 * Do a sgclst shutdown only if we couldn't move it, e.g.
		 * everyone else is being shutdown too.  Therefore by the
		 * time we get here, the sgclst must be on our node and we
		 * are not able to migrate it.
		 */
		if (dbbm_listed && SGC->midnidx(dbbm_proc.mid) == SGC->midnidx(ourself.mid)) {
			cnt += killadm(dbbm_proc);
			if (!handled && gotintr) {     /* order of tests important */
				if (ignintr())
					goto retry;
				else
					goto end;
			}
		}
	} else if (dbbm_listed) {
		/*
		 * No bridges thus ourself is not set and we should kill the
		 * sgclst if we are told to do so.
		 */
		cnt += killadm(dbbm_proc);
		if (!handled && gotintr) { // order of tests important
			if (ignintr())
				goto retry;
			else
				goto end;
		}
	}

end:
	retval = cnt;
	return retval;
}

bool sg_shut::gws_suspended(int mid)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("mid={1}") % mid).str(SGLOCALE), &retval);
#endif

	BOOST_FOREACH(const sg_lan_t& v, lans) {
		if (v.mid != mid)
			continue;

		if (v.status & BRSUSPEND)
			retval = true;

		return retval;
	}

	return retval;
}

void sg_shut::getlanstat()
{
	sg_lan_t entry;
	bool goodlan = false;
	sg_ste_t ste;
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	/*
	 * Create list and if bridge is local, e.g. on same node, then get its status.
	 * Defer doing remote ones, because we don't know if we can communicate with
	 * them yet.
	 */
	for (int i = 0; i < SGC->MAXPES(); i++) {
		if (SGC->_SGC_ptes[i].flags & NP_UNUSED)
			break;

		if (SGC->_SGC_ptes[i].flags & NP_INVALID)
			continue;

		if (SGC->_SGC_ptes[i].flags & NP_NETWORK) {	/* a sggws should be here */
			entry.mid = SGC->_SGC_ptes[i].mid;

			if (SGC->midnidx(entry.mid) == SGC->midnidx(SGC->_SGC_proc.mid)) {
				// a local entry
				ste.grpid() = SGC->mid2grp(entry.mid);
				ste.svrid() = MNGR_PRCID;
				if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) > 0) {
					// retrieve sggws status from its sgmngr
					rpc_mgr.setraddr(&ste.svrproc());
					ste.svrid() = GWS_PRCID;
					if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) > 0) {
						entry.status = ste.global.status;
						lans.push_back(entry);

						if (!(entry.status & BRSUSPEND))
							goodlan = true;
					}
					rpc_mgr.setraddr(NULL);
				}
				/* if we couldn't find it, don't put it in the list */
			} else { 	/* remote, put in list and do later */
				lans.push_back(entry);
			}
		}
	}

	// now process remote list
	BOOST_FOREACH(sg_lan_t& v, lans) {
		if (SGC->midnidx(v.mid) != SGC->midnidx(SGC->_SGC_proc.mid)) {
			if (goodlan) {
				// we can communicate with it, so get the status
				ste.grpid() = SGC->mid2grp(entry.mid);
				ste.svrid() = MNGR_PRCID;
				if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) > 0) {
					// retrieve sggws status from its sgmngr
					rpc_mgr.setraddr(&ste.svrproc());
					ste.svrid() = GWS_PRCID;
					if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) > 0)
						entry.status = ste.global.status;
					else {
						// can't find it set status to BRSUSPEND
						entry.status = BRSUSPEND;
					}
					rpc_mgr.setraddr(NULL);
				} else {
					// can't find it set status to BRSUSPEND
					entry.status = BRSUSPEND;
				}
			} else {
				// can't communicate with it all networks down
				entry.status = BRSUSPEND;
			}
		}
	}
}

bool sg_shut::no_other_lan(int mid)
{
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("mid={1}") % mid).str(SGLOCALE), &retval);
#endif

	BOOST_FOREACH(const sg_lan_t& v, lans) {
		if (SGC->midnidx(v.mid) != SGC->midnidx(mid))
			continue;

		// do not count self
		if (v.mid == mid)
			continue;

		if (!(v.status & BRSUSPEND))
			return retval;
	}

	retval = true;
	return retval;
}

void sg_shut::drain_queue()
{
	static message_pointer msg;
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_switch& switch_mgr = sg_switch::instance();
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (msg == NULL)
		msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(int)));

	// while non-blocking message receives are successful
	while (switch_mgr.getack(SGT, msg, SGNOBLOCK, 0))
		;
}

// Set the status of all servers in our ordered list.
void sg_shut::setsvstat(int status, int flag)
{
	sg_ste_t ste;
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("status={1}, flag={2}") % status % flag).str(SGLOCALE), NULL);
#endif

	int sgidcnt = ulist.size();
	boost::shared_array<sgid_t> auto_sgids(new sgid_t[sgidcnt]);
	sgid_t *sgids = auto_sgids.get();

	/*
	 * Zero out the count and use it to track the actual number
	 * to be updated.
	 */
	sgidcnt = 0;

	for (list<sg_ulist_t>::iterator iter = ulist.begin(); iter != ulist.end();) {
		ste.svrproc() = iter->proc;
		if (ste_mgr.retrieve(S_GRPID, &ste, &ste, &iter->sgid) < 0) {
			iter = ulist.erase(iter);
			continue;
		}

		sgids[sgidcnt++] = iter->sgid;
		++iter;
	}

	// If we were saving SGIDs for group update then do it now.
	if (sgidcnt > 0 && !(bsflags & BS_NOEXEC))
		(void)rpc_mgr.ustat(sgids, sgidcnt, status, flag);
}

// Stop the process specified in the passed STE.
int sg_shut::stopproc(int scope, sg_ste_t& ste)
{
	string temp;
	const char *machnm;
	string status;
	sg_rpc_rply_t *rply;
	int shutdown_count;	/* used to store a count of shutdown attempts */
	sgc_ctx *SGC = SGT->SGC();
	sg_switch& switch_mgr = sg_switch::instance();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_meta& meta_mgr = sg_meta::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("scope={1}") % scope).str(SGLOCALE), &retval);
#endif

	if ((machnm = SGC->mid2lmid(ste.mid())) == NULL) {
		temp = (_("node {1}") % ste.mid()).str(SGLOCALE);
		machnm = temp.c_str();
	}

	bool bbm = (ste.svrid() == MNGR_PRCID);

	if (bsflags & BS_VERBOSE)
		std::cerr << (_("\tProcess Id = {1} Group Id = {2} Node = {3}:\t") %ste.svrid() %ste.grpid() %machnm).str(SGLOCALE);

	if ((bsflags & BS_NOEXEC)) {
		if (bsflags & BS_VERBOSE)
			std::cerr << (_("INFO: Still running\n")).str(SGLOCALE);

		retval = 0;
		return retval;
	}

	// Prepare to send the shutdown message...
	int flags = (scope == S_QUEUE) ? SGSHUTQUEUE : SGSHUTSERVER;
	flags |= SGNOTIME;

	/*
	 * Create a buffer to receive the server's reply
	 * message into. A reply message has two parts: a
	 * success/fail indication, and an optional list
	 * of deleted IPC resources. Only (D)sgmngr's return
	 * the second part.
	 *
	 * NOTE: we can't free this buffer because it is
	 * the static buffer used strictly for ServiceGalaxy RPC.
	 */
	int *l = NULL;
	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(l)));

	sg_msghdr_t *msghdr = *msg;
	if (!bbm)
		msghdr->sendmtype = SENDBASE;

	l = reinterpret_cast<int *>(msg->data());
	*l = 0;
	if (bbm) {
		if (bsflags & BS_KILLBB)
			*l = SD_KEEPBB;
		if (bsflags & BS_IGCLIENTS)
			*l = SD_IGNOREC;
	}

	if (!bbm && (bsflags & BS_RELOC)) {
		if (oktoreloc(ste)) {
			// restartable prints its own warning()s if needed
			if (restartable(ste))
				*l |= SD_RELOC;
		} else {
			warning((_("WARN: No active alternate HOSTID for migration")).str(SGLOCALE));
		}
	}

	if (!proc_mgr.alive(ste.svrproc())) {
		if (bsflags & BS_VERBOSE)
			std::cerr << (_("shutdown succeeded")).str(SGLOCALE) << std::endl;

		retval = 0;
		return retval;
	}

	// Send, but don't await reply... better for handling interrupts.
	if (!meta_mgr.metasnd(msg, &ste.svrproc(), flags)) {
		if (hurry) {
			exit_code = 1;
			return retval;
		}
		if (bsflags & BS_VERBOSE)
			std::cerr << (_("WARN: Can't shutdown process\n")).str(SGLOCALE);

		if (ste.global.status & MIGRATING) {
			warning((_("INFO: Process ({1}/{2}) was shutdown for migration") % ste.grpid() % ste.svrid()).str(SGLOCALE));
		} else {
			warning((_("WARN: Can't shutdown process ({1}/{2})") % ste.grpid() % ste.svrid()).str(SGLOCALE));
		}
		return retval;
	}

	// Now get reply...
retry:
	if (hurry) {
		exit_code = 1;
		return retval;
	}
	/*
	 * record count of shutdowns
	 * this is incremented when there is a msgrcv problem
	 */
	shutdown_count = SGP->_SGP_shutdown;

	if (!switch_mgr.getack(SGT, msg, flags, delay_time)) {
		// alarm went off
		if (hurry) {
			exit_code = 1;
			return retval;
		}
		if (SGT->_SGT_error_code == SGGOTSIG) {
			int ret;
			sg_proc_t pproc;

			/*
			 * NOTE: gotintr is not set yet due to UDEFERSIGS. Must
			 * fake out caller. It is safe to handle signals here.
			 */
			{
				scoped_usignal sig;
			}

			// gotintr is set by now
			ret = ignintr();
			if (ret > 0) // continue...
				goto retry;
			else
				handled = 1; //disable caller's ignintr() call

			if (ret == -2) { // interrupted to kill the process.
			    bool sig_sent = false;

				pproc.mid = ste.mid();
				pproc.pid = ste.pid();
				if (!kill_signo || (kill_signo == SIGTERM)) {
					if (!proc_mgr.nwkill(pproc, SIGTERM, 10)) {
					    sig_sent = false;	/* SIGTERM failed */
						GPP->write_log((_("INFO: PRCID={1} PGID={2} SIGTERM send failed - {3}")
							% ste.svrid() % ste.grpid() % SGT->strerror()).str(SGLOCALE));
					} else {
					    sig_sent = true;	/* SIGTERM sent */
						GPP->write_log((_("INFO: PRCID={1} PGID={2} SIGTERM sent")
							% ste.svrid() % ste.grpid()).str(SGLOCALE));
    				    // time to catch SIGTERM and shutdown
				   	    if (delay_time)
							sleep(delay_time);
					}
			     }

			     // SIGTERM maybe did not work, try SIGKILL
			     if (!kill_signo || kill_signo == SIGKILL) {
				 	if (!proc_mgr.nwkill(pproc, SIGKILL, 10)) {
						/* sending SIGKILL failed but do not set
						 * sig_sent=0, SIGTERM may be already sent
						 */
						/* If _tmnwkill() failed because process
						 * no longer exists, this is success! --
						 * we don't need to report that the
						 * SIGKILL send failed. */
					    if (!(SGT->_SGT_error_code == SGEOS && errno == ESRCH)) {
							GPP->write_log((_("INFO: PRCID={1} PGID={2} SIGKILL send failed - {3}")
								% ste.svrid() % ste.grpid() % SGT->strerror()).str(SGLOCALE));
					    }
					} else {
						/* After kill the server, mark the server
						 * to be unrestartable and set flag
						 * CLEANING so that sgmngr can clear the
						 * server's ipc resource.
						 */
					    rpc_mgr.ustat(&ste.hashlink.sgid, 1, ~RESTARTABLE, STATUS_OP_AND);
					    rpc_mgr.ustat(&ste.hashlink.sgid, 1, CLEANING, STATUS_OP_OR);
					    sig_sent = true;	/* SIGKILL sent */
						GPP->write_log((_("INFO: PRCID={1} PGID={2} SIGKILL sent")
							% ste.svrid() % ste.grpid()).str(SGLOCALE));
					}
				}

				/* if either SIGTERM or SIGKILL is sent, hope that
				 * the 'shutdown' succeeded.
				 */
				if (sig_sent){
					std::cerr << (_("shutdown succeeded")).str(SGLOCALE) << std::endl;
					retval = 1;
					return retval;
				}

				// Force to shutdown failed!
				std::cerr << (_("shutdown failed")).str(SGLOCALE) << std::endl;
			}

			if (bsflags & BS_VERBOSE)
				std::cerr << (_("WARN: Process was shutdown for migration\n")).str(SGLOCALE);

			/*
			 * Note that before calling this function, the status
			 * of the server is marked as 'shutdown'. If server
			 * could not be shutdown, function should return -2
			 * indicating that the status should be marked as
			 * 'not shutdown', otherwise return -1 for error.
			 */
			if (ste.global.status & MIGRATING) {
				warning((_("INFO: Process ({1}/{2}) was shutdown for migration") % ste.grpid() % ste.svrid()).str(SGLOCALE));
				return retval;
			} else {
				warning((_("WARN: Can't shutdown process ({1}/{2})") % ste.grpid() % ste.svrid()).str(SGLOCALE));
				retval = -2;
				return retval;
			}
		}
	}

	msghdr = *msg;

	// if we have message failures, don't loop back
	if (SGP->_SGP_shutdown <= shutdown_count
		&& (msghdr->sender.svrid != ste.svrid()
			|| msghdr->sender.grpid != ste.grpid())) {
		/* Ignore reply from other than the server we intended.
		 * Most likely this is from a previously shutdown server,
		 * whose reply is now too late for normal processing.
		 */
		goto retry;
	}

	rply = msg->rpc_rply();
	if (rply->rtn > 0) {
		status = (_("shutdown succeeded")).str(SGLOCALE);
	} else {
		if (bbm)
			std::cerr << "\n" << (_("WARN: Cannot shutdown sgmngr on {1}. Clients and/or servers may be running\n") %machnm ).str(SGLOCALE);
		exit_code = 1;
		status = (_("WARN: Shutdown failed")).str(SGLOCALE);
	}

	if (bsflags & BS_VERBOSE)
		std::cerr << status << std::endl;

	retval = rply->rtn;
	return retval;
}

bool sg_shut::restartable(const sg_ste_t& ste)
{
	sg_qte_t qkey;
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	qkey.hashlink.rlink = ste.queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &qkey, &qkey, NULL) < 0) {
		warning((_("ERROR: Cannot retrieve process information")).str(SGLOCALE));
		return retval;
	}

	if (!(qkey.parms.options & RESTARTABLE)) {
		warning((_("ERROR: Process ID {1} in group {2} not marked as restartable.  This process will not be restarted upon migration.") % ste.svrid() % ste.grpid()).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

/*
 * Determine if the indicated server can be shutdown for relocation.
 *
 * A native server group may be shutdown for relocation, if it has at
 * least one alternate active location. An active location is one that
 * has an active sgmngr.
 *
 */
bool sg_shut::oktoreloc(const sg_ste_t& ste)
{
	sg_sgte_t sgte;
	sg_ste_t bbm;
	int	i;
	sgc_ctx *SGC = SGT->SGC();
	sg_sgte& sgte_mgr = sg_sgte::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	sgte.parms.grpid = ste.grpid();
	if (sgte_mgr.retrieve(S_GROUP, &sgte, &sgte, NULL) != 1)
		return retval;

	for (i = 0; i < MAXLMIDSINLIST && sgte.parms.midlist[i] != BADMID; i++) {
		if (sgte.parms.midlist[i] == ste.mid())
			continue;

		bbm.grpid() = SGC->mid2grp(sgte.parms.midlist[i]);
		bbm.svrid() = MNGR_PRCID;
		if (ste_mgr.retrieve(S_GRPID, &bbm, NULL, NULL) == 1) {
			retval = true;
			return retval;
		}
	}

	return retval;
}

bool sg_shut::partition(const sg_process_t& process)
{
	sg_ste_t ste;
	int pidx;
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	int retval = false;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	/*
	 * If we're doing a partitioned shutdown -P, then don't worry
	 * about partitioning.
	 */
	if (bsflags & BS_PARTITION)
		return retval;

	/*
	 * If we're not really shutting down, then don't worry
	 * about partitioning.
	 */
	if (bsflags & BS_NOEXEC)
		return retval;

	if (!no_other_lan(process.mid)) {
		// there is another good lan
		return retval;
	}
	/*
	 * If I'm shutting down a BRIDGE local to the master node, then
	 * I need to check for any registered remote BBMs. It doesn't matter
	 * at this point if they can be contacted. It is the administrators
	 * responsibility to clean out the remote entries using tmadmin/pclean.
	 */
	if (SGC->midnidx(process.mid) == SGC->midnidx(dbbm_proc.mid)) {
		for (pidx = 0; pidx < SGC->MAXPES() && !(SGC->_SGC_ptes[pidx].flags & NP_UNUSED); pidx++) {
			if (SGC->_SGC_ptes[pidx].flags & NP_INVALID)
				continue;

			if (SGC->midnidx(SGC->_SGC_ptes[pidx].mid) == SGC->midnidx(process.mid))
				continue;

			ste.grpid() = SGC->mid2grp(SGC->_SGC_ptes[pidx].mid);
			ste.svrid() = MNGR_PRCID;
			if (ste_mgr.retrieve(S_GRPID, &ste, NULL, NULL) > 0)
				goto cantshut;
		}
	} else {
		/*
		 * for a BRIDGE remote from the master node, we just need to check
		 * if any other BBMs exist on that node. If so, we would partition them
		 * and we can't do that.
		 */
		for (pidx = SGC->_SGC_ntes[SGC->midnidx(process.mid)].link; pidx != -1; pidx = SGC->_SGC_ptes[pidx].link) {
			if (SGC->_SGC_ptes[pidx].mid != process.mid) {
				ste.grpid() = SGC->mid2grp(SGC->_SGC_ptes[pidx].mid);
				ste.svrid() = MNGR_PRCID;
				if (ste_mgr.retrieve(S_GRPID, &ste, NULL, NULL) > 0)
					goto cantshut;
			}
		}
	}
	/*
	 * Once we've reached this point without jumping to cantshut, we know
	 * that is it is ok to shutdown this sgmngr.
	 */
	return retval;

cantshut:
	const char *lmid = SGC->mid2lmid(process.mid);
	warning((_("ERROR: Can't shutdown sgmngr on node {1} causes partitioning") % (lmid ? lmid : "?")).str(SGLOCALE));
	retval = true;
	return retval;
}

int sg_shut::killadm(const sg_process_t& process)
{
	sg_ste_t ste;
	int ret = 0;
	const char *machnm;
	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	ste.grpid() = process.grpid;
	ste.svrid() = process.svrid;
	if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) < 0) {
		if (!(bsflags & BS_ALLADMIN) && !process.hopt) {
			if (process.proctype == BF_BBM) {
				string temp;
				if ((machnm = SGC->mid2lmid(process.mid)) == NULL) {
					temp = boost::lexical_cast<string>(SGC->grp2mid(ste.grpid()));
					machnm = temp.c_str();
				}
				warning((_("sgmngr on node {1}") % machnm).str(SGLOCALE));
			} else {
				warning((_("can't find sgclst")).str(SGLOCALE));
			}
		}
	} else if (proc_mgr.alive(ste.svrproc())) {
		scoped_usignal sig;
		ret = stopproc(S_QUEUE, ste);
	}

	if (ret < 0) {
		retval = 0;
		return retval;
	} else {
		retval = ret;
		return retval;
	}
}

}
}

