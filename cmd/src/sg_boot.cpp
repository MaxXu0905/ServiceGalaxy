#include "sg_boot.h"

namespace ai
{
namespace sg
{

sg_boot::sg_boot()
{
	dbbm_fail = false;
}

sg_boot::~sg_boot()
{
}

int sg_boot::do_admins()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_bbparms_t& bbparms = config_mgr.get_bbparms();
	bool dbbmgw;
	sg_tditem_t item;
	int cnt = 0;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (bsflags & BS_VERBOSE)
		cout << "\n" << (_("Booting admin processes ...")).str(SGLOCALE) << "\n\n";

	/*
	 * Keep track of whether or not the BBMs failed to boot. If so, then
	 * there is no need to try and boot anything else at that site.
	 */
	bbm_fail.reset(new bool[bbparms.maxpes]);
	for (int i = 0; i < bbparms.maxpes; i++)
		bbm_fail[i] = false;

	bool nwait;
	if (bsflags & BS_NOWAIT) {
		bsflags &= ~BS_NOWAIT;
		nwait = true;
	} else {
		nwait = false;
	}
	BOOST_SCOPE_EXIT((&bsflags)(&nwait)) {
		if (nwait)
			bsflags |= BS_NOWAIT;
	} BOOST_SCOPE_EXIT_END

	if (bsflags & BS_ALLADMIN) {
		bitmap.clear();
		bitmap.resize(MAXBBM); // Mark all PEs as non-booted

		init_node(false); // Mark all nodes as unaccessible
		dbbmgw = false;

		// Fill fields in item that are the same for both sgclst & sgmngr
		item.ctype = CT_MCHID;
		item.name = "";		/* default name is OK */
		item.clopt = "";	/* default clopt is "-A" */
		item.flags = 0;

		// Add sgclst
		item.ptype = BF_DBBM;
		item.ct_mid() = dbbm_mid;
		tdlist.push_back(item);

		// Add sgmngr
		item.ptype = BF_BBM;
		for (int pidx = 0; pidx < SGC->MAXPES() && !(SGC->_SGC_ptes[pidx].flags & NP_UNUSED); pidx++) {
			if (SGC->_SGC_ptes[pidx].flags & NP_INVALID)
				continue;
			item.ct_mid() = SGC->_SGC_ptes[pidx].mid;
			tdlist.push_back(item);
		}
	} else {
		// starting specific sgclst or sgmngr
		// initially, ASSUME any location is usable
		bsflags |= BS_HAVEDBBM;
		bitmap.clear();
		bitmap.resize(MAXBBM, true);
		init_node(true);
		dbbmgw = true;
	}

	/*
	 * Now sort the admin entries in the todo list
	 * Once sorted we can just blaze through the list without regard
	 * to doing things in the proper order.
	 */
	sort();

	/*
	 * The following loop boots all admin processes in the todo list.
	 * Note that there is some special pre & post processing for the
	 * different types of admin processes.
	 */
	for (vector<sg_tditem_t>::iterator iter = tdlist.begin(); iter != tdlist.end() && !gotintr; ++iter) {
		if (iter->ctype == CT_DONE)
			continue;

		if (!(bsflags & BS_HAVEDBBM) && iter->ptype != BF_DBBM) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot continue booting without sgclst")).str(SGLOCALE));
			break;
		}

		sg_svrent_t svrent;
		memset(&svrent, '\0', sizeof(sg_svrent_t));

		switch (iter->ptype) {
		case BF_DBBM:
			strcpy(svrent.sparms.rqparms.filename, "sgclst");
			svrent.sparms.svrproc.grpid = CLST_PGID;
			svrent.sparms.svrproc.svrid = MNGR_PRCID;
			strcpy(svrent.bparms.clopt, "-A");
			break;
		case BF_BBM:
			strcpy(svrent.sparms.rqparms.filename, "sgmngr");
			svrent.sparms.svrproc.grpid = SGC->mid2grp(iter->ct_mid());
			svrent.sparms.svrproc.svrid = MNGR_PRCID;
			strcpy(svrent.bparms.clopt, "-A");
			break;
		case BF_BSGWS:
			strcpy(svrent.sparms.rqparms.filename, "sghgws");
			svrent.sparms.svrproc.grpid = SGC->mid2grp(iter->ct_mid());
			svrent.sparms.svrproc.svrid = GWS_PRCID;
			strcpy(svrent.bparms.clopt, "-A");
			break;
		case BF_GWS:
			strcpy(svrent.sparms.rqparms.filename, "sggws");
			svrent.sparms.svrproc.grpid = SGC->mid2grp(iter->ct_mid());
			svrent.sparms.svrproc.svrid = GWS_PRCID;

			// 调整GWS的clopt选项
			{
				sg_mchent_t mchent;
				mchent.mid = iter->ct_mid();

				if (config_mgr.find(mchent))
					strcpy(svrent.bparms.clopt, mchent.clopt);
				else
					strcpy(svrent.bparms.clopt, "-A");
			}

			if (!bbmok(iter->ct_mid())) {
				if (!(iter->flags & CF_HIDDEN))
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: No sgmngr available for boot of sggws on {1}") % iter->ct_mid()).str(SGLOCALE));
				continue;
			}
			break;
		default:	/* Skip over server procs */
			break;
		}

		svrent.sparms.svrproc.mid = iter->ct_mid();

		// Make sure the node is accessible for the booting process
		if (SGC->midnidx(iter->ct_mid()) != SGC->midnidx(dbbm_mid)) {
			// sgclst node sggws must be available for remote boot
			if (!dbbmgw) {
				if (!(iter->flags & CF_HIDDEN)) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot boot remote sgmngr({1}), no sggws available on master node(s)({2})")
						% SGC->mid2lmid(iter->ct_mid()) % SGC->mid2lmid(dbbm_mid)).str(SGLOCALE));
				}
				continue;
			}
			// Non-sggws remote boots must have access to the node
			if (!SGC->innet(iter->ct_mid()) && !SGC->tstnode(iter->ct_mid())) {
				if (!(iter->flags & CF_HIDDEN)) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot boot remote sgmngr({1}), node inaccessible")
						% SGC->mid2lmid(iter->ct_mid())).str(SGLOCALE));
				}
				continue;
			}
		}

		// If this is a hidden process, turn off verbose temporary
		bool is_verbose = (bsflags & BS_VERBOSE);
		BOOST_SCOPE_EXIT((&bsflags)(&is_verbose)) {
			if (is_verbose) {
				bsflags |= BS_VERBOSE;
				bsflags &= ~BS_HIDERR;
			}
		} BOOST_SCOPE_EXIT_END

		if (iter->flags & CF_HIDDEN) {
			bsflags &= ~BS_VERBOSE;
			bsflags |= BS_HIDERR;
		}

		// Done with preprocessing, now start the process
		int ret = startadmin(svrent, iter->ptype);

		// If we succeeded (or were a duplicate, same as success)
		if (ret || SGT->_SGT_error_code == SGEDUPENT) {
			SGT->_SGT_error_code = 0;

			if (ret && !(bsflags & BS_HIDERR))
				cnt++;

			/*
			 * When we boot a sggws, mark that node as accessible.
			 * In addition, mark when we boot a gws on the sgclst
			 * node so we know if we can go remote.
			 */
			if (iter->ptype == BF_GWS) {
				if (SGC->innet(iter->ct_mid())) {
					set_node(iter->ct_mid(), true);
					if (SGC->midnidx(iter->ct_mid()) == SGC->midnidx(dbbm_mid))
						dbbmgw = true;
				}
			}
		} else {
			if (iter->ptype == BF_BBM && !bbm_fail[SGC->midpidx(iter->ct_mid())]) {
				bbm_fail[SGC->midpidx(iter->ct_mid())] = true;
				GPP->write_log((_("WARN: No sgmngr available on site {1}.\n\tWill not attempt to boot process processes on that site.")
					% SGC->mid2lmid(iter->ct_mid())).str(SGLOCALE));
			}
			if (iter->ptype == BF_GWS && !bbm_fail[SGC->midpidx(iter->ct_mid())]) {
				bbm_fail[SGC->midpidx(iter->ct_mid())] = true;
				GPP->write_log((_("WARN: No sggws available on site {1}.\n\tWill not attempt to boot process processes on that site.")
					% SGC->mid2lmid(iter->ct_mid())).str(SGLOCALE));
			}
			if (iter->ptype == BF_DBBM && !dbbm_fail) {
				dbbm_fail = true;
				GPP->write_log((_("WARN: No sgclst available on site {1}.\n\tWill not attempt to boot process processes.")
					% SGC->mid2lmid(iter->ct_mid())).str(SGLOCALE));
			}
		}

		iter->ctype = CT_DONE;	// Done with that entry now

		if (handled || (gotintr && ignintr()))
			break;
	}

	retval = cnt;
	return retval;
}

int sg_boot::do_servers()
{
	sgc_ctx *SGC = SGT->SGC();
	vector<sg_svrent_t> svrents;
	sg_svrent_t svrent;
	int i;
	int j;
	int svrcnt = 0;
	int mincnt;
	sg_sgte_t sgte;
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_bbparms_t& bbparms = config_mgr.get_bbparms();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (bsflags & BS_VERBOSE)
		cout << "\n" << (_("Booting process processes ...")).str(SGLOCALE) << "\n\n";

	if (bbm_fail == NULL) {
		/*
		 * Keep track of whether or not the BBMs failed to boot. If so, then
		 * there is no need to try and boot anything else at that site.
		 */
		bbm_fail.reset(new bool[bbparms.maxpes]);
		for (int i = 0; i < bbparms.maxpes; i++)
			bbm_fail[i] = false;
	}

	boost::shared_array<sg_grpinfo_t> grpinfo(new sg_grpinfo_t[bbparms.maxsgt]);
	if (grpinfo == NULL) {
		warning((_("ERROR: Cannot start processes, memory allocation failure")).str(SGLOCALE));
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
		if (!locok(*iter) || !grpok(*iter) || !aoutok(*iter) || !seqok(*iter))
			continue;

		if (byid) {
			j = iter->sparms.svridmax - iter->sparms.svrproc.svrid + 1;
		} else if (bootmin >= 0) { /* -m option is specified */
			int bootmax = iter->sparms.svridmax - iter->sparms.svrproc.svrid + 1;
			j = (bootmin < bootmax) ? bootmin : bootmax;
		} else {
			j = iter->bparms.bootmin;
		}

		mincnt = svrcnt;
		for (i = 0; i < j; i++) {
			svrent = *iter;
			svrent.sparms.svrproc.svrid += i;

			if (!idok(svrent))
				continue;

			svrcnt++;
			svrents.push_back(svrent);
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
		retval = 0;
		return retval;
	}

	std::sort(svrents.begin(), svrents.end());

	/*
	 * Have to examine every entry in the servers section regardless of
	 * whether we are starting all servers or just those specified by the
	 * user. Initialize some loop variables that we don't want reset if
	 * the loop is reentered after an interrupt.
	 */
	int who = BF_DBBM;
	int cnt = 0;
	i = 0;
	sg_svrent_t *svrentp = &svrents[i];

	for (; !gotintr && i < svrcnt; i++, svrentp++) {
		scoped_usignal sig;

		sgte.parms.grpid = svrentp->sparms.svrproc.grpid;
		if (sg_sgte::instance(SGT).retrieve(S_GROUP, &sgte, &sgte, NULL) < 0)
			continue;

		int curmid = SGC->midpidx(sgte.parms.midlist[sgte.parms.curmid]);
		bool bbm_is_ok = false;
		bool bbm_fail_ok = false;
		for (int k = 0; k < MAXLMIDSINLIST; k++) {
			int bpe = sgte.parms.midlist[k];
			int bmid = SGC->midpidx(bpe);

			if (bpe != BADMID && bbmok(bmid))
				bbm_is_ok = true;
			if (bpe != BADMID && !bbm_fail[bmid])
				bbm_fail_ok = true;
		}
		if (!bbm_is_ok || dbbm_fail || !bbm_fail_ok) {
			/*
			 * If already tried to boot sgmngr but failed or already
			 * printed message about the current mid, don't bother
			 * to print or log message, simply skip to next entry.
			 */
			if (!dbbm_fail && !bbm_fail[curmid]) {
				bbm_fail[curmid] = true;
				GPP->write_log(__FILE__, __LINE__, (_("INFO: No sgmngr available on site {1}.\n\tWill not attempt to boot processes on that site.")
					% SGC->mid2lmid(curmid)).str(SGLOCALE));
			}

			continue;
		}

		int ret;
		if ((ret = startproc(*svrentp, BF_SERVER)) > 0) {
			cnt += ((ret == BSYNC_DUPENT) ? 0 : ret);
		} else {
			int error = 0;
			switch (ret) {
			case BSYNC_DUPENT:
				// ignore TMEDUPENT errors
				break;
			case BSYNC_NOBBM:
				who = BF_BBM;
				// FALLTHRU
			case BSYNC_NODBBM:
				// No (D)sgmngr on requesting machine
				if (!set_bbm(who, svrentp->sparms.svrproc.mid, false)) {
					// "can't happen"
					GPP->write_log((_("WARN: aout = {1} loc = {2}")
						% svrentp->sparms.rqparms.filename % svrentp->sparms.svrproc.mid).str());
				}
				who = BF_DBBM;
				error++;
				// FALLTHRU
			case 0:
				/*
				 * If BS_NOERROR is set, ignore
				 * this "error." If BS_NOEXEC is
				 * set, startproc() is supposed
				 * to return 0, hence this is not
				 * really an error. Therefore, we must
				 * *check* for an error condition
				 * in the BS_NOEXEC case.
				 */
				if ((bsflags & BS_NOEXEC) && !error)
					break;
				// FALLTHRU
			default:
				break;
			}
		}

		if (handled || (gotintr && ignintr()))
			break;
	}

	retval = cnt;
	return retval;
}

// Common routine to start an admin process.
int sg_boot::startadmin(sg_svrent_t& svrent, int who)
{
	bool onoff = true;
	int started = 0;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("who={1}") % who).str(SGLOCALE), &retval);
#endif

	/*
	 * Make sure to call setbbl(_TCARG) when BS_NOEXEC flag is on.
	 */
	scoped_usignal sig;

	int ret = startproc(svrent, who);
	switch (ret) {
	case 0:
		if (!(bsflags & BS_NOEXEC)) {
			if (!(bsflags & BS_NOERROR)) {
				do_uerror();
			} else {
				/*
				 * Invalidate initial
				 * assumption above...
				 */
				onoff = false;
			}
		}
		// FALLTHRU
	case 1: 	/* started successfully */
		if (who == BF_DBBM) {
			/*
			 * Successful boot of sgclst means that nothing else
			 * is available currently regardless of what options
			 * were used on the boot command line.
			 */
			bitmap.clear();
			bitmap.resize(MAXBBM, false);
		}
		started = ret;
		// FALLTHRU
	case BSYNC_DUPENT:	/* ignore SGEDUPENT errors */
		set_bbm(who, svrent.sparms.svrproc.mid, onoff);	/* ok if fails */
		break;
	case BSYNC_NODBBM:
		bsflags &= ~BS_HAVEDBBM;
		// FALLTHRU
	default:
		/*
		 * Disable this process in the
		 * appropriate bitmap.
		 */
		if ((bsflags & BS_NOERROR))
			set_bbm(who, svrent.sparms.svrproc.mid, false);
		else
			do_uerror();
	}

	retval = started;
	return retval;
}

/*
 * Start a process with the specified command line options; on
 * the specified machine; and with the specified environment.
 * If queue placement is to be done, create the request queue
 * for the process.
 */
int sg_boot::startproc(sg_svrent_t& svrent, int ptype)
{
	sgc_ctx *SGC = SGT->SGC();
	char **argv;
	string clopt;
	sg_proc_t& proc = svrent.sparms.svrproc;
	sg_mchent_t mchent;
	int sp_ret;
	int i;
	int j;
	pid_t pid = -1;	/* assume it didn't start */
	vector<int> ml;	/* List of MIDs to consider */
	bool nochoice = false;	/* set if server has no choice in proc location */
	const char *aout = svrent.sparms.rqparms.filename;
	sg_config& config_mgr = sg_config::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("ptype={1}") % ptype).str(SGLOCALE), &retval);
#endif

	/*
	 * For MSSQ servers, see if an appropriate location has already been
	 * decided for the set and use it.
	 */
	if (ptype == BF_SERVER) {
		if ((proc.mid = getgroup(proc.grpid)) != BADMID) {
			nochoice = true;
			ml.push_back(proc.mid);
		}
	}

	/*
	 * There is still a choice involved, so call doprocloc which will filter
	 * out bad MIDs and give us a list of possibilities. Note that the
	 * define DFLTMID is used to identify the case of SHM mode on a multi-
	 * processor in which case default placement should be used.
	 */
	if (!nochoice) {
		/*
		 * If doprocloc() fails, then we have no valid mids left, the only
		 * thing left to do is give up.
		 */
		if (!doprocloc(proc, ml)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Skipping aout={1}, LOC={2} - illegal HOSTID specification.\n")
				% aout % SGC->mid2lmid(ml[0])).str(SGLOCALE));

			retval = 0;
			return retval;
		}
	}

	for (i = 0; i < ml.size(); i++) {
		proc.mid = ml[i];	/* Try next mid */
		/*
		 * Format the command line arguments for this server. First free
		 * space allocated on the first try if any and then add the
		 * necessary internal options. Then add the application specified
		 * options.
		 */
		if ((clopt = SGT->formatopts(svrent, NULL)).empty()) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Argument vector formation error")).str(SGLOCALE));
			retval = 0;
			return retval;
		}

		if ((argv = SGT->setargv(aout, clopt)) == NULL) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Aout={1} - too many arguments") % aout).str(SGLOCALE));
			retval = 0;
			return retval;
		}

		/*
		 * Print out the exec AOUT options line. We do this within the loop
		 * since we need to recalculate argv each time through the loop.
		 * However, only print it out once, because the only thing that
		 * changes is the MID and that is reprinted for each try anyway.
		 */
		if (i == 0 && (bsflags & BS_VERBOSE)) {
			string info_msg = (_("exec ")).str(SGLOCALE);
			for (j = 0; argv[j]; j++) {
				info_msg += argv[j];
				info_msg += " ";
			}
			info_msg += ":\n\t";
			std::cerr << info_msg;
		}

		// Print out the HOSTID being used for this attempt.
		if (bsflags & BS_VERBOSE)
			std::cerr << (i > 0 ? "\t" : "") << (_("on ")).str(SGLOCALE) << SGC->mid2lmid(ml[i]) << " -> ";

		/*
		 * If we're in NOEXEC mode, then we're done, so set the return
		 * code and break out of the loop.
		 */

		if (bsflags & BS_NOEXEC) {
			sp_ret = 1;
			pid = 0;
			if (bsflags & BS_VERBOSE)
				std::cerr << "\n";
			break;
		}

		/*
		 * Now we need to fork/exec/[sync] with the server being booted.
		 * If the process is to be booted remotely, we need to format
		 * a OT_SYNCPROC RPC and send it to the tagent on the machine in
		 * question. Otherwise, we need to call the routine _tmsync_proc(_TCARG)
		 * to do the job locally. Note that when errors occur, we have
		 * to be cognizant of whether we've printed the "exec ..." stuff
		 * yet. If not, we need to give more information on an error to
		 * be of any use.
		 */
		mchent.mid = proc.mid;
		if (!config_mgr.find(mchent)) {
			if (bsflags & BS_VERBOSE)
				std::cerr << (_("ERROR: Illegal host id value\n")).str(SGLOCALE);
			else
				std::cerr << (_("ERROR: Illegal host id value, {1}, for {2}") % proc.mid %aout).str(SGLOCALE) << "\n";
			sp_ret = 0;
			continue;	/* Try next location */
		}

		int syncflags = (bsflags & BS_NOWAIT) ? 0 : SP_SYNC;
		if (!config_mgr.propagate(mchent)) {
			int curmid = SGC->midpidx(proc.mid);
			std::cerr << (_("ERROR: Cannot propagate SGPROFILE file\n")).str(SGLOCALE);
			if (!bbm_fail[curmid] && (ptype == BF_BBM || ptype == BF_SERVER)) {
				bbm_fail[curmid] = true;
				warning((_("No sgmngr available on site {1}.\n\tWill not attempt to boot processes on that site.") % SGC->mid2lmid(curmid)).str(SGLOCALE));
			}
			sp_ret = 0;
			continue;
		}

		sg_proc& proc_mgr = sg_proc::instance(SGT);
		if (SGC->remote(proc.mid))
			sp_ret = proc_mgr.rsync_proc(argv, mchent, syncflags, pid);
		else
			sp_ret = proc_mgr.sync_proc(argv, mchent, NULL, syncflags, pid, sg_boot::ignintr);

		// Now interpret the return value from sync_proc().
		switch (sp_ret) {
		case BSYNC_FORKERR:
			std::cerr << (_("ERROR: Cannot fork\n")).str(SGLOCALE);
			pid = -1;
			break;	/* try next location */
		case BSYNC_EXECERR:
			std::cerr << (_("ERROR: Cannot exec, executable file not found\n")).str(SGLOCALE);
			pid = -1;
			break;	/* try next location */
		case BSYNC_NSNDERR:	/* Network send error */
			std::cerr << (_("ERROR: Cannot exec, network send error\n")).str(SGLOCALE);
			pid = -1;
			break;	/* try next location */
		case BSYNC_APPINIT: /* Application initialization failure */
			std::cerr << (_("ERROR: Application initialization failure\n")).str(SGLOCALE);
			pid = -1;
			break;	/* try next location */
		case BSYNC_NRCVERR:	/* Network receive error */
			if (bsflags & BS_VERBOSE)
				std::cerr << (_("WARN: Process id={1} Assume started (network).\n") % pid).str(SGLOCALE);
			sp_ret = 1;	/* One process started */
			break;
		case BSYNC_PIPERR:
			if (bsflags & BS_VERBOSE)
				std::cerr << (_("INFO: Process id={1} Assume started (pipe).\n")% pid).str(SGLOCALE);
			sp_ret = 1;	/* One process started */
			break;
		case BSYNC_TIMEOUT:
			if (SGC->remote(proc.mid)) {
				int curmid = SGC->midpidx(proc.mid);
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Process process ID {1} on site {2} failed to initialize within BOOTTIMEOUT seconds") % pid % SGC->mid2lmid(curmid)).str(SGLOCALE));
			} else {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Process process ID {1} failed to initialize within {2} seconds")
					% pid % boottimeout).str());
			}
			if (bsflags & BS_VERBOSE)
				std::cerr << (_("INFO: Process ID={1} Assume started (timeout).\n") % pid).str(SGLOCALE);
			sp_ret = 1;	/* One process started */
			break;
		case BSYNC_NEEDPCLEAN: 	/* sgmngr boot failed - pclean may help */
			std::cerr << (_("ERROR: Remote sgmngr boot failed. Try running pclean before booting\n")).str(SGLOCALE);
			pid = -1;
			break;
		case BSYNC_OK:	/* Did start OK */
			if (bsflags & BS_VERBOSE)
				std::cerr << (_("process id={1} ... Started.\n")% pid).str(SGLOCALE);
			sp_ret = 1;	/* One process started */
			break;
		case BSYNC_DUPENT:
			if (bsflags & BS_VERBOSE)
				std::cerr << (_("INFO: Duplicate process.\n")).str(SGLOCALE);
			pid = 1;	/* Fake success */
			SGT->_SGT_error_code = SGEDUPENT;
			break;
		case BSYNC_NOBBM:
			{
				std::cerr << (_("ERROR: No sgmngr available, cannot boot\n")).str(SGLOCALE);
				int curmid = SGC->midpidx(proc.mid);
				if (!bbm_fail[curmid]) {
					bbm_fail[curmid] = true;
					warning((_("No sgmngr available on site {1}.\n\tWill not attempt to boot processes on that site.") % SGC->mid2lmid(curmid)).str(SGLOCALE));
				}
				pid = -1;
				break;
			}
		case BSYNC_NODBBM:
			std::cerr << (_("ERROR: No sgclst available, cannot boot\n")).str(SGLOCALE);
			dbbm_fail = true;
			warning("No sgclst available.\n\tWill not attempt to boot server processes.");
			pid = -1;
			break;
		default:
			/*
			 * sp_ret < 0 indicates special internal errors. Others
			 * are returned as >= 0 and have error message entries
			 * in an array.
			 */
			pid = -1;
			if (bsflags & BS_VERBOSE)
				std::cerr << (_("Failed.\n")).str(SGLOCALE);
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Process {1} ON {2} failed with error_code ({3})") % aout % mchent.lmid % sp_ret).str(SGLOCALE));
			sp_ret = 0;	/* Nothing started */
			break;
		}
		/*
		 * If the process booted successfully, then no need to continue.
		 */
		if (pid > 0) {
			setgroup(ml[i], proc.grpid);
			break;
		}
	}

	if (pid < 0)
		exit_code = 1;

	retval = sp_ret;
	return retval;
}

/*
 * Record the status of a (D)sgmngr in the appropriate bitmap:
 * i.e. whether the process is available or not.
 *
 * First, find the relative position of the specified machine
 * in the MACHINES section of the ubbconfig file. That is,
 * determine the line number of this entry wrt the beginning
 * of the MACHINES section. Once this is determined, set the
 * bit corresponding to the line number in the appropriate
 * bitmap--i.e. either BS_HAVEDBBM or bblmap.
 *
 * If no machine name is specified, default to the "local"
 * machine. NOTE THAT THIS CODE ASSUMES THAT THE LOCAL "HOST"
 * OCCUPIES SLOT 0 OF THE MACHINES CACHE.
 */
bool sg_boot::set_bbm(int ptype, int mid, bool value)
{
	sgc_ctx *SGC = SGT->SGC();
	int	bitno;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("ptype={1}, mid={2}, value={3}") % ptype % mid % value).str(SGLOCALE), &retval);
#endif

	switch (ptype) {
	case BF_DBBM:
		if (value)
			bsflags |= BS_HAVEDBBM;
		retval = true;
		return retval;
	case BF_BBM:
		bitno = SGC->midpidx(mid);
		if (value)
			bitmap.set(bitno);
		else
			bitmap.reset(bitno);
		retval = true;
		return retval;
	default:
		retval = false;
		return retval;
	}
}

void sg_boot::setgroup(int mid, int grpid)
{
	int	i;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("mid={1}, pgid={2}") % mid % grpid).str(SGLOCALE), NULL);
#endif

	if (grpid >= DFLT_PGID)
		return;

	for (i = 0; i < groups.size() && groups[i].grpid != grpid; i++)
		;

	if (i != groups.size()) {
		/*
		 * The grpid is already in our table, now check the server group
		 * and make sure it is the same. If not, print a warning and return
		 * an error.
		 */
		if (mid == groups[i].mid)	/* They match, this ones ok */
			return;

		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Host id mismatch for pgid {1}") % grpid).str(SGLOCALE));
		return;
	}

	// At this point we know we have a new RQADDR and must add it to the table.
	grpchk_t item;
	item.grpid = grpid;
	item.mid = mid;
	groups.push_back(item);
}

int sg_boot::getgroup(int grpid)
{
	int i;
	int retval = BADMID;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("pgid={1}") % grpid).str(SGLOCALE), &retval);
#endif

	for (i = 0; i < groups.size() && groups[i].grpid != grpid ; i++)
		continue;

	if (i < groups.size()) {
		retval = groups[i].mid;
		return retval;
	}

	SGT->_SGT_error_code = SGENOENT;
	return retval;
}

bool sg_boot::doprocloc(const sg_proc_t& proc, vector<int>& ml)
{
	sg_sgte_t sgkey;
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	sgkey.parms.grpid = proc.grpid;
	ml.clear();

	if (proc.admin_grp()) {
		sgkey.parms.midlist[0] = proc.mid;
		sgkey.parms.midlist[1] = BADMID;
		sgkey.parms.curmid = 0;
	} else if (sg_sgte::instance(SGT).retrieve(S_GROUP, &sgkey, &sgkey, NULL) < 0) {
		ml.push_back(BADMID);
		return retval;
	}

	/*
	 * Reduce the list of locations to those machines where we know
	 * there's a sgmngr. If the sgclst is unavailable or there are no sgmngr's
	 * available on the specified machines, return an error indication.
	 * Preview alternate locations, weeding out those locations which are
	 * invalid, or  where there is no sgmngr running.
	 */

	int i = sgkey.parms.curmid;        /* start at current boot location */

	/*
	 * Use a do/while loop because we want to start at curmid and quit
	 * the loop once we get back to curmid, but we don't want to do
	 * our check to soon.
	 */
	do {
		if (sgkey.parms.midlist[i] == BADMID || i >= MAXLMIDSINLIST)
			goto skip;
		/*
		 * Skip invalid locations and print a warning message if the
		 * first mid in the list is invalid.
		 */
		if (!proc.is_dbbm()
			&& ((proc.admin_grp() && !(bsflags & BS_HAVEDBBM))
				|| (!proc.admin_grp() && !bbmok(sgkey.parms.midlist[i])))) {
			if (ml.empty() && !(bsflags & BS_HIDERR))
				warning((_("No sgmngr on hostid = {1}") % SGC->mid2lmid(sgkey.parms.midlist[i])).str(SGLOCALE));
			goto skip;
		}
		ml.push_back(sgkey.parms.midlist[i]);

skip:
		i++;	/* OK, check the next entry */
		// If there is no next entry, then start back at beginning.
		if (sgkey.parms.midlist[i] == BADMID || i >= MAXLMIDSINLIST)
			i = 0;
	} while (i != sgkey.parms.curmid); /* until back at curmid */

	/*
	 * If there were no valid locations, put the first mid for the
	 * group in (for error message reporting).
	 */
	if (ml.empty()) {
		ml.push_back(sgkey.parms.midlist[0]);
		return retval;
	}

	retval = true;
	return retval;
}

}
}

