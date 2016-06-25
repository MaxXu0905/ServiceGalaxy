#include "prt_parser.h"

namespace bi = boost::interprocess;
namespace bio = boost::io;
namespace po = boost::program_options;
using namespace std;

namespace ai
{
namespace sg
{

d_parser::d_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("all,A", (_("specify default hostid to all nodes")).str(SGLOCALE).c_str())
		("hostid,h", po::value<string>(&d_lmid), (_("specify default hostid")).str(SGLOCALE).c_str())
		("qname,q", po::value<string>(&d_qname), (_("specify default qname")).str(SGLOCALE).c_str())
		("pgname,G", po::value<string>(&d_grpname), (_("specify default pgname")).str(SGLOCALE).c_str())
		("pgid,g", po::value<int>(&d_grpid), (_("specify default pgid")).str(SGLOCALE).c_str())
		("prcid,p", po::value<int>(&d_svrid), (_("specify default prcid")).str(SGLOCALE).c_str())
		("opname,o", po::value<string>(&d_svcname), (_("specify default operation name")).str(SGLOCALE).c_str())
		("usrname,u", po::value<string>(&d_usrname), (_("specify default user name")).str(SGLOCALE).c_str())
		("cltname,c", po::value<string>(&d_cltname), (_("specify default client name")).str(SGLOCALE).c_str())
	;
}

d_parser::~d_parser()
{
}

parser_result_t d_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	if (d_mid != ALLMID) {
		retval = get_proc();
		if (retval != PARSE_SUCCESS)
			return retval;
	}

	if (argc == 1) {
		cout << (_("Default Settings:\n")).str(SGLOCALE);
		cout << (_("         Host ID: ")).str(SGLOCALE) << (!d_lmid.empty() ? d_lmid : (_("(not set)")).str(SGLOCALE)) << endl;
		cout << (_("      Queue Name: ")).str(SGLOCALE) << (!d_qname.empty() ? d_qname : (_("(not set)")).str(SGLOCALE)) << endl;
		cout << (_("     PGroup Name: ")).str(SGLOCALE) << (!d_grpname.empty() ? d_grpname : (_("(not set)")).str(SGLOCALE)) << endl;
		cout << (_("    Process Name: ")).str(SGLOCALE) << (d_svrid != -1 ? boost::lexical_cast<string>(d_svrid) : (_("(not set)")).str(SGLOCALE)) << endl;
		cout << (_("  Operation Name: ")).str(SGLOCALE) << (!d_svcname.empty() ? d_svcname : (_("(not set)")).str(SGLOCALE)) << endl;
		cout << (_("       User Name: ")).str(SGLOCALE) << (!d_usrname.empty() ? d_usrname : (_("(not set)")).str(SGLOCALE)) << endl;
		cout << (_("     Client Name: ")).str(SGLOCALE) << (!d_cltname.empty() ? d_cltname : (_("(not set)")).str(SGLOCALE)) << endl;

	}

	return PARSE_SUCCESS;
}

option_parser::option_parser(const string& option_name_, int flags_, bool& value_)
	: cmd_parser(flags_),
	  option_name(option_name_),
	  value(value_)
{
	string desc_msg = (_("turn {1} on/off, 1st positional parameter takes same effect.") % option_name).str(SGLOCALE);
	desc.add_options()
		("option,o", po::value<string>(&option)->default_value(""), desc_msg.c_str())
	;
	pdesc.add("option", 1);
}

option_parser::~option_parser()
{
}

parser_result_t option_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	if (option.empty()) {
		value = !value;
	} else if (strcasecmp(option.c_str(), "on") == 0) {
		value = true;
	} else if (strcasecmp(option.c_str(), "off") == 0) {
		value = false;
	} else {
		err_msg = (_("ERROR: only on/off is allowed for option 'option'")).str(SGLOCALE);
		return PARSE_CERR;
	}

	if (value)
		cout << option_name << (_(" now on\n")).str(SGLOCALE);
	else
		cout << option_name << (_(" now off\n")).str(SGLOCALE);
	return PARSE_SUCCESS;
}

pclt_parser::pclt_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("hostid,h", po::value<string>(&c_lmid)->default_value(""), (_("produce information of specified hostid")).str(SGLOCALE).c_str())
		("usrname,u", po::value<string>(&c_usrname)->default_value(d_usrname), (_("produce information of specified user name")).str(SGLOCALE).c_str())
		("cltname,c", po::value<string>(&c_cltname)->default_value(d_cltname), (_("produce information of specified client name")).str(SGLOCALE).c_str())
	;
}

pclt_parser::~pclt_parser()
{
}

parser_result_t pclt_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_ste_t skey;
	int nmach;
	int mid;
	bool all = (c_mid == ALLMID);
	vector<sg_rte_t> clients;

	if (c_mid != ALLMID) {
		skey.grpid() = SGC->mid2grp(c_mid);
		skey.svrid() = MNGR_PRCID;
		if (ste_mgr.retrieve(S_GRPID, &skey, bbm_stes, NULL) != -1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_NOBBM;
			else
				return PARSE_SRERR;
		}

		nmach = 1;
	} else {
		if ((retval = get_mach()) != PARSE_SUCCESS)
			return retval;

		nmach = bbm_count;
	}

	for (int mach = 0; mach < nmach && !gotintr; mach++) {
		mid = bbm_stes[0].mid();

		if (bbm_stes[mach].global.status & PSUSPEND) {
			if (!all) {
				err_msg = (_("ERROR: Client information could not be retrieved from site {1}") % SGC->mid2lmid(mid)).str(SGLOCALE);
				return PARSE_CERR;
			}
			continue;
		}

		message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(sg_clinfo_t)));
		if (msg == NULL) {
			err_msg = (_("ERROR: make msg failed - {1}") % SGT->strerror()).str(SGLOCALE);
			return PARSE_CERR;
		}

		sg_rpc_rqst_t *rqst = msg->rpc_rqst();
		rqst->opcode = O_RRTE | VERSION;
		rqst->arg1.flag = CLIENT;
		rqst->datalen = sizeof(sg_clinfo_t);
		sg_clinfo_t *info = reinterpret_cast<sg_clinfo_t *>(rqst->data());
		strcpy(info->usrname, c_usrname.c_str());
		strcpy(info->cltname, c_cltname.c_str());

		if (!rpc_mgr.send_msg(msg, &bbm_stes[mach].svrproc(), SGAWAITRPLY)) {
			if (all)
				continue;

			if (SGT->_SGT_error_code == 0) {
				SGT->_SGT_error_code = SGESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: error during send/receive of remote procedure call")).str(SGLOCALE));
			}
			err_msg = (_("ERROR: error during send/receive of remote procedure call")).str(SGLOCALE);
			return PARSE_CERR;
		}

		sg_rpc_rply_t *rply = msg->rpc_rply();
		if (rply->rtn < 0) {
			if (all)
				continue;

			SGT->_SGT_error_code = rply->error_code;
			err_msg = (_("ERROR: failure on remote procedure call")).str(SGLOCALE);
			return PARSE_CERR;
		}

		if (rply->rtn == 0)
			continue;

		sg_rte_t *cp = reinterpret_cast<sg_rte_t *>(rply->data());
		for (int i = 0; i < rply->rtn; i++)
			clients.push_back(cp[i]);
	}

	if (verbose)
		show_verbose(clients);
	else
		show_terse(clients);

	return PARSE_SUCCESS;
}

void pclt_parser::show_terse(const vector<sg_rte_t>& clients) const
{
	sgc_ctx *SGC = SGT->SGC();

	cout << (_("    HOSTID        User Name       Client Name       Time       Status\n")).str(SGLOCALE)
		<< "--------------- --------------- --------------- -------------- -------\n";

	BOOST_FOREACH(const sg_rte_t& rte, clients) {
		if (gotintr)
			break;

		string dts;
		datetime dt(rte.rt_timestamp);
		dt.iso_string(dts);

		cout << setw(15) << SGC->mid2lmid(rte.mid) << " "
			<< setw(15) << rte.usrname << " "
			<< setw(15) << rte.cltname << " "
			<< setw(14) << dts << " "
			<< sg_client_status_t(rte.rstatus)
			<< endl;
	}
}

void pclt_parser::show_verbose(const vector<sg_rte_t>& clients) const
{
	bool first = true;
	BOOST_FOREACH(const sg_rte_t& rte, clients) {
		if (first)
			first = false;
		else
			cout << "\n";

		cout << (_("           PID: ")).str(SGLOCALE) << rte.pid << "\n";
		cout << (_("        HOSTID: ")).str(SGLOCALE) << rte.mid << "\n";
		cout << (_("         QADDR: ")).str(SGLOCALE) << rte.qaddr << "\n";
		cout << (_("     LAST PGID: ")).str(SGLOCALE) << rte.lastgrp << "\n";
		cout << (_("       USRNAME: ")).str(SGLOCALE) << rte.usrname << "\n";
		cout << (_("       CLTNAME: ")).str(SGLOCALE) << rte.cltname << "\n";
		cout << (_("        PGNAME: ")).str(SGLOCALE) << rte.grpname << "\n";
		cout << (_("    RTIMESTAMP: ")).str(SGLOCALE) << rte.rtimestamp << "\n";
		cout << (_("       RSTATUS: ")).str(SGLOCALE) << sg_client_status_t(rte.rstatus) << "\n";
		cout << (_("      RREQMADE: ")).str(SGLOCALE) << rte.rreqmade << "\n";
		cout << (_("       TIMEOUT: ")).str(SGLOCALE) << rte.rt_svctimeout << "\n";
		cout << (_("     TIMESTAMP: ")).str(SGLOCALE) << rte.rt_timestamp << "\n";
		cout << (_("       RELEASE: ")).str(SGLOCALE) << rte.rt_release << "\n";
		cout << (_("       BLKTIME: ")).str(SGLOCALE) << rte.rt_blktime << "\n";
		cout << (_("      EXECTIME: ")).str(SGLOCALE) << rte.rt_svcexectime << "\n";
		cout << (_("          PGID: ")).str(SGLOCALE) << rte.rt_grpid << "\n";
		cout << (_("         PRCID: ")).str(SGLOCALE) << rte.rt_svrid << "\n";
		cout << (_("  GOOD HANDLES: ")).str(SGLOCALE) << rte.hndl.numghandle << "\n";
		cout << (_(" STALE HANDLES: ")).str(SGLOCALE) << rte.hndl.numshandle << "\n";
		cout << (_("           GEN: ")).str(SGLOCALE) << rte.hndl.gen << "\n";
		cout << (_(" READING QADDR: ")).str(SGLOCALE) << rte.hndl.qaddr << "\n";
		for (int i = 0; i < MAXHANDLES; i++) {
			if (rte.hndl.bbscan_mtype[i] == 0)
				continue;

			cout << (_("         INDEX: ")).str(SGLOCALE) << i << "\n";
			cout << (_("        HANDLE: ")).str(SGLOCALE) << rte.hndl.bbscan_mtype[i] << "\n";
			cout << (_("      RPLYITER: ")).str(SGLOCALE) << rte.hndl.bbscan_rplyiter[i] << "\n";
			cout << (_("      TIMELEFT: ")).str(SGLOCALE) << rte.hndl.bbscan_timeleft[i] << "\n";

		}
	}
}

pg_parser::pg_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("pgname,G", po::value<string>(&d_grpname)->default_value(""), (_("produce information of specified pgname")).str(SGLOCALE).c_str())
		("pgid,g", po::value<int>(&d_grpid)->default_value(-1), (_("produce information of specified pgid")).str(SGLOCALE).c_str())
	;
}

pg_parser::~pg_parser()
{
}

parser_result_t pg_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sgc_ctx *SGC = SGT->SGC();
	sg_sgte& sgte_mgr = sg_sgte::instance(SGT);
	sg_sgte_t key;
	sg_sgte_t sgkey;
	sg_sgte_t *sg;
	int nfound;
	bool first;
	int scope;

	sgkey.parms.grpid = 0;
	sgkey.parms.curmid = 0;
	sgkey.parms.midlist[0] = BADMID;

	/*
	 * take advantage of the fact that a group can only live
	 * on a specific machine - if c_gopt set, use it instead of
	 * c_mopt.
	 */
	if (c_grpid != -1) { // set to a specific group
		scope = S_GROUP;
		sgkey.parms.grpid = c_grpid;
	} else if (c_mid != ALLMID) { // groups from a specific machines
		sgkey.parms.curmid = 0;
		sgkey.parms.midlist[0] = c_mid;
		scope = S_MACHINE;
	} else { // groups from all machines
		sgkey.parms.curmid = 0;
		sgkey.parms.midlist[0] = ALLMID;
		scope = S_MACHINE;
	}

	boost::shared_array<sg_sgte_t> auto_sgtes(new sg_sgte_t[SGC->MAXSGT() + 1]);
	sg_sgte_t * sgtes = auto_sgtes.get();
	if (sgtes == NULL)
		return PARSE_MALFAIL;

	if ((nfound = sgte_mgr.retrieve(scope, &sgkey, sgtes, NULL))< 0) {
		err_msg = (_("WARN: No entries found")).str(SGLOCALE);
		return PARSE_CERR;
	}

	sg = sgtes;
	first = true;
	while (nfound-- && !gotintr) {
		if (first) {
			cout << "\n" << (_("Process group parameters:\n")).str(SGLOCALE);
			first = false;
		} else {
			cout << "\n";
		}
		show(*sg);
		sg++;
	}

	return PARSE_SUCCESS;
}

void pg_parser::show(const sg_sgte_t& sgte) const
{
	sgc_ctx *SGC = SGT->SGC();
	char lmid[MAXLMIDSINLIST][MAX_IDENT + 1];
	const char *s;

	for (int i = 0; i < MAXLMIDSINLIST; i++) {
		if (sgte.parms.midlist[i] == BADMID) {
			lmid[i][0] = '\0';
		} else {
			s = SGC->mid2lmid(sgte.parms.midlist[i]);
			if (s == NULL)
				lmid[i][0] = '\0';
			else
				strcpy(lmid[i], s);
		}
	}

	cout << (_("           PGroup Name: ")).str(SGLOCALE) << sgte.parms.sgname << "\n";
	cout << (_("             PGroup Id: ")).str(SGLOCALE) << sgte.parms.grpid << "\n";

	if (lmid[0][0] != '\0')
		cout << (_("          Primary Node: ")).str(SGLOCALE) << lmid[0] << "\n";

	string alter_lmids;
	for (int i = 1; i < MAXLMIDSINLIST; i++) {
		if (lmid[i][0] != '\0') {
			if (!alter_lmids.empty())
				alter_lmids += ",";
			alter_lmids += lmid[i];
		}
	}
	if (!alter_lmids.empty())
		cout << (_("     Alternative Nodes: ")).str(SGLOCALE) << lmid[0] << "\n";

	cout << (_("          Current Node: ")).str(SGLOCALE) << lmid[sgte.parms.curmid] << "\n";
}

pq_parser::pq_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("qname", po::value<string>(&c_qname)->default_value(d_qname), (_("produce information of specified qname, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;
	pdesc.add("qname", 1);
}

pq_parser::~pq_parser()
{
}

parser_result_t pq_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sgc_ctx *SGC = SGT->SGC();
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_qte_t qkey;
	sg_qte_t *cq;
	sg_ste_t skey;
	sg_ste_t *cs;
	int nfound;
	int sfound;
	int numclients;
	int tnq;
	int totcompleted;
	bool first = true;
	bool byQ = true;
	int i;
	char sans[20];
	char lmid[MAX_IDENT + 1];
	char saddr[MAX_IDENT + 1];
	const char *s;
	int c;

	qkey.saddr()[0] = '\0';	/* Need to do this to retrieve "all" */
	if (!c_qname.empty()) {
		// retrieve the queue specified on the command line
		i = sizeof(qkey.parms.saddr) - 1;
		strncpy(qkey.saddr(), c_qname.c_str(), i);
		qkey.saddr()[i] = '\0';
	} else {
		byQ = false;
	}

	boost::shared_array<sg_qte_t> auto_qtes(new sg_qte_t[SGC->MAXQUES() + 1]);
	sg_qte_t *qtes = auto_qtes.get();
	if (qtes == NULL)
		return PARSE_MALFAIL;

	// retrieve the queue entries based on name
	if((nfound = qte_mgr.retrieve(S_QUEUE, &qkey, qtes, NULL)) == -1) {
		if (SGT->_SGT_error_code == SGENOENT) {
			if (c_qname.empty())
				return PARSE_NOQ;
			else
				return PARSE_BADQ;
		}
		return PARSE_QERR;
	}

	if (!(global_flags & SM)) {
		int nq = 0;
		cq = qtes;
		for (i = 0; i < nfound; i++, cq++) {
			if (strcmp(cq->saddr(), "N/A") == 0)
				// we don't want to update clients' entries
				continue;

			if (!(global_flags & PRV) && !proc_mgr.nwqctl(*cq, SGSTATQ, &nq))
				cq->local.nqueued = 0;	/* ignore error */
			else
				cq->local.nqueued = nq;
		}
	}

	cq = qtes;
	numclients = 0;

	boost::shared_array<sg_ste_t> auto_stes(new sg_ste_t[SGC->MAXSVRS() + 1]);
	sg_ste_t *stes = auto_stes.get();
	if (stes == NULL)
		return PARSE_MALFAIL;

	for ( ;nfound-- && !gotintr; cq++) {
		if (strcmp(cq->saddr(), "N/A") == 0) {
			// we don't want to print out clients' entries
			numclients++;
			continue;
		}

		if (verbose) {
			/*
			 * Note that we do NOT want to print a new line
			 * the first time through.
			 */
			cout << (first ? "" : "\n") << (_("            Prog Name: ")).str(SGLOCALE) << cq->parms.filename << "\n";
			first = false;
			cout << (_("           Queue Name: ")).str(SGLOCALE) << cq->saddr() << "\n";
			cout << (_(" # Processes on Queue: ")).str(SGLOCALE) << cq->global.svrcnt << "\n";
			cout << (_("      Process Options:")).str(SGLOCALE) << sg_qte_options_t(cq->global.status) << "\n";
			cout << (_("          Cost Queued: ")).str(SGLOCALE) << cq->local.wkqueued << "\n";
			cout << (_("             # Queued: ")).str(SGLOCALE) << cq->local.nqueued << "\n";
			cout << (_("    Total Cost Queued: ")).str(SGLOCALE) << cq->local.totwkqueued << "\n";
			cout << (_("       Total # Queued: ")).str(SGLOCALE) << cq->local.totnqueued << "\n";

			skey.queue.qlink = cq->hashlink.rlink;
			if (ste_mgr.retrieve(S_QUEUE, &skey, stes, NULL) >= 0) {
				cs = stes;
				if (cs->svrproc().admin_grp())
					cout << (_("           Queue Type: ADMIN\n")).str(SGLOCALE);
				else
					cout << (_("           Queue Type: USER\n")).str(SGLOCALE);
			}
		} else {
			if (first) {
				cout << setw(15) << (_("Proc Name ")).str(SGLOCALE)
					<< setw(12) << (_("Queue Name ")).str(SGLOCALE)
					<< setw(10) << (_("# Processes ")).str(SGLOCALE)
					<< setw(10) << (_("Cs Queued ")).str(SGLOCALE)
					<< setw(10) << (_("# Queued ")).str(SGLOCALE)
					<< setw(10) << (_("Ave. Len ")).str(SGLOCALE)
					<< setw(11) << (_("Hostid ")).str(SGLOCALE)
					<< endl;
				cout << setw(15) << setfill('-') << " "
					<< setw(12) << " "
					<< setw(10) << " "
					<< setw(10) << " "
					<< setw(10) << " "
					<< setw(10) << " "
					<< setw(11) << " "
					<< setfill(' ') << endl;
				first = false;
			}
			skey.queue.qlink = cq->hashlink.rlink;
			if ((sfound = ste_mgr.retrieve(S_QUEUE, &skey, stes, NULL)) < 0) {
				// ignore error - statistic not needed
				sfound = 0;
			}

			tnq = cq->local.nqueued;

			cs = stes;
			totcompleted = 0;
			while (sfound--) {
				totcompleted += cs->local.ncompleted;
				cs++;
			}

			if (totcompleted) {
			 	/*
				 * calculate avg. queue length and
				 * put into a string for printing.
				 */
				sprintf(sans, "%0.1f", static_cast<float>(cq->local.totnqueued) / totcompleted);
			} else {
				strcpy(sans, "0.0");
			}

			for (c = 0, s = cq->saddr(); *s && c < 11; c++)
				saddr[c] = *s++;
			saddr[c] = '\0';
			if (*s)
				saddr[c - 1] = '+';
			for (c = 0, s = SGC->mid2lmid(stes->mid()); *s && c < 10; c++)
				lmid[c] = *s++;
			lmid[c] = '\0';
			if (*s)
				lmid[c - 1] = '+';

			cout << setw(14) << cq->parms.filename << " "
				<< setw(11) << saddr << " "
				<< setw(9) << cq->global.svrcnt << " "
				<< setw(9) << cq->local.wkqueued << " "
				<< setw(9) << tnq << " "
				<< setw(9) << sans << " "
				<< setw(10) << lmid
				<< endl;
		}
	}

	/*
	 * We want to print client info only if client entries were found
	 * and no queue was given.
	 */
	if (numclients && !byQ) {
		if(numclients > 1)
			cout << "\n" << numclients << (_(" Queue Table Entries allocated for client processes.\n")).str(SGLOCALE);
		else
			cout << "\n" << numclients << (_(" Queue Table Entry allocated for client processes.\n")).str(SGLOCALE);
	}

	return PARSE_SUCCESS;
}

psr_parser::psr_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("hostid,h", po::value<string>(&c_lmid)->default_value(""), (_("produce information of specified hostid")).str(SGLOCALE).c_str())
		("qname,q", po::value<string>(&c_qname)->default_value(d_qname), (_("produce information of specified qname")).str(SGLOCALE).c_str())
		("pgname,G", po::value<string>(&c_grpname)->default_value(""), (_("produce information of specified pgname")).str(SGLOCALE).c_str())
		("pgid,g", po::value<int>(&c_grpid)->default_value(d_grpid), (_("produce information of specified pgid")).str(SGLOCALE).c_str())
		("prcid,p", po::value<int>(&c_svrid)->default_value(d_svrid), (_("produce information of specified prcid")).str(SGLOCALE).c_str())
	;
}

psr_parser::~psr_parser()
{
}

parser_result_t psr_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_ste_t skey;
	sg_qte_t qkey;
	bool none = true;
	int scope;
	int nmach;
	int nfound;
	int i;
	int mid;
	int cnum = 0;
	bool all = (c_mid == ALLMID);

	if (c_mid != ALLMID) {
		nmach = 1;
		mid = 0;
	} else {
		if ((retval = get_mach()) != PARSE_SUCCESS)
			return retval;

		nmach = bbm_count;
	}

	boost::shared_array<sg_ste_t> auto_stes(new sg_ste_t[SGC->MAXSVRS() + 1]);
	sg_ste_t *stes = auto_stes.get();
	if (stes == NULL)
		return PARSE_MALFAIL;

	boost::shared_array<sg_qte_t> auto_qtes(new sg_qte_t[SGC->MAXSVRS() + 1]);
	sg_qte_t *qtes = auto_qtes.get();
	if (qtes == NULL)
		return PARSE_MALFAIL;

	boost::shared_array<sg_ste_t> auto_cstes;
	sg_ste_t *cstes = NULL;
	boost::shared_array<sg_qte_t> auto_cqtes;
	sg_qte_t *cqtes = NULL;

	sg_ste_t *cste;
	sg_qte_t *cqte;
	sg_ste_t *ccste;
	sg_qte_t *ccqte;

	if (all) {
		auto_cstes.reset(new sg_ste_t[SGC->MAXSVRS() + 1]);
		cstes = auto_cstes.get();
		if (cstes == NULL)
			return PARSE_MALFAIL;

		auto_cqtes.reset(new sg_qte_t[SGC->MAXSVRS() + 1]);
		cqtes = auto_cqtes.get();
		if (cqtes == NULL)
			return PARSE_MALFAIL;
	}

	bool first_print = true;
	bool first = true;
	for (int mach = 0; mach < nmach && !gotintr; mach++) {
		if (all) {
			sg_ste_t& bbm_ste = bbm_stes[mach];

			if (bbm_ste.grpid() == CLST_PGID) {
				mid = -1;
				rpc_mgr.setraddr(NULL);	// sgclst entry
			} else {
				mid = bbm_ste.mid();
				rpc_mgr.setraddr(&bbm_ste.svrproc());
			}
		}

		if (first) {
			if (!c_qname.empty()) {
				strcpy(qkey.saddr(), c_qname.c_str());
				if (qte_mgr.retrieve(S_QUEUE, &qkey, &qkey, NULL) != 1) {
					if (all)
						continue;

					if (SGT->_SGT_error_code == SGENOENT)
						return PARSE_BADQ;
					else
						return PARSE_QERR;
				}

				scope = S_QUEUE;
				skey.queue.qlink = qkey.hashlink.rlink;

				// place this entry at the beginning
				*qtes = qkey;
			} else {
				scope = S_MACHINE;
				skey.mid() = ALLMID;
			}
		} else {
			scope = S_BYRLINK;
		}

		if ((nfound = ste_mgr.retrieve(scope, first ? &skey : NULL, stes, NULL)) < 0) {
			if (all) // try next machine
				continue;

			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_NOSR;
			else
				return PARSE_SRERR;
		}

		/*
		 * Since future retrieves will be done by RLINK,
		 * the last entry must be set to NIL.
		 */
		stes[nfound].rlink() = -1;

		if (first) {
			/*
			 * If we are retrieving by queue is
			 * set, then we don't need to retrieve a queue entry
			 * for each server, since all the servers will be on
			 * the same queue. If we're not retrieving by queue,
			 * then a queue entry
			 * must be retrieved for each server entry. Note that
			 * there is a one to one correspondence between
			 * queue and server entries.
			 */
			if (c_qname.empty()) {
				// retrieve Prog names (held in queue entry)
				for (i = 0; i < nfound; i++)
					qtes[i].rlink() = stes[i].queue.qlink;
				qtes[i].rlink() = -1;

				if (qte_mgr.retrieve(S_BYRLINK, NULL, qtes, NULL) != nfound) {
					if (all) // next machine
						continue;

					return PARSE_QERR;
				}
			}
		}

		cste = stes;
		cqte = qtes;
		ccste = cstes;
		ccqte = cqtes;

		while (nfound-- && !gotintr) {
			bool valid = true;

			/*
			 * Now that we have all the entries, we must select
			 * out only those entries that match the default group,
			 * server id, and machine id, if set.
			 */
			if (c_grpid != -1 && cste->grpid() != c_grpid)
				valid = false;
			else if (c_svrid != -1 && cste->svrid() != c_svrid)
				valid = false;
			else if (c_mid >= 0 && cste->mid() != c_mid)
				valid = false;
			else if (cste->grpid() == CLT_PGID && cste->svrid() == CLT_PRCID)
				valid = false;

			if (valid) {
				none = false;

				if (all) {
					if (first) {
						if (!Ptest(cste->mid(), cste->grpid(), mid)) {
							cste->local.total_idle = 0;
							cste->local.total_busy = 0;
							cste->local.ncompleted = 0;
							cste->local.wkcompleted = 0;
						}
						*ccste = *cste;
						*ccqte = *cqte;
						cnum++;
					} else {
						while (update(*ccste, *cste, mid)) {
							ccste++;
							ccqte++;
						}
					}

					ccste++;
					ccqte++;
				} else if (mid != ALLMID) {
					if (!verbose) {
						if (first_print)
							show_head(all, mid);
						show_terse(*cste, *cqte, c_mid);
					} else {
						if (first_print)
							cout << "\n";
						show_verbose(*cste, *cqte);
					}
					first_print = false;
				}
			}

			cste++;
			if (c_qname.empty())
				cqte++;
		}

		first = false;
	}

	if (all && cnum) {
		ccqte = cqtes;
		ccste = cstes;

		while (cnum-- && !gotintr) {
			// Print each cumulative entry
			if (!verbose) {
				if (first_print)
					show_head(true, ALLMID);
				show_terse(*ccste, *ccqte, ALLMID);
			} else {
				if (!first_print)
					cout << "\n";
				show_verbose(*ccste, *ccqte);
			}
			first_print = false;

			ccqte++;
			ccste++;
		}
	}

	if (none)
		return PARSE_NOSR;
	else
		return PARSE_SUCCESS;
}

// Update a server entry for psr
bool psr_parser::update(sg_ste_t& s1, const sg_ste_t& s2, int mid)
{
	if (!(s1 == s2))
		return true;

	/*
	 * If we're on the sgclst, then we should ignore all entries
	 * except the sgclst's, which we should zero out.
	 */
	if (mid == -1) { // m == -1 -> we're on the sgclst
		if (s1.grpid() == CLST_PGID && s1.svrid() == MNGR_PRCID) {
			// zero out the sgclst's entry
			s1.local.total_idle = 0;
			s1.local.total_busy = 0;
			s1.local.ncompleted = 0;
			s1.local.wkcompleted = 0;
		} else {
			// not the sgclst's entry, so ignore it
			return false;
		}
	} else {
		/*
		 * We're on a machine other than the sgclst, so Ptest will
		 * determine if these statistics are meaningful.
		 */
		if (!Ptest(s1.mid(), s1.grpid(), mid))
			return false;
	}

	// Now add s2 to s1
	s1.local.total_idle += s2.local.total_idle;
	s1.local.total_busy += s2.local.total_busy;
	s1.local.ncompleted += s2.local.ncompleted;
	s1.local.wkcompleted += s2.local.wkcompleted;

	return false;
}

void psr_parser::show_head(bool ph, int mid) const
{
	sgc_ctx *SGC = SGT->SGC();

	if (ph) {
		if (mid == ALLMID)
			cout << (_("Totals for all nodes:\n")).str(SGLOCALE);
		else
			cout << (_("Node ")).str(SGLOCALE) << SGC->mid2lmid(mid) << ":\n";
	}

	cout << setw(15) << (_("Proc Name ")).str(SGLOCALE)
		<< setw(12) << (_("Queue Name ")).str(SGLOCALE)
		<< setw(10) << (_("PGName ")).str(SGLOCALE)
		<< setw(6) << (_("ID ")).str(SGLOCALE)
		<< setw(7) << (_("RqDone ")).str(SGLOCALE)
		<< setw(10) << (_("Cost Done ")).str(SGLOCALE)
		<< setw(16) << ((mid != ALLMID) ? (_("Node ")).str(SGLOCALE) : (_("Current Operation ")).str(SGLOCALE))
		<< endl;

	cout << setw(15) << setfill('-') << " "
		<< setw(12) << " "
		<< setw(10) << " "
		<< setw(6) << " "
		<< setw(7) << " "
		<< setw(10) << " "
		<< setw(16) << " "
		<< setfill(' ') << endl;
}

void psr_parser::show_terse(const sg_ste_t& ste, const sg_qte_t& qte, int mid) const
{
	sgc_ctx *SGC = SGT->SGC();

	cout << setw(14) << qte.parms.filename << " "
		<< setw(11) << qte.saddr() << " "
		<< setw(9) << gid2gname(ste.grpid()) << " "
		<< setw(5) << ste.svrid() << " "
		<< setw(6) << ste.local.ncompleted << " "
		<< setw(9) << ste.local.wkcompleted << " "
		<< setw(15);
	if (mid != BADMID) {
		if (ste.global.status & BUSY)
			cout << ste.local.currservice;
		else
			cout << sg_server_status_t(ste.global.status);
	} else {
		cout << SGC->mid2lmid(mid);
	}
	cout << endl;
}

void psr_parser::show_verbose(const sg_ste_t& ste, const sg_qte_t& qte) const
{
	cout << (_("      PROCNAME: ")).str(SGLOCALE) << qte.parms.filename << "\n";
	cout << (_("    TOTAL_IDEL: ")).str(SGLOCALE) << ste.local.total_idle << "\n";
	cout << (_("    TOTAL_BUSY: ")).str(SGLOCALE) << ste.local.total_busy << "\n";
	cout << (_("    NCOMPLETED: ")).str(SGLOCALE) << ste.local.ncompleted << "\n";
	cout << (_("  CSTCOMPLETED: ")).str(SGLOCALE) << ste.local.wkcompleted << "\n";
	cout << (_("        HOSTID: ")).str(SGLOCALE) << ste.mid() << "\n";
	cout << (_("          PGID: ")).str(SGLOCALE) << ste.grpid() << "\n";
	cout << (_("         PRCID: ")).str(SGLOCALE) << ste.svrid() << "\n";
	cout << (_("           PID: ")).str(SGLOCALE) << ste.pid() << "\n";
	cout << (_("        RQADDR: ")).str(SGLOCALE) << ste.rqaddr() << "\n";
	cout << (_("        RPADDR: ")).str(SGLOCALE) << ste.rpaddr() << "\n";
	cout << (_("         CTIME: ")).str(SGLOCALE) << ste.global.ctime << "\n";
	cout << (_("        STATUS: ")).str(SGLOCALE) << sg_server_status_t(ste.global.status) << "\n";
	cout << (_("         MTIME: ")).str(SGLOCALE) << ste.global.mtime << "\n";
	cout << (_("      SRELEASE: ")).str(SGLOCALE) << ste.global.srelease << "\n";
	cout << (_("           GEN: ")).str(SGLOCALE) << ste.global.gen << "\n";

}

psc_parser::psc_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("hostid,h", po::value<string>(&c_lmid)->default_value(""), (_("produce information of specified hostid")).str(SGLOCALE).c_str())
		("qname,q", po::value<string>(&c_qname)->default_value(d_qname), (_("produce information of specified qname")).str(SGLOCALE).c_str())
		("pgname,G", po::value<string>(&c_grpname)->default_value(""), (_("produce information of specified pgname")).str(SGLOCALE).c_str())
		("pgid,g", po::value<int>(&c_grpid)->default_value(d_grpid), (_("produce information of specified pgid")).str(SGLOCALE).c_str())
		("prcid,i", po::value<int>(&c_svrid)->default_value(d_svrid), (_("produce information of specified prcid")).str(SGLOCALE).c_str())
		("opname,o", po::value<string>(&c_svcname)->default_value(""), (_("produce information of specified operation")).str(SGLOCALE).c_str())
	;
}

psc_parser::~psc_parser()
{
}

parser_result_t psc_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_scte_t vkey;
	sg_qte_t qkey;
	bool none = true;
	int scope;
	int nmach;
	int nfound;
	int i;
	int mid;
	int cnum = 0;
	bool all = (c_mid == ALLMID);

	if (c_mid != ALLMID) {
		nmach = 1;
		mid = 0;
	} else {
		if ((retval = get_mach()) != PARSE_SUCCESS)
			return retval;

		nmach = bbm_count;
	}

	boost::shared_array<sg_scte_t> auto_sctes(new sg_scte_t[SGC->MAXSVCS() + 1]);
	sg_scte_t *sctes = auto_sctes.get();
	if (sctes == NULL)
		return PARSE_MALFAIL;

	boost::shared_array<sg_qte_t> auto_qtes(new sg_qte_t[SGC->MAXSVCS() + 1]);
	sg_qte_t *qtes = auto_qtes.get();
	if (qtes == NULL)
		return PARSE_MALFAIL;

	boost::shared_array<sg_scte_t> auto_csctes;
	sg_scte_t *csctes = NULL;
	boost::shared_array<sg_qte_t> auto_cqtes;
	sg_qte_t *cqtes = NULL;

	sg_scte_t *cscte;
	sg_qte_t *cqte;
	sg_scte_t *ccscte;
	sg_qte_t *ccqte;

	if (all) {
		auto_csctes.reset(new sg_scte_t[SGC->MAXSVCS() + 1]);
		csctes = auto_csctes.get();
		if (csctes == NULL)
			return PARSE_MALFAIL;

		auto_cqtes.reset(new sg_qte_t[SGC->MAXSVCS() + 1]);
		cqtes = auto_cqtes.get();
		if (cqtes == NULL)
			return PARSE_MALFAIL;
	}

	bool first_print = true;
	bool first = true;
	for (int mach = 0; mach < nmach && !gotintr; mach++) {
		if (all) {
			sg_ste_t& bbm_ste = bbm_stes[mach];

			if (bbm_ste.grpid() == CLST_PGID) {
				mid = -1;
				rpc_mgr.setraddr(NULL);	// sgclst entry
			} else {
				mid = bbm_ste.mid();
				rpc_mgr.setraddr(&bbm_ste.svrproc());
			}
		}

		if (first) {
			strcpy(vkey.parms.svc_name, c_svcname.c_str());
			if (!c_qname.empty()) {
				strcpy(qkey.saddr(), c_qname.c_str());
				if (qte_mgr.retrieve(S_QUEUE, &qkey, &qkey, NULL) != 1) {
					if (all)
						continue;

					if (SGT->_SGT_error_code == SGENOENT)
						return PARSE_BADQ;
					else
						return PARSE_QERR;
				}

				scope = S_QUEUE;
				vkey.queue.qlink = qkey.hashlink.rlink;

				// place this entry at the beginning
				*qtes = qkey;
			} else {
				scope = S_MACHINE;
				vkey.mid() = ALLMID;
			}
		} else {
			scope = S_BYRLINK;
		}

		if ((nfound = scte_mgr.retrieve(scope, first ? &vkey : NULL, sctes, NULL)) < 0) {
			if (all) // try next machine
				continue;

			if (SGT->_SGT_error_code == SGENOENT) {
				if (c_mid != ALLMID || c_grpid != -1 || c_svrid != -1
					|| !c_qname.empty() || !c_svcname.empty())
					return PARSE_BADSC;
				else
					return PARSE_NOSR;
			} else {
				return PARSE_SCERR;
			}
		}

		/*
		 * Since future retrieves will be done by RLINK,
		 * the last entry must be set to NIL.
		 */
		sctes[nfound].rlink() = -1;

		if (first) {
			/*
			 * If we are retrieving by queue is
			 * set, then we don't need to retrieve a queue entry
			 * for each server, since all the servers will be on
			 * the same queue. If we're not retrieving by queue,
			 * then a queue entry
			 * must be retrieved for each server entry. Note that
			 * there is a one to one correspondence between
			 * queue and server entries.
			 */
			if (c_qname.empty()) {
				// retrieve Prog names (held in queue entry)
				for (i = 0; i < nfound; i++)
					qtes[i].rlink() = sctes[i].queue.qlink;
				qtes[i].rlink() = -1;

				if (qte_mgr.retrieve(S_BYRLINK, NULL, qtes, NULL) != nfound) {
					if (all) // next machine
						continue;

					return PARSE_QERR;
				}
			}
		}

		cscte = sctes;
		cqte = qtes;
		ccscte = csctes;
		ccqte = cqtes;

		while (nfound-- && !gotintr) {
			bool valid = true;

			/*
			 * Now that we have all the entries, we must select
			 * out only those entries that match the default group,
			 * server id, and machine id, if set.
			 */
			if (c_grpid != -1 && cscte->grpid() != c_grpid)
				valid = false;
			else if (c_svrid != -1 && cscte->svrid() != c_svrid)
				valid = false;
			else if (c_mid >= 0 && cscte->mid() != c_mid)
				valid = false;
			else if (cscte->parms.svc_name[0] == '.')
				valid = false;

			if (valid) {
				none = false;

				if (all) {
					if (first) {
						if (!Ptest(cscte->mid(), cscte->grpid(), mid)) {
							cscte->local.ncompleted = 0;
						}
						*ccscte = *cscte;
						*ccqte = *cqte;
						cnum++;
					} else {
						while (update(*ccscte, *cscte, mid)) {
							ccscte++;
							ccqte++;
						}
					}

					ccscte++;
					ccqte++;
				} else if (mid != ALLMID) {
					if (!verbose) {
						if (first_print)
							show_head(all, mid);
						show_terse(*cscte, *cqte, c_mid);
					} else {
						if (first_print)
							cout << "\n";
						show_verbose(*cscte, *cqte);
					}
					first_print = false;
				}
			}

			cscte++;
			if (c_qname.empty())
				cqte++;
		}

		first = false;
	}

	if (all && cnum) {
		ccqte = cqtes;
		ccscte = csctes;

		while (cnum-- && !gotintr) {
			// Print each cumulative entry
			if (!verbose) {
				if (first_print)
					show_head(true, ALLMID);
				show_terse(*ccscte, *ccqte, ALLMID);
			} else {
				if (!first_print)
					cout << "\n";
				show_verbose(*ccscte, *ccqte);
			}
			first_print = false;

			ccqte++;
			ccscte++;
		}
	}

	if (none)
		return PARSE_BADSC;
	else
		return PARSE_SUCCESS;
}

// Update a service entry for psc
bool psc_parser::update(sg_scte_t& s1, const sg_scte_t& s2, int mid)
{
	if (strcmp(s1.parms.svc_name, s2.parms.svc_name) != 0)
		return true;

	/*
	 * If we're on the sgclst, then we should ignore all entries
	 * except the sgclst's, which we should zero out.
	 */
	if (mid == -1) { // m == -1 -> we're on the sgclst
		if (s1.grpid() == CLST_PGID && s1.svrid() == MNGR_PRCID) {
			// zero out the sgclst's entry
			s1.local.ncompleted = 0;
		} else {
			// not the sgclst's entry, so ignore it
			return false;
		}
	} else {
		/*
		 * We're on a machine other than the sgclst, so Ptest will
		 * determine if these statistics are meaningful.
		 */
		if (!Ptest(s1.mid(), s1.grpid(), mid))
			return false;
	}

	// Now add s2 to s1
	s1.local.ncompleted += s2.local.ncompleted;

	return false;
}

void psc_parser::show_head(bool ph, int mid) const
{
	sgc_ctx *SGC = SGT->SGC();

	if (ph) {
		if (mid == ALLMID)
			cout << (_("Totals for all nodes:\n")).str(SGLOCALE);
		else
			cout << (_("Node ")).str(SGLOCALE) << SGC->mid2lmid(mid) << ":\n";
	}
	cout << setw(13) << (_("Operation Name ")).str(SGLOCALE)
		<< setw(13) << (_("Class Name ")).str(SGLOCALE)
		<< setw(15) << (_("Proc Name ")).str(SGLOCALE)
		<< setw(10) << (_("PGName ")).str(SGLOCALE)
		<< setw(6) << (_("ID ")).str(SGLOCALE)
		<< setw(11) << (_("Node ")).str(SGLOCALE)
		<< setw(8) << (_("# Done ")).str(SGLOCALE)
		<< setw(6) << (_("Status ")).str(SGLOCALE)
		<< endl;

	cout << setw(13) << setfill('-') << " "
		<< setw(13) << " "
		<< setw(15) << " "
		<< setw(10) << " "
		<< setw(6) << " "
		<< setw(11) << " "
		<< setw(8) << " "
		<< setw(6) << " "
		<< setfill(' ') << endl;
}

void psc_parser::show_terse(const sg_scte_t& scte, const sg_qte_t& qte, int mid) const
{
	sgc_ctx *SGC = SGT->SGC();

	cout << setw(12) << scte.parms.svc_name << " "
		<< setw(12) << scte.parms.svc_cname << " "
		<< setw(14) << qte.parms.filename << " "
		<< setw(9) << gid2gname(scte.grpid()) << " "
		<< setw(5) << scte.svrid() << " "
		<< setw(10) << SGC->mid2lmid(scte.mid()) << " "
		<< setw(7) << scte.local.ncompleted << " ";
	if (mid == ALLMID)
		cout << "-";
	else
		cout << sg_server_status_t(scte.global.status);
	cout << endl;
}

void psc_parser::show_verbose(const sg_scte_t& scte, const sg_qte_t& qte) const
{
	cout << (_("      PROCNAME: ")).str(SGLOCALE) << qte.parms.filename << "\n";
	cout << (_("    Queue Name: ")).str(SGLOCALE) << qte.saddr() << "\n";
	cout << (_("      PRIORITY: ")).str(SGLOCALE) << scte.parms.priority << "\n";
	cout << (_("       OP_NAME: ")).str(SGLOCALE) << scte.parms.svc_name << "\n";
	cout << (_("      OP_CNAME: ")).str(SGLOCALE) << scte.parms.svc_cname << "\n";
	cout << (_("      OP_INDEX: ")).str(SGLOCALE) << scte.parms.svc_index << "\n";
	cout << (_("        POLICY: ")).str(SGLOCALE) << sg_service_policy_t(scte.parms.svc_policy) << "\n";
	cout << (_("          COST: ")).str(SGLOCALE) << scte.parms.svc_load << "\n";
	cout << (_("     COSTLIMIT: ")).str(SGLOCALE) << scte.parms.load_limit << "\n";
	cout << (_("       TIMEOUT: ")).str(SGLOCALE) << scte.parms.svctimeout << "\n";
	cout << (_("       BLKTIME: ")).str(SGLOCALE) << scte.parms.svcblktime << "\n";
	cout << (_("    NCOMPLETED: ")).str(SGLOCALE) << scte.local.ncompleted << "\n";
	cout << (_("       NQUEUED: ")).str(SGLOCALE) << scte.local.nqueued << "\n";
	cout << (_("        HOSTID: ")).str(SGLOCALE) << scte.mid() << "\n";
	cout << (_("          PGID: ")).str(SGLOCALE) << scte.grpid() << "\n";
	cout << (_("         PRCID: ")).str(SGLOCALE) << scte.svrid() << "\n";
	cout << (_("           PID: ")).str(SGLOCALE) << scte.pid() << "\n";
	cout << (_("        RQADDR: ")).str(SGLOCALE) << scte.rqaddr() << "\n";
	cout << (_("        RPADDR: ")).str(SGLOCALE) << scte.rpaddr() << "\n";
	cout << (_("        STATUS: ")).str(SGLOCALE) << sg_server_status_t(scte.global.status) << "\n";
}

srp_parser::srp_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("pgid,g", po::value<int>(&c_grpid)->required(), (_("produce information of specified pgid")).str(SGLOCALE).c_str())
		("prcid,p", po::value<int>(&c_svrid)->required(), (_("produce information of specified prcid")).str(SGLOCALE).c_str())
	;
}

srp_parser::~srp_parser()
{
}

parser_result_t srp_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sgc_ctx *SGC = SGT->SGC();
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_ste_t skey;
	sg_qte_t qkey;

	// Assign group and id to scope key
	skey.grpid() = c_grpid;
	skey.svrid() = c_svrid;

	// Now retrieve the server entry based on group and id given above.
	if (ste_mgr.retrieve(S_GRPID, &skey, &skey, NULL) < 0) {
		if (SGT->_SGT_error_code == SGENOENT)
			return PARSE_BADSR;
		else
			return PARSE_SRERR;
	}

	// We also need to retrieve the associated queue entry.
	qkey.hashlink.rlink = skey.queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &qkey, &qkey, NULL) < 0)
		return PARSE_QERR;

	cout << (_("         Proc Name: ")).str(SGLOCALE) << qkey.parms.filename << "\n";
	cout << (_("        Queue Name: ")).str(SGLOCALE) << qkey.saddr() << "\n";
	cout << (_("   Process Options: ")).str(SGLOCALE) << sg_qte_options_t(qkey.parms.options) << "\n";
	cout << (_("              PGID: ")).str(SGLOCALE) << skey.grpid() << "\n";
	cout << (_("        Process ID: ")).str(SGLOCALE) << skey.svrid() << "\n";
	cout << (_("           HOST ID: ")).str(SGLOCALE) << SGC->mid2lmid(skey.mid()) << "\n";
	return PARSE_SUCCESS;
}

scp_parser::scp_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("pgid,g", po::value<int>(&c_grpid)->required(), (_("produce information of specified pgid")).str(SGLOCALE).c_str())
		("prcid,p", po::value<int>(&c_svrid)->required(), (_("produce information of specified prcid")).str(SGLOCALE).c_str())
		("opname,o", po::value<string>(&c_svcname)->required(), (_("produce information of specified opname")).str(SGLOCALE).c_str())
	;
}

scp_parser::~scp_parser()
{
}

parser_result_t scp_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_scte_t vkey;

	// Assign values to the scope key
	vkey.grpid() = c_grpid;
	vkey.svrid() = c_svrid;
	strcpy(vkey.parms.svc_name, c_svcname.c_str());

	// Retrieve the service table entry using the values assigned above.
	if (scte_mgr.retrieve(S_GRPID, &vkey, &vkey, NULL) < 0) {
		if (SGT->_SGT_error_code == SGENOENT)
			return PARSE_BADSC;
		else
			return PARSE_SCERR;
	}

	cout << (_("  Operation Name: ")).str(SGLOCALE) << vkey.parms.svc_name << "\n";
	cout << (_("      Class Name: ")).str(SGLOCALE) << vkey.parms.svc_cname << "\n";
	cout << (_(" Operation Index: ")).str(SGLOCALE) << vkey.parms.svc_index << "\n";
	cout << (_("        Priority: ")).str(SGLOCALE) << vkey.parms.priority << "\n";
	cout << (_("            Cost: ")).str(SGLOCALE) << vkey.parms.svc_load << "\n";
	cout << (_("      Cost Limit: ")).str(SGLOCALE) << vkey.parms.load_limit << "\n";
	cout << (_("         Timeout: ")).str(SGLOCALE) << vkey.parms.svctimeout << "\n";
	cout << (_("      Block Time: ")).str(SGLOCALE) << vkey.parms.svcblktime << "\n";
	return PARSE_SUCCESS;
}

stats_parser::stats_parser(int flags_)
	: cmd_parser(flags_)
{
}

stats_parser::~stats_parser()
{
}

parser_result_t stats_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return PARSE_SUCCESS;
}

}
}

