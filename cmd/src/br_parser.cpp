#include "br_parser.h"

namespace bi = boost::interprocess;
namespace bio = boost::io;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
using namespace std;

namespace ai
{
namespace sg
{

m_parser::m_parser(int flags_)
	: cmd_parser(flags_)
{
}

m_parser::~m_parser()
{
}

parser_result_t m_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return PARSE_SUCCESS;
}

pcl_parser::pcl_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("hostid,h", po::value<string>(&c_lmid)->required(), (_("specify hostid to do pclean, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;
	pdesc.add("hostid", 1);
}

pcl_parser::~pcl_parser()
{
}

parser_result_t pcl_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	int cleaned, outof;
	int	stime;
	int	spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
	sgc_ctx *SGC = SGT->SGC();
	sg_switch& switch_mgr = sg_switch::instance();

	if (global_flags& SM)
		return PARSE_PCLSELF;

	// First do a regular clean if possible and sleep to let things settle.
	cout << "\n" << (_("	Cleaning the sgclst.\n")).str(SGLOCALE);
	switch_mgr.bbclean(SGT, -1, NOTAB, spawn_flags);
	stime = (SGC->_SGC_bbp != NULL ? SGC->_SGC_bbp->bbparms.scan_unit : DFLT_POLLTIME);
	cout << (_("	Pausing {1} seconds waiting for system to stabilize.\n") % stime).str(SGLOCALE);
	sleep(stime);

	if (!pclean(cleaned, outof))
		return PARSE_PCLEANFAIL;

	if (cleaned == outof)
		cout << "	" << cleaned << " " << c_lmid << (_(" processes removed from bulletin board\n")).str(SGLOCALE);
	else
		cout << "	" << cleaned << " " << outof << " " << c_lmid << (_(" processes removed from bulletin board\n")).str(SGLOCALE);

	return PARSE_SUCCESS;
}

bool pcl_parser::pclean(int& cleaned, int& outof)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);

	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(0));
	if (msg == NULL)
		return false;

	sg_rpc_rqst_t *rqst = msg->rpc_rqst();
	rqst->datalen = 0;
	rqst->opcode = O_PCLEAN | VERSION;
	rqst->arg1.host = c_mid;

	if (rpc_mgr.asnd(msg) < 0)
		return false;

	sg_rpc_rply_t *rply = msg->rpc_rply();
	cleaned = rply->rtn;
	outof = rply->num;
	return true;
}

pnw_parser::pnw_parser(int flags_)
	: cmd_parser(flags_)
{
}

pnw_parser::~pnw_parser()
{
}

parser_result_t pnw_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return PARSE_SUCCESS;
}

rco_parser::rco_parser(int flags_)
	: cmd_parser(flags_)
{
}

rco_parser::~rco_parser()
{
}

parser_result_t rco_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return PARSE_SUCCESS;
}

stopl_parser::stopl_parser(int flags_)
	: cmd_parser(flags_)
{
}

stopl_parser::~stopl_parser()
{
}

parser_result_t stopl_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	if (c_mid == ALLMID)
		return PARSE_SYNERR;

	sg_agent& agent_mgr = sg_agent::instance(SGT);
	sg_rpc_rply_t *rply = agent_mgr.send(c_mid, OT_EXIT, NULL, 0);
	if (rply == NULL) {
		err_msg = (_("ERROR: Could not send message to sgagent")).str(SGLOCALE);
		return PARSE_CERR;
	}

	if (rply->rtn == -1) {
		err_msg = (_("ERROR: Failed to stop sgagent")).str(SGLOCALE);
		return PARSE_CERR;
	}

	cout << (_("sgagent will stop in a few seconds.\n")).str(SGLOCALE);
	return PARSE_SUCCESS;
}

}
}

