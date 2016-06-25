#include "bb_parser.h"

namespace bi = boost::interprocess;
namespace bio = boost::io;
namespace po = boost::program_options;
using namespace std;

namespace ai
{
namespace sg
{

bbc_parser::bbc_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("hostid,h", po::value<string>(&c_lmid)->default_value(d_lmid), (_("specify hostid to do bbclean")).str(SGLOCALE).c_str())
		("section,z", po::value<string>(&sect_name)->default_value(""), (_("specify section name to do bbclean, REGTAB/PROCTAB/OPTAB/PGTAB")).str(SGLOCALE).c_str())
		("all,A", (_("do bbclean on all nodes")).str(SGLOCALE).c_str())
		("clst,c", (_("do bbclean on sgclst only")).str(SGLOCALE).c_str())
	;
}

bbc_parser::~bbc_parser()
{
}

parser_result_t bbc_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sg_ste_t skey;
	sg_ste_t *cs;
	int scope;
	int nfound;
	int mid;
	ttypes sect = NOTAB;
	int spawn_flags;
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_switch& switch_mgr = sg_switch::instance();

	if (strcasecmp(sect_name.c_str(), "REGTAB") == 0)
		sect = REGTAB;
	else if (strcasecmp(sect_name.c_str(), "PROCTAB") == 0)
		sect = SVRTAB;
	else if (strcasecmp(sect_name.c_str(), "OPTAB") == 0)
		sect = SVCTAB;
	else if (strcasecmp(sect_name.c_str(), "PGTAB") == 0)
		sect = SGTAB;
	else if (sect_name.empty())
		sect = NOTAB;
	else
		return PARSE_SYNERR;

	skey.svrid() = MNGR_PRCID;
	if (c_mid == ALLMID || c_mid == BADMID) {
		// clean all machines
		scope = S_SVRID;
	} else {
		// clean particular machine
		skey.grpid() = SGC->mid2grp(c_mid);
		scope = S_GRPID;
	}

	/*
	 * We set the following addresses so that the statistics are
	 * credited on the correct machine (ie, the default one)
	 */
	rpc_mgr.setuaddr(d_mid != ALLMID ? &d_proc : NULL);
	rpc_mgr.setraddr(d_mid != ALLMID ? &d_proc : NULL);

	BOOST_SCOPE_EXIT((&rpc_mgr)) {
		rpc_mgr.setuaddr(NULL);
	} BOOST_SCOPE_EXIT_END

	boost::shared_array<sg_ste_t> auto_stes(new sg_ste_t[SGC->MAXSVRS() + 1]);
	sg_ste_t *stes = auto_stes.get();
	if (stes == NULL)
		return PARSE_MALFAIL;

	// get the entry of each machine we are to clean
	if ((nfound = ste_mgr.retrieve(scope, &skey, stes, NULL)) < 0) {
		if (SGT->_SGT_error_code == SGENOENT)
			return PARSE_NOBBM;
		else
			return PARSE_SRERR;
	}

	for (cs = stes; nfound-- && !gotintr; cs++) {
		// Save the sgclst for last
		if (cs->svrproc().is_dbbm())
			continue;

		mid = SGC->grp2mid(cs->grpid());
		std::cout << (_("Cleaning the bulletin board on node {1}") % SGC->mid2lmid(mid)).str(SGLOCALE) << ".\n";

		spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
		switch_mgr.bbclean(SGT, mid, sect, spawn_flags);
	}

	std::cout << (_("Cleaning the Distinguished Bulletin Board.\n")).str(SGLOCALE);
	spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
	switch_mgr.bbclean(SGT, -1, sect, spawn_flags);

	return PARSE_SUCCESS;
}

bbi_parser::bbi_parser(int flags_)
	: cmd_parser(flags_)
{
}

bbi_parser::~bbi_parser()
{
}

parser_result_t bbi_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return show_bbi();
}

parser_result_t bbi_parser::show_bbi() const
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	if (bbp == NULL)
		return PARSE_BBNATT;

	bio::ios_all_saver ias(cout);

	cout << (_("Bulletin Board Internals:\n")).str(SGLOCALE);
	cout << (_("             MAGIC: 0x")).str(SGLOCALE) << setbase(16) << bbp->bbparms.magic << "\n";
	ias.restore();
	cout << (_("      CLST RELEASE: ")).str(SGLOCALE) << bbp->bbparms.bbrelease << "\n";
	cout << (_("      CLST VERSION: ")).str(SGLOCALE) << bbp->bbparms.bbversion << "\n";
	cout << (_("        SG VERSION: ")).str(SGLOCALE) << bbp->bbparms.sgversion << "\n";
	cout << (_("        TOTAL SIZE: 0x")).str(SGLOCALE) << setbase(16) << bbp->bbparms.bbsize << "\n";
	ias.restore();
	cout << (_("            CLSTID: ")).str(SGLOCALE) << bbp->bbparms.ipckey << "\n";
	cout << (_("               UID: ")).str(SGLOCALE) << bbp->bbparms.uid << "\n";
	cout << (_("               GID: ")).str(SGLOCALE) << bbp->bbparms.gid << "\n";
	cout << (_("              PERM: ")).str(SGLOCALE) << bbp->bbparms.perm << "\n";
	cout << (_("          QUESBKTS: ")).str(SGLOCALE) << bbp->bbparms.quebkts << "\n";
	cout << (_("            PGBKTS: ")).str(SGLOCALE) << bbp->bbparms.sgtbkts << "\n";
	cout << (_("          PROCBKTS: ")).str(SGLOCALE) << bbp->bbparms.svrbkts << "\n";
	cout << (_("            OPBKTS: ")).str(SGLOCALE) << bbp->bbparms.svcbkts << "\n";
	cout << "\n";

	if (global_flags & PRV)
		return PARSE_SUCCESS;

	sg_rte_t *rte;
	sg_rte& rte_mgr = sg_rte::instance(SGT);

	// print the accessor information
	if (bbp->bbmap.rte_use != -1) {
		rte = rte_mgr.link2ptr(bbp->bbmap.rte_use);
		bool first = true;
		while (rte != NULL && !gotintr) {
			if (rte->pid != 0) {
				if (first) {
					cout << (_("Clients: Slot                    Process ID LOCK TYPE Queue Address\n")).str(SGLOCALE)
						<< setw(32) << setfill('-') << " "
						<< setfill(' ') << "---------- --------- -------------\n";
					first = false;
				}
				cout << setw(31) << rte->slot
					<< setw(11) << rte->pid
					<< setw(10) << rte->lock_type
					<< setw(14) << rte->qaddr << "\n";
			}
			// follow the linked list
			if (rte->nextreg == -1)
				break;
			else
				rte = rte_mgr.link2ptr(rte->nextreg);
		}
	}

	// print the administrative information
	rte = rte_mgr.link2ptr(SGC->MAXACCSRS());
	bool first = true;
	for (int idx = 0; idx < MAXADMIN && !gotintr; idx++, rte++) {
		if (rte->pid != 0) {
			if (rte->pid != 0) {
				if (first) {
					cout << (_("ADMINISTRATIVE Clients: Slot     Process ID LOCK TYPE Queue Address\n")).str(SGLOCALE)
						<< setw(32) << setfill('-') << " "
						<< setfill(' ') << "---------- --------- -------------\n";
					first = false;
				}
				cout << setw(31) << rte->slot
					<< setw(11) << rte->pid
					<< setw(10) << rte->lock_type
					<< setw(14) << rte->qaddr << "\n";
			}
		}
	}

	return PARSE_SUCCESS;
}

bbm_parser::bbm_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("queue,q", (_("produce information of queue section")).str(SGLOCALE).c_str())
		("process,p", (_("produce information of process section")).str(SGLOCALE).c_str())
		("operation,o", (_("produce information of operation section")).str(SGLOCALE).c_str())
	;
}

bbm_parser::~bbm_parser()
{
}

parser_result_t bbm_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	if (vm->count("queue"))
		qsect = true;
	if (vm->count("process"))
		ssect = true;
	if (vm->count("operation"))
		csect = true;

	if (!qsect && !ssect && !csect) {
		qsect = true;
		ssect = true;
		csect = true;
	}

	return show_bbm();
}

parser_result_t bbm_parser::show_bbm() const
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	cout << (_("Bulletin Board Allocation Maps:\n")).str(SGLOCALE);

	/*
	 * In the following calls, the word TABLE: is added to the end of
	 * each string instead of printing it out once in domaps because
	 * we need to left justify the whole field for alignment, and this
	 * was easier and more efficient than doing a strcat in domaps.
	 */

	if (qsect)
		domaps("QUEUE TABLE:", "Queue", SGC->MAXQUES(), SGC->QUEBKTS(), sizeof(sg_qte_t),
			bbp->bbmap.qte_hash, bbp->bbmap.qte_root, bbp->bbmap.qte_free, SGC->_SGC_qte_hash);

	if (ssect)
		domaps("PROCESS TABLE:", "Process", SGC->MAXSVRS(), SGC->SVRBKTS(), sizeof(sg_ste_t),
			bbp->bbmap.ste_hash, bbp->bbmap.ste_root, bbp->bbmap.ste_free, SGC->_SGC_ste_hash);

	if (csect)
		domaps("OPERATION TABLE:", "Operation", SGC->MAXSVCS(), SGC->SVCBKTS(), sizeof(sg_scte_t),
			bbp->bbmap.scte_hash, bbp->bbmap.scte_root, bbp->bbmap.scte_free, SGC->_SGC_scte_hash);

	return PARSE_SUCCESS;
}


void bbm_parser::domaps(const char *uname, const char *lname, int maxents, int numbkts, int entsize, long hash,
	long base, int tfree, int *hashtab) const
{
	int val;
	int lines = 0;

	cout << "\n" << setw(16) << uname << maxents << (_(" Total Entries\n")).str(SGLOCALE);

	// This call is just to get the number of entries free
	int i = Pfmap(base, tfree, entsize, 0);
	if(i == 1)
		cout << (verbose ? "\n" : "") << (_("      Free List: 1 Entry Free\n")).str(SGLOCALE);
	else
		cout << (verbose ? "\n" : "") << (_("      Free List: {1} Entries Free\n") % i).str(SGLOCALE);

	if (verbose) // Here's where we print out the table if necessary
		Pfmap(base, tfree, entsize, 1);

	/*
	 * We really need two values from Pumap here, one being the number of
	 * entries allocated, and the other being the longest collision
	 * chain. Pumap returns the first number, and the
	 * second number is returned via its last argument (val).
	 */
	val = 0;

	/*
	 * This call is just to get the number of entries used and the
	 * longest collision chain.
	 */
	i = Pumap(base, hash, numbkts, entsize, val);
	if (i == 1)
		cout << (verbose ? "\n" : "") << (_("    In-Use List: 1 Entry Allocated, ")).str(SGLOCALE);
	else
		cout << (verbose ? "\n" : "") << (_("    In-Use List: {1} Entries Allocated, ") % i).str(SGLOCALE);
	if (val == 1)
		cout << (_("Longest Collision Chain: 1 Entry\n")).str(SGLOCALE);
	else
		cout << (_("Longest Collision Chain: {1} Entries\n") % val).str(SGLOCALE);

	if (verbose) {
		val = 1; // Here's where we actually print the list, maybe
		Pumap(base, hash, numbkts, entsize, val);
	}

	// Now go through the hash table
	cout << "\n   " << lname << (_(" Hash Table:\n  ")).str(SGLOCALE);
	for (i = 0; i < 10; i++)
		cout << setw(7) << i;
	for (i = 0; i < numbkts && !gotintr; i++) {
		if ((i % 10) == 0) {
			cout << "\n   " << setw(2) << lines << ":";
			lines++;
		}
		cout << setw(6) << hashtab[i] << " ";
	}
	cout << "\n";
}

int bbm_parser::Pfmap(long base, int offset, int elen, int vflag) const
{
	sg_hashlink_t *hp;
	long i = 0;
	sgc_ctx *SGC = SGT->SGC();

	if (offset == -1)
		return 0;

	hp = reinterpret_cast<sg_hashlink_t *>(reinterpret_cast<char *>(SGC->_SGC_bbp) + base + offset * elen);

	if (vflag)
		cout << (_("        Backward Link   Entry   Forward Link\n")).str(SGLOCALE);

	for (; hp != NULL && !gotintr; i++) {
		if (vflag) {
			cout << setw(14) << hp->bhlink
				<< "   <--" << setw(8) << hp->rlink
				<< "     -->   " << hp->fhlink << "\n";
		}

		if (hp->fhlink == -1)
			hp = NULL;
		else
			hp = reinterpret_cast<sg_hashlink_t *>(reinterpret_cast<char *>(SGC->_SGC_bbp) + base + hp->fhlink * elen);
	}

	return i;
}

int bbm_parser::Pumap(long offset, long hashoff, int nbkts, int elen, int& vflag) const
{
	sg_hashlink_t *hp = NULL;
	int entoff;
	int len;
	int maxlen = 0;
	int cnt = 0;
	sgc_ctx *SGC = SGT->SGC();
	bool first = true;

	int *hasht = reinterpret_cast<int *>(reinterpret_cast<char *>(SGC->_SGC_bbp) + hashoff);
	for (int i = 0; i < nbkts; i++) {
		for (entoff = hasht[i], len = 0; entoff != -1; entoff = hp->fhlink, len++) {
			cnt++;
			hp = reinterpret_cast<sg_hashlink_t *>(reinterpret_cast<char *>(SGC->_SGC_bbp) + offset + entoff * elen);
			if (vflag) {
				if (first) {
					cout << (_("        Backward Link   Entry   Forward Link\n")).str(SGLOCALE);
					first = false;
				}
				cout << setw(14) << hp->bhlink
					<< "   <--" << setw(8) << hp->rlink
					<< "     -->   " << hp->fhlink << "\n";
			}
		}
		if (len > maxlen)
			maxlen = len;
	}

	vflag = maxlen;
	return cnt;
}



bbp_parser::bbp_parser(int flags_)
	: cmd_parser(flags_)
{
}

bbp_parser::~bbp_parser()
{
}

parser_result_t bbp_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return show_bbp();
}

parser_result_t bbp_parser::show_bbp() const
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	if (bbp == NULL)
		return PARSE_BBNATT;

	cout << (_("Bulletin Board Parameters:\n")).str(SGLOCALE);
	cout << (_("           OPTIONS: ")).str(SGLOCALE) << sg_resource_options_t(bbp->bbparms.options) << "\n";
	cout << (_("             LDBAL: ")).str(SGLOCALE) << sg_resource_ldbal_t(bbp->bbparms.ldbal) << "\n";
	cout << (_("            MASTER: ")).str(SGLOCALE) << bbp->bbparms.master[bbp->bbparms.current] << "\n";
	cout << (_("          MAXNODES: ")).str(SGLOCALE) << bbp->bbparms.maxpes << "\n";
	cout << (_("           MAXNETS: ")).str(SGLOCALE) << bbp->bbparms.maxnodes << "\n";
	cout << (_("           MAXCLTS: ")).str(SGLOCALE) << bbp->bbparms.maxaccsrs << "\n";
	cout << (_("           MAXQUES: ")).str(SGLOCALE) << bbp->bbparms.maxques << "\n";
	cout << (_("            MAXPGS: ")).str(SGLOCALE) << bbp->bbparms.maxsgt << "\n";
	cout << (_("          MAXPROCS: ")).str(SGLOCALE) << bbp->bbparms.maxsvrs << "\n";
	cout << (_("            MAXOPS: ")).str(SGLOCALE) << bbp->bbparms.maxsvcs << "\n";
	cout << (_("           QUEBKTS: ")).str(SGLOCALE) << bbp->bbparms.quebkts << "\n";
	cout << (_("            PGBKTS: ")).str(SGLOCALE) << bbp->bbparms.sgtbkts << "\n";
	cout << (_("          PROCBKTS: ")).str(SGLOCALE) << bbp->bbparms.svrbkts << "\n";
	cout << (_("            OPBKTS: ")).str(SGLOCALE) << bbp->bbparms.svcbkts << "\n";
	cout << (_("          POLLTIME: ")).str(SGLOCALE) << bbp->bbparms.scan_unit << "\n";
	cout << (_("         ROBUSTINT: ")).str(SGLOCALE) << bbp->bbparms.sanity_scan << "\n";
	cout << (_("         STACKSIZE: 0x")).str(SGLOCALE) << setbase(16) << bbp->bbparms.stack_size << setbase(10) << "\n";
	cout << (_("            MSGMNB: ")).str(SGLOCALE) << bbp->bbparms.max_num_msg << "\n";
	cout << (_("            MSGMAX: ")).str(SGLOCALE) << bbp->bbparms.max_msg_size << "\n";
	cout << (_("        CMPRSLIMIT: ")).str(SGLOCALE) << bbp->bbparms.cmprs_limit << "\n";
	cout << (_("            NFILES: ")).str(SGLOCALE) << bbp->bbparms.max_open_queues << "\n";
	cout << (_("           BLKTIME: ")).str(SGLOCALE) << bbp->bbparms.block_time << "\n";
	cout << (_("          CLSTWAIT: ")).str(SGLOCALE) << bbp->bbparms.dbbm_wait << "\n";
	cout << (_("         MNGRQUERY: ")).str(SGLOCALE) << bbp->bbparms.bbm_query << "\n";
	cout << (_("          INITWAIT: ")).str(SGLOCALE) << bbp->bbparms.init_wait << "\n";
	cout << "\n";

	cout << (_("Bulletin Board Meters:\n")).str(SGLOCALE);
	cout << (_("          Cur Cost: ")).str(SGLOCALE) << bbp->bbmeters.currload << "\n";
	cout << (_("       Cur # Queue: ")).str(SGLOCALE) << bbp->bbmeters.cques << "\n";
	cout << (_("     Cur # Process: ")).str(SGLOCALE) << bbp->bbmeters.csvrs << "\n";
	cout << (_("   Cur # Operation: ")).str(SGLOCALE) << bbp->bbmeters.csvcs << "\n";
	cout << (_("      Cur # PGroup: ")).str(SGLOCALE) << bbp->bbmeters.csgt << "\n";
	cout << (_("      Cur # Client: ")).str(SGLOCALE) << bbp->bbmeters.caccsrs << "\n";
	cout << (_("        Max # Node: ")).str(SGLOCALE) << bbp->bbmeters.maxmachines << "\n";
	cout << (_("       Max # Queue: ")).str(SGLOCALE) << bbp->bbmeters.maxques << "\n";
	cout << (_("     Max # Process: ")).str(SGLOCALE) << bbp->bbmeters.maxsvrs << "\n";
	cout << (_("   Max # Operation: ")).str(SGLOCALE) << bbp->bbmeters.maxsvcs << "\n";
	cout << (_("      Max # PGroup: ")).str(SGLOCALE) << bbp->bbmeters.maxsgt << "\n";
	cout << (_("      Max # Client: ")).str(SGLOCALE) << bbp->bbmeters.maxaccsrs << "\n";
	cout << (_("    # Wk Initiated: ")).str(SGLOCALE) << bbp->bbmeters.wkinitiated << "\n";
	cout << (_("    # Wk Completed: ")).str(SGLOCALE) << bbp->bbmeters.wkcompleted << "\n";
	cout << (_("       Lan Version: ")).str(SGLOCALE) << bbp->bbmeters.lanversion << "\n";
	cout << (_("         # Request: ")).str(SGLOCALE) << bbp->bbmeters.rreqmade << "\n";
	cout << (_("         Timestamp: ")).str(SGLOCALE) << bbp->bbmeters.timestamp << "\n";
	cout << "\n";


	return PARSE_SUCCESS;
}

bbr_parser::bbr_parser(int flags_)
	: cmd_parser(flags_)
{
}

bbr_parser::~bbr_parser()
{
}

parser_result_t bbr_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return PARSE_RELBB;
}

bbs_parser::bbs_parser(int flags_)
	: cmd_parser(flags_)
{
}

bbs_parser::~bbs_parser()
{
}

parser_result_t bbs_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return show_bbs();
}

parser_result_t bbs_parser::show_bbs()
{
	sg_qte_t qkey;
	sg_ste_t skey;
	sg_scte_t sckey;
	sg_sgte_t sgkey;
	int qfound;
	int sfound;
	int scfound;
	int sgfound;
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_sgte& sgte_mgr = sg_sgte::instance(SGT);

	qkey.parms.saddr[0] = '\0';	/* means to retrieve all */
	if ((qfound = qte_mgr.retrieve(S_QUEUE, &qkey, NULL, NULL)) == -1) {
		if (SGT->_SGT_error_code == SGENOENT)
			qfound = 0;
		else
			return PARSE_QERR;
	}

	skey.mid()= ALLMID;		/* means to retrieve all */
	if ((sfound = ste_mgr.retrieve(S_MACHINE, &skey, NULL, NULL)) == -1) {
		if (SGT->_SGT_error_code == SGENOENT)
			sfound = 0;
		else
			return PARSE_SRERR;
	}

	sckey.mid() = ALLMID;		/* means to retrieve all */
	sckey.parms.svc_name[0] = '\0';
	if ((scfound = scte_mgr.retrieve(S_MACHINE, &sckey, NULL, NULL)) == -1) {
		if (SGT->_SGT_error_code == SGENOENT)
			scfound = 0;
		else
			return PARSE_SCERR;
	}

	sgkey.parms.curmid = 0;		/* initialize currmid index */
	sgkey.curmid() = ALLMID;	/* means to retrieve all */
	if ((sgfound = sgte_mgr.retrieve(S_MACHINE, &sgkey, NULL, NULL)) == -1) {
		if (SGT->_SGT_error_code == SGENOENT)
			sgfound = 0;
		else
			return PARSE_SCERR;
	}

	cout << (_("Current Bulletin Board Status:\n")).str(SGLOCALE);
	cout << (_("           Cur # Process: ")).str(SGLOCALE) << sfound << "\n";
	cout << (_("         Cur # Operation: ")).str(SGLOCALE) << scfound << "\n";
	cout << (_("     Cur # Request queue: ")).str(SGLOCALE) << qfound << "\n";
	cout << (_("            Cur # SGroup: ")).str(SGLOCALE) << sgfound << "\n";
	return PARSE_SUCCESS;
}

du_parser::du_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("filename,f", po::value<string>(&c_value)->required(), (_("specify filename to dump, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;
	pdesc.add("filename", 1);
}

du_parser::~du_parser()
{
}

parser_result_t du_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sg_proc_t up;
	message_pointer msg;
	int fd;
	sgc_ctx *SGC = SGT->SGC();

	if (global_flags & SM)	// in shmemflg mode
		up.grpid = SGC->mid2grp(SGC->_SGC_proc.mid);
	else
		up.grpid = CLST_PGID;
	up.svrid = MNGR_PRCID;

	if ((msg = dump(up)) == NULL)
		return PARSE_DUMPERR;

	/*
	 * Now we need to copy the message created by tmdump into the file
	 * the user wants.
	 */
	sg_msghdr_t& msghdr = *msg;
	sg_rpc_rply_t *rply = msg->rpc_rply();
	if (rply->rtn < 0) {
		SGT->_SGT_error_code = rply->error_code;
		if (SGT->_SGT_error_code == SGEOS)
			SGT->_SGT_native_code = msghdr.native_code;
		return PARSE_DUMPERR;
	}

	/*
	 * c_value contains the file name which the user gave on the
	 * command line to dump.
	 */
	if ((fd = open(c_value.c_str(), O_CREAT | O_WRONLY | O_TRUNC, SGC->_SGC_perm)) < 0)
		return PARSE_DUMPERR;

	BOOST_SCOPE_EXIT((&fd)) {
		close(fd);
	} BOOST_SCOPE_EXIT_END

	/*
	 * We want to count the number of characters transferrred so that
	 * we can later check that the copy went OK.
	 */
	if (write(fd, rply->data(), rply->datalen) != rply->datalen) {
		unlink(c_value.c_str());
		return PARSE_DUMPERR;
	}

	cout << (_("Bulletin board dumped into ")).str(SGLOCALE) << c_value << ".\n";
	return PARSE_SUCCESS;
}

message_pointer du_parser::dump(const sg_proc_t& bbm)
{
	message_pointer msg;
	const char *svcname;
	sg_ste_t ste;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);

	/*
	 * We need to retrieve the server entry before making the O_REFRESH
	 * request because we use the same message space and we don't want to
	 * over write the message with the O_RSRVR opcode.  Only happens in
	 * MP mode.
	 */
	ste.svrproc() = bbm;
	if (ste_mgr.retrieve(S_GRPID, &ste, &ste, NULL) < 0) {
		if(global_flags & SM)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: dump failed - can't find sgmngr")).str(SGLOCALE));
		else
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: dump failed - can't find sgclst")).str(SGLOCALE));
		return message_pointer();
	}

	msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(0));
	if (msg == NULL) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: dump failed")).str(SGLOCALE));
		return message_pointer();
	}

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->opcode = O_REFRESH | VERSION;
	rqst->datalen = 0;

	sg_msghdr_t& msghdr = *msg;
	msghdr.flags = SGAWAITRPLY;

	svcname = (global_flags & SM) ? BBM_SVC_NAME : DBBM_SVC_NAME;
	strcpy(msghdr.svc_name, svcname);

	if (!rpc_mgr.send_msg(msg, &ste.svrproc(), SGAWAITRPLY)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: dump failed")).str(SGLOCALE));
		return message_pointer();
	}

	return msg;
}

dumem_parser::dumem_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("filename,f", po::value<string>(&c_value)->required(), (_("specify filename to dump, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;
	pdesc.add("filename", 1);
}

dumem_parser::~dumem_parser()
{
}

parser_result_t dumem_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	string dts;
	sgc_ctx *SGC = SGT->SGC();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_bbparms_t& bbparms = config_mgr.get_bbparms();

	datetime dt(time(0));
	dt.iso_string(dts);

	/*
	 *  mach_name is used as a char ptr that will contain the name of the
	 *  machine.  This will be printed in the output file and if it is in MP
	 *  mode, then it will be used in the function call.
	 */
	string mach_name = GPP->uname();
	string sgconfig;

	for (sg_config::mch_iterator iter = config_mgr.mch_begin(); iter != config_mgr.mch_end(); ++iter) {
		if (mach_name == iter->pmid) {
			sgconfig = iter->sgconfig;
			break;
		}
	}

	sg_bboard_t *temp = SGC->_SGC_bbp;
	SGC->_SGC_bbp = NULL;

	/*
	 *  The following code calls a function using the machine name to get
	 *  the correct IPCKEY for applications running in MP mode. This new key
	 *  will be used to attach to shared memory later on.
	 */
	int mid;
    if ((mid = SGC->pmid2mid(mach_name.c_str())) == BADMID) {
		err_msg = (_("ERROR: Can't find hostid for hostname {1}") % mach_name).str(SGLOCALE);
		return PARSE_CERR;
    }

	SGC->_SGC_wka = bbparms.ipckey;
	int newkey = SGC->midkey(mid);
	SGC->_SGC_bbp = temp;
	boost::shared_ptr<bi::shared_memory_object> mapping_mgr;
	boost::shared_ptr<bi::mapped_region> region_mgr;

	try {
		string shm_name = (boost::format("shm.%1%") % newkey).str();
		mapping_mgr.reset(new bi::shared_memory_object(bi::open_only, shm_name.c_str(), bi::read_write));
	} catch (bi::interprocess_exception& ex) {
		err_msg = (_("ERROR: Can't open shared memory, error_code = {1}, native_error = {2}")
			 % ex.get_error_code()% ex.get_native_error()).str(SGLOCALE);
		return PARSE_CERR;
	}

	bi::offset_t bbsize = 0;
	mapping_mgr->get_size(bbsize);
	region_mgr.reset(new bi::mapped_region(*mapping_mgr, bi::read_write, 0, bbsize, NULL));

	char *addr = reinterpret_cast<char *>(region_mgr->get_address());

	ofstream ofs(c_value.c_str());
	if (!ofs) {
		err_msg = (_("ERROR: Can't open file {1} for dump") % c_value).str(SGLOCALE);
		return PARSE_CERR;
	}

	for (; bbsize > 0; bbsize -= 4096) {
		if (bbsize > 4096)
			ofs.write(addr, 4096);
		else
			ofs.write(addr, bbsize);

		if (!ofs) {
			err_msg = (_("ERROR: Can't write file {1} for dump") % c_value).str(SGLOCALE);
			return PARSE_CERR;
		}
	}

	ofs.close();

	string inf_name = (boost::format("%1%.inf") % c_value).str();
	ofs.open(inf_name.c_str());
	if (!ofs) {
		err_msg = (_("ERROR: Can't open file {1} for dump") % inf_name).str(SGLOCALE);
		return PARSE_CERR;
	}

	ofs << "CLSTID            :" << bbparms.ipckey << "\n"
		<< "SIZE OF SHARE MEM :" << SGC->_SGC_bbp->bbparms.bbsize << "\n"
		<< "MACHINE NAME      :" << mach_name << "\n"
		<< "UID NUMBER        :" << bbparms.uid << "\n"
		<< "GID NUMBER	      :" << bbparms.gid << "\n"
		<< "SGPROFILE         :" << sgconfig << "\n"
		<< "DATE/TIME         :" << dts << "\n";

	cout << (_("Bulletin board dumped into ")).str(SGLOCALE) << c_value << ".\n";
	return PARSE_SUCCESS;
}

loadmem_parser::loadmem_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("filename,f", po::value<string>(&c_value)->required(), (_("specify filename to load, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
		("clstid,c", po::value<int>(&ipckey)->required(), (_("specify clstid to load")).str(SGLOCALE).c_str())
	;
	pdesc.add("filename", 1);
}

loadmem_parser::~loadmem_parser()
{
}

parser_result_t loadmem_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sgc_ctx *SGC = SGT->SGC();
	struct stat statbuf;

	if (stat(c_value.c_str(), &statbuf) < 0) {
		err_msg = (_("ERROR: Can't stat file {1} for load") % c_value).str(SGLOCALE);
		return PARSE_CERR;
	}

	bi::offset_t bbsize = statbuf.st_size;

	/*
	 *  mach_name is used as a char ptr that will contain the name of the
	 *  machine.  This will be printed in the output file and if it is in MP
	 *  mode, then it will be used in the function call.
	 */
	string mach_name = GPP->uname();

	sg_bboard_t *temp = SGC->_SGC_bbp;
	SGC->_SGC_bbp = NULL;

	/*
	 *  The following code calls a function using the machine name to get
	 *  the correct IPCKEY for applications running in MP mode. This new key
	 *  will be used to attach to shared memory later on.
	 */
	int mid;
    if ((mid = SGC->pmid2mid(mach_name.c_str())) == BADMID) {
		err_msg = (_("ERROR: Can't find hostid for hostname {1}") % mach_name).str(SGLOCALE);
		return PARSE_CERR;
    }

	SGC->_SGC_wka = ipckey;
	int newkey = SGC->midkey(mid);
	SGC->_SGC_bbp = temp;
	boost::shared_ptr<bi::shared_memory_object> mapping_mgr;
	boost::shared_ptr<bi::mapped_region> region_mgr;

	try {
		string shm_name = (boost::format("shm.%1%") % newkey).str();
		bi::shared_memory_object::remove(shm_name.c_str());
		mapping_mgr.reset(new bi::shared_memory_object(bi::create_only, shm_name.c_str(), bi::read_write, 0600));
		bi::shared_memory_object mapping_mgr(bi::open_or_create, shm_name.c_str(), bi::read_write, 0600);

		mapping_mgr.truncate(bbsize);
		region_mgr.reset(new bi::mapped_region(mapping_mgr, bi::read_write, 0, bbsize, NULL));
	} catch (bi::interprocess_exception& ex) {
		err_msg = (_("ERROR: Can't open/create shared memory, error_code = {1}, native_error = {2}")
			 % ex.get_error_code()% ex.get_native_error()).str(SGLOCALE);
		return PARSE_CERR;
	}

	char *addr = reinterpret_cast<char *>(region_mgr->get_address());

	ifstream ifs(c_value.c_str());
	if (!ifs) {
		err_msg = (_("ERROR: Can't open file {1} for load") % c_value).str(SGLOCALE);
		return PARSE_CERR;
	}

	for (; bbsize > 0; bbsize -= 4096) {
		if (bbsize > 4096)
			ifs.read(addr, 4096);
		else
			ifs.read(addr, bbsize);

		if (!ifs) {
			err_msg = (_("ERROR: Can't read file {1} for load") % c_value).str(SGLOCALE);
			return PARSE_CERR;
		}
	}

	cout << (_("Bulletin board loaded from ")).str(SGLOCALE) << c_value << ".\n";
	return PARSE_SUCCESS;
}

}
}

