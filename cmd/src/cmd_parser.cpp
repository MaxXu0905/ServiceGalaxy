#include "cmd_parser.h"

namespace ai
{
namespace sg
{

std::string cmd_parser::d_lmid;
int cmd_parser::d_mid = ALLMID;
bool cmd_parser::d_allmid = false;
bool cmd_parser::d_dbbm_only = false;
std::string cmd_parser::d_qname;
std::string cmd_parser::d_grpname;
int cmd_parser::d_grpid = -1;
int cmd_parser::d_svrid = -1;
std::string cmd_parser::d_svcname;
std::string cmd_parser::d_usrname;
std::string cmd_parser::d_cltname;


std::string cmd_parser::c_lmid;
int cmd_parser::c_mid = ALLMID;
bool cmd_parser::c_allmid = false;
bool cmd_parser::c_dbbm_only = false;
std::string cmd_parser::c_qname;
std::string cmd_parser::c_grpname;
int cmd_parser::c_grpid = -1;
int cmd_parser::c_svrid = -1;
std::string cmd_parser::c_svcname;
std::string cmd_parser::c_usrname;
std::string cmd_parser::c_cltname;

bool cmd_parser::echo = false;
bool cmd_parser::verbose = false;
bool cmd_parser::page = true;

int cmd_parser::global_flags = 0;
sg_proc_t cmd_parser::d_proc;
std::string cmd_parser::err_msg;

std::string cmd_parser::c_uname;
int cmd_parser::bbm_count = -1;
boost::shared_array<sg_ste_t> cmd_parser::auto_bbm_stes;
sg_ste_t * cmd_parser::bbm_stes = NULL;
bool cmd_parser::gotintr = false;

cmd_parser::cmd_parser(int flags_)
	: desc((_("Allowed options")).str(SGLOCALE).c_str()),
	  flags(flags_)
{
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
	;
}

cmd_parser::~cmd_parser()
{
}

bool cmd_parser::allowed() const
{
	sgc_ctx *SGC = SGT->SGC();

	// if we're in private mode, check if this command is valid
	if (global_flags & PRV) {
		if (flags & PRV)
			return true;
		else
			return false;
	}

	// if we're in CONF mode, check if this command is valid
	if (global_flags & CONF) {
		if (flags & CONF)
			return true;
		else
			return false;
	}

	// if we're in BOOT mode, check if this command is valid
	if (global_flags & BOOT) {
		if (flags & BOOT)
			return true;
		else
			return false;
	}

	// check that this command is correct for this type of instantiation.
	if (!(global_flags & SM) && (flags & SM))
		return false;

	// Check that we're in wizard mode if the command requires it.
	if ((global_flags & WIZ) && (flags & WIZ))
		return false;

	if (flags & MASTER) {
		if (SGC->_SGC_bbp == NULL)
			return false;

		for (int i = 0; i < MAX_MASTERS; i++) {
			if (i == SGC->_SGC_bbp->bbparms.current)
				continue;

			const char *lmid = SGC->_SGC_bbp->bbparms.master[i];
			if (lmid[0] == '\0')
				continue;

			int mid = SGC->lmid2mid(lmid);
			if (mid == BADMID)
				continue;

			const char *pmid = SGC->mid2pmid(mid);
			if (pmid == NULL)
				continue;

			if (c_uname != pmid)
				continue;

			return true;
		}

		return false;
	}

	if (flags & MASTERNODE) {
		if (SGC->_SGC_bbp == NULL)
			return false;

		const char *lmid = SGC->_SGC_bbp->bbparms.master[SGC->_SGC_bbp->bbparms.current];
		if (lmid[0] == '\0')
			return false;

		int mid = SGC->lmid2mid(lmid);
		if (mid == BADMID)
			return false;

		const char *pmid = SGC->mid2pmid(mid);
		if (pmid == NULL)
			return false;

		if (c_uname != pmid)
			return false;

		return true;
	}

	if (!(global_flags & ADM)) {
		if (flags & ADM)
			return false;
		else if (flags & READ)
			return true;
		else
			return false;
	}

	return true;
}

void cmd_parser::show_err_msg() const
{
	if (!err_msg.empty())
		std::cout << err_msg << std::endl;
}

void cmd_parser::show_desc() const
{
	std::cout << desc << std::endl;
}

bool cmd_parser::enable_page() const
{
	return (flags & PAGE);
}

void cmd_parser::initialize()
{
	gpp_ctx *GPP = gpp_ctx::instance();
	c_uname = GPP->uname();

	if (global_flags & SM)
		d_mid = SHMMID;
	else
		d_mid = ALLMID;

	if (isatty(0) && isatty(1))
		page = true;
	else
		page = false;

	if (bbm_stes == NULL) {
		bbm_stes = new sg_ste_t[MAXBBM + 1];
		if (bbm_stes == NULL)
			exit(0);

		auto_bbm_stes.reset(bbm_stes);
	}
}

int& cmd_parser::get_global_flags()
{
	return global_flags;
}

bool& cmd_parser::get_echo()
{
	return echo;
}

bool& cmd_parser::get_verbose()
{
	return verbose;
}

bool& cmd_parser::get_page()
{
	return page;
}

void cmd_parser::sigintr(int signo)
{
	gotintr = true;
}

parser_result_t cmd_parser::parse_command_line(int argc, char **argv)
{
	c_lmid = "";
	c_mid = ALLMID;
	c_allmid = false;
	c_dbbm_only = false;
	c_qname = "";
	c_grpname = "";
	c_grpid = -1;
	c_svrid = -1;
	c_svcname = "";

	vm.reset(new po::variables_map());

	try {
		po::store(po::command_line_parser(argc, argv).options(desc).positional(pdesc).run(), *vm);

		if (vm->count("help")) {
			show_desc();
			err_msg = "";
			return PARSE_HELP;
		}

		if (c_allmid)
			c_mid = ALLMID;

		if (vm->count("clst"))
			c_dbbm_only = true;
		else
			c_dbbm_only = d_dbbm_only;

		if (c_allmid && c_dbbm_only)
			return PARSE_BADCOMB;

		po::notify(*vm);
	} catch (exception& ex) {
		err_msg = ex.what();
		return PARSE_CERR;
	}

	sgc_ctx *SGC = SGT->SGC();
	if (!c_lmid.empty()) {
		if ((c_mid = SGC->lmid2mid(c_lmid.c_str())) == BADMID)
			return PARSE_BADMNAM;
	}

	if (!c_grpname.empty()) {
		if (c_grpid != -1)
			return PARSE_BADCOMB;

		if ((c_grpid = gname2gid(c_grpname.c_str())) == -1)
			return PARSE_BADGRP;
	} else if (c_grpid != -1) {
		c_grpname = gid2gname(c_grpid);
	}

	if (vm->count("all"))
		c_allmid = true;
	else
		c_allmid = d_allmid;

	return PARSE_SUCCESS;
}

parser_result_t cmd_parser::get_proc()
{
	sgc_ctx *SGC = SGT->SGC();

	if (SGC->_SGC_bbp == NULL)
		return PARSE_BBNATT;

	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_ste_t skey;
	skey.grpid() = SGC->mid2grp(d_mid);
	skey.svrid() = MNGR_PRCID;
	if ((ste_mgr.retrieve(S_GRPID, &skey, &skey, NULL)) != 1) {
		if (SGT->_SGT_error_code == SGENOENT)
			return PARSE_NOBBM;
		else
			return PARSE_SRERR;
	}

	if (skey.global.status & PSUSPEND)
		return PARSE_BBMDEAD;

	/*
	 * Next we must make sure that the sgmngr on the
	 * requested machine is alive, and that its queue
	 * is not suspended, since we wouldn't be able to
	 * do anything if either case is not true.
	 */
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_proc_t proc;
	proc.pid = skey.pid();
	proc.mid = skey.mid();
	if (!proc_mgr.alive(proc) || SGT->_SGT_error_code == SGETIME)
		return PARSE_BBMDEAD;

	/*
	 * Must retrieve the queue entry to determine
	 * its status.
	 */
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_qte_t qkey;
	qkey.hashlink.rlink = skey.queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &qkey, &qkey, NULL) != 1)
		return PARSE_QERR;

	if (qkey.global.status & SUSPENDED)
		return PARSE_QSUSP;

	d_proc = skey.svrproc();
	return PARSE_SUCCESS;
}

parser_result_t cmd_parser::get_mach()
{
	sg_ste_t ste;
	sg_ste& ste_mgr = sg_ste::instance(SGT);

	ste.svrid() = MNGR_PRCID; //retrieve all sgmngr entries
	if ((bbm_count = ste_mgr.retrieve(S_SVRID, &ste, bbm_stes, NULL)) == -1)
		return PARSE_SRERR;

	return PARSE_SUCCESS;
}

int cmd_parser::gname2gid(const char *grpname) const
{
	sgc_ctx *SGC = SGT->SGC();

	if (SGC->_SGC_bbp == NULL) {
		sg_config& config_mgr = sg_config::instance(SGT);

		for (sg_config::sgt_iterator iter = config_mgr.sgt_begin(); iter != config_mgr.sgt_end(); ++iter) {
			if (strcmp(iter->sgname, grpname))
				return iter->grpid;
		}

		return -1;
	} else {
		sg_sgte& sgte_mgr = sg_sgte::instance(SGT);
		sg_sgte_t sgte;

		strcpy(sgte.parms.sgname, grpname);
		if (sgte_mgr.retrieve(S_GRPNAME, &sgte, &sgte, NULL) < 0)
			return -1;

		return sgte.parms.grpid;
	}
}

string cmd_parser::gid2gname(int grpid) const
{
	sgc_ctx *SGC = SGT->SGC();

	if (SGC->_SGC_bbp == NULL) {
		sg_config& config_mgr = sg_config::instance(SGT);

		for (sg_config::sgt_iterator iter = config_mgr.sgt_begin(); iter != config_mgr.sgt_end(); ++iter) {
			if (iter->grpid == grpid)
				return iter->sgname;
		}

		return boost::lexical_cast<string>(grpid);
	} else {
		sg_sgte_t sgte;
		sg_sgte& sgte_mgr = sg_sgte::instance(SGT);

		sgte.parms.grpid = grpid;
		if (sgte_mgr.retrieve(S_GROUP, &sgte, &sgte, NULL) < 0)
			return boost::lexical_cast<string>(grpid);

		return sgte.parms.sgname;
	}
}

// determine if local info is useful to print
bool cmd_parser::Ptest(int mid, int grpid, int cmid)
{
	/*
	 * cmid == 02 means that this is the summary in "-m all" mode of
	 * psr or psc, so all entries are relevant.
	 *
	 * cmid == -1 means that we are at the sgclst, so ignore the
	 * machine information.
	 *
	 * Otherwise, print the information only if we are on the
	 * same machine as the entry (mid == cmid) AND this is not the
	 * sgclst entry (grpid != CLST_PGID).
	 *
	 * Note that (grpid == CLST_PGID) ONLY for the sgclst's entry.
	 */
	return (cmid == -2
		|| (cmid == -1 && grpid == CLST_PGID)
		|| (mid == cmid && grpid != CLST_PGID));
}

}
}

