#include "chg_parser.h"

namespace ai
{
namespace sg
{

adv_parser::adv_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("opname,o", po::value<string>(&c_svcname)->required(), (_("operation name to advertise, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
		("class,c", po::value<string>(&class_name)->default_value(""), (_("operation implementation to advertise")).str(SGLOCALE).c_str())
		("qname,q", po::value<string>(&c_qname)->default_value(d_qname), (_("advertise operation on specified qname")).str(SGLOCALE).c_str())
		("pgname,G", po::value<string>(&c_grpname)->default_value(""), (_("advertise operation on specified pgname")).str(SGLOCALE).c_str())
		("pgid,g", po::value<int>(&c_grpid)->default_value(d_grpid), (_("advertise operation on specified pgid")).str(SGLOCALE).c_str())
		("prcid,p", po::value<int>(&c_svrid)->default_value(d_svrid), (_("advertise operation on specified prcid")).str(SGLOCALE).c_str())
	;
	pdesc.add("opname", 1);
}

adv_parser::~adv_parser()
{
}

parser_result_t adv_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	if (class_name.empty())
		class_name = c_svcname;

	sg_qte_t qkey;
	sg_ste_t skey;
	sg_ste_t *cs;
	sgid_t sgid;
	sgid_t *sgids;
	boost::shared_array<sgid_t> auto_sgids;
	int nfound = 0;
	bool valid = false;
	sgc_ctx *SGC = SGT->SGC();
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);

	if (c_qname.empty() && c_grpid != -1 && c_svrid != -1) {
		/*
		 * Do this only if no queue was given. However, we must
		 * have both the group name and the server id number.
		 */
		skey.grpid() = c_grpid;
		skey.svrid() = c_svrid;

		/*
		 * We must retrieve the server entry to
		 * determine the queue this server is on.
		 */
		if (ste_mgr.retrieve(S_GRPID, &skey, &skey, &sgid) != 1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADSR; /* no such server */
			else
				return PARSE_SRERR;
		}

		cs = &skey;
		qkey.hashlink.rlink = skey.queue.qlink;

		// now get the queue entry
		if (qte_mgr.retrieve(S_BYRLINK, &qkey, &qkey, NULL) != 1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADQ; /* no such queue */
			else
				return PARSE_QERR;
		}
	} else if (c_qname.empty() && (c_grpid != -1 || c_svrid != -1)) {
		/*
		 * This is the case where no queue was given and that
		 * either the c_gopt or c_iopt was set.
		 * Something's missing, so we'll return
		 * an error.
		 */
		if (c_grpid == -1)
			return PARSE_NOGID;

		if (c_svrid == -1)
			return PARSE_NOSID;
	} else if (!c_qname.empty()) {
		// A queue was given, so we can retrieve its entry directly.
		strcpy(qkey.saddr(), c_qname.c_str());
		if (qte_mgr.retrieve(S_QUEUE, &qkey, &qkey, NULL) != 1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADQ;		/* no such queue */
			else
				return PARSE_QERR;
		}

		auto_sgids.reset(new sgid_t[SGC->MAXSVRS()]);
		sgids = auto_sgids.get();
		if (sgids == NULL)
			return PARSE_MALFAIL;

		boost::shared_array<sg_ste_t> auto_stes(new sg_ste_t[SGC->MAXSVRS() + 1]);
		sg_ste_t *stes = auto_stes.get();
		if (stes == NULL)
			return PARSE_MALFAIL;

		/*
		 * Either a queue OR a group and server id can be specified
		 * to indicate on which queue the advertisment is to
		 * occur (since it's done across the queue). However, if
		 * a queue is given on the command line, for example, and a
		 * default group and/or server id are set, we must check to see
		 * whether that group and/or server id are valid on
		 * that queue. Therefore, we must retrieve all the servers
		 * on that queue first. Note that we retrieve all the
		 * servers anyway because we need the value "nfound".
		 */
		skey.queue.qlink = qkey.hashlink.rlink;

		if ((nfound = ste_mgr.retrieve(S_QUEUE, &skey, stes, sgids)) < 0) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADSR;
			else
				return PARSE_SRERR;
		}

		cs = stes;		/* point to beginning of array */
		if (c_grpid != -1 || c_svrid != -1) {
			int i = nfound;

			while (i--) {
				/*
				 * Go through each server until we find one
				 * that matches group and/or server id.
				 */
				valid = true;
				if (c_grpid != -1 && cs->grpid() != c_grpid)
					valid = false;
				if (c_svrid != -1 && cs->svrid() != c_svrid)
					valid = false;

				if (valid)	// found one that matched
					break;
				cs++;
			}

			if (!valid)
				return PARSE_BADCOMB;	/* no such server/id on q */
		}
		skey = *cs;
		cs = &skey;
	} else {	/* neither queue nor group and/or id specified */
		return PARSE_SRNOTSPEC;
	}

	if (cs->svrproc().admin_grp()) {
		err_msg = (_("ERROR: Cannot advertise operations for administrative processes")).str(SGLOCALE);
		return PARSE_CERR;
	}

	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	message_pointer msg = rpc_mgr.create_msg(sg_rpc_rqst_t::need(sizeof(int) + c_svcname.length() + class_name.length() + 2));
	if (msg == NULL)
		return PARSE_MALFAIL;

	sg_msghdr_t& msghdr = *msg;
	msghdr.sendmtype = SENDBASE;
	int *l = reinterpret_cast<int *>(msg->data());
	*l = 0;
	strcpy(reinterpret_cast<char *>(l) + sizeof(*l), c_svcname.c_str());
	strcpy(reinterpret_cast<char *>(l) + sizeof(*l) + c_svcname.length() + 1, class_name.c_str());
	valid = true;

	sg_meta& meta_mgr = sg_meta::instance(SGT);
	if (!meta_mgr.metasnd(msg, &cs->svrproc(), SGAWAITRPLY | SGADVMETA)
		|| msg->rpc_rply()->rtn == BADSGID) {
		valid = false;
		SGT->_SGT_error_code = msghdr.error_code;
		if (SGT->_SGT_error_code == SGEDUPENT) { // already advertised on queue
			err_msg = (_("WARN: {1} is already advertised on queue {2}") % c_svcname % qkey.saddr()).str(SGLOCALE);
		} else {
			if (!c_qname.empty())
				err_msg = (_("ERROR: Error advertising {1} on queue {2}") % c_svcname % c_qname).str(SGLOCALE);
			else
				err_msg = (_("ERROR: Error advertising {1} on pgroup {2} process id {3}") % c_svcname % c_grpname % c_svrid).str(SGLOCALE);
		}
	} else {
		if (c_qname.empty())
			nfound = qkey.global.svrcnt;

		if (nfound <= 1)
			std::cout << (_("{1} advertised on {2} process on queue {3}.") % c_svcname % nfound % qkey.saddr()).str(SGLOCALE);
		else
			std::cout << (_("{1} advertised on {2} processes on queue {3}.") % c_svcname % nfound % qkey.saddr()).str(SGLOCALE);
		std::cout << "\n";
	}

	if (valid)
		return PARSE_SUCCESS;
	else
		return PARSE_CERR;
}

una_parser::una_parser(int flags_)
	: cmd_parser(flags_)
{
	desc.add_options()
		("opname,o", po::value<string>(&c_svcname)->required(), (_("operation name to unadvertise, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
		("qname,q", po::value<string>(&c_qname)->default_value(d_qname), (_("unadvertise operation on specified qname")).str(SGLOCALE).c_str())
		("pgname,G", po::value<string>(&c_grpname)->default_value(""), (_("unadvertise operation on specified pgname")).str(SGLOCALE).c_str())
		("pgid,g", po::value<int>(&c_grpid)->default_value(d_grpid), (_("unadvertise operation on specified pgid")).str(SGLOCALE).c_str())
		("prcid,p", po::value<int>(&c_svrid)->default_value(d_svrid), (_("unadvertise operation on specified prcid")).str(SGLOCALE).c_str())
	;
	pdesc.add("opname", 1);
}

una_parser::~una_parser()
{
}

parser_result_t una_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	sg_qte_t qkey;
	sg_scte_t scte;
	sg_ste_t skey;
	sg_ste_t *cs;
	sgid_t sgid;
	int nfound;
	bool valid = false;
	sgc_ctx *SGC = SGT->SGC();
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);

	memset(&scte, '\0', sizeof(sg_scte_t));

	/*
	 * All this routine needs is the sgid for the service to
	 * be unadvertised. Note that we can get this sgid if we
	 * have the service name, group and id number.
	 */
	if (c_qname.empty() && (c_grpid != -1 || c_svrid != -1)) {
		/*
		 * Since no queue was given, all we need to check
		 * is that BOTH a group and id exists. If they do,
		 * then we can simply go on to retrieve the service entry.
		 */
		if (c_grpid == -1)
			return PARSE_NOGID;

		if (c_svrid == -1)
			return PARSE_NOSID;
	} else if (!c_qname.empty()) {

		/*
		 * In this case, since a queue was given, we do the
		 * following: We first retrieve the entry corresponding
		 * to the given queue name, and then retrieve all the
		 * servers on that queue. After checking the validity
		 * of the default group and id, if set, we then get
		 * the needed group and id from the first server
		 * entry.
		 */

		/*
		 * First retrieve the queue entry, based on the queue
		 * name given.
		 */
		strcpy(qkey.saddr(), c_qname.c_str());
		if (qte_mgr.retrieve(S_QUEUE, &qkey, &qkey, NULL) != 1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADQ;		/* no such queue */
			else
				return PARSE_QERR;
		}
		skey.queue.qlink = qkey.hashlink.rlink;

		/*
		 * Either a queue OR a group and server id can be specified
		 * to indicate on which queue the unadvertisment is to
		 * occur (since it's done across the queue). However, if
		 * a queue is given on the command line, for example, and a
		 * default group and/or server id are set, we must check to see
		 * whether that group and/or server id are valid on
		 * that queue. Therefore, we must retrieve all the servers
		 * on that queue first.
		 */
		boost::shared_array<sg_ste_t> auto_stes(new sg_ste_t[SGC->MAXSVRS() + 1]);
		sg_ste_t *stes = auto_stes.get();
		if (stes == NULL)
			return PARSE_MALFAIL;

		if ((nfound = ste_mgr.retrieve(S_QUEUE, &skey, stes, NULL)) < 0) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADSR;
			else
				return PARSE_SRERR;
		}

		if (c_grpid != -1 || c_svrid != -1) {
			cs = stes;
			while (nfound--) {
				/*
				 * go through each server until we find one
				 * that matches group and/or server id
				 */
				valid = true;
				if (c_grpid != -1 && cs->grpid() != c_grpid)
					valid = false;
				if (c_svrid != -1 && cs->svrid() != c_svrid)
					valid = false;
				if (valid)
					break;
				cs++;
			}

			if (!valid)
				return PARSE_BADCOMB;
		}

		/*
		 * Everything was okay, so now we can get the group and
		 * id from the first entry in the server array. Note that
		 * there's a slight window here from the time we retrieved
		 * the entries to the time we actually use the group and
		 * id; that server could have gone away! Highly unlikely,
		 * though.
		 */
		c_grpid = stes->grpid();
		c_svrid = stes->svrid();
	} else {
		return PARSE_SRNOTSPEC;
	}

	/*
	 * Now that we have the group and id, either explicitly or implicitly,
	 * we can retrieve the corresponding service table entry and sgid.
	 */
	int i = sizeof(scte.parms.svc_name) - 1;
	strncpy(scte.parms.svc_name, c_svcname.c_str(),i);
	scte.parms.svc_name[i] = '\0';

	scte.grpid() = c_grpid;
	scte.svrid() = c_svrid;

	valid = true;
	if (scte_mgr.retrieve(S_GRPID, &scte, &scte, &sgid) < 0) {
		/*
		 * Note that if the service doesn't exist (SGENOENT), we
		 * want to form a customized error message rather than
		 * just returning an error code.
		 */
		if (SGT->_SGT_error_code == SGENOENT)
			return PARSE_SCERR;
		else
			valid = false;
	}

	if (!valid) {
		if (!c_qname.empty())
			err_msg = (_("{1} is not advertised on queue {2}") % c_svcname % c_qname).str(SGLOCALE);
		else
			err_msg = (_("{1} is not advertised on process {2}/{3}") % c_svcname % c_grpname % c_svrid).str(SGLOCALE);
		return PARSE_CERR;
	} else {
		// If valid is true, we finally call unadvertise.
		sg_api& api_mgr = sg_api::instance(SGT);
		if ((nfound = api_mgr.unadvertise_internal(sgid, U_GLOBAL)) == 0) {
			if (!c_qname.empty())
				err_msg = (_("Error removing {1} from queue {2}") % c_svcname % c_qname).str(SGLOCALE);
			else
				err_msg = (_("Error removing {1} from process {2}/{3}") % c_svcname % c_grpname % c_svrid).str(SGLOCALE);
			return PARSE_CERR;
		}
	}

	if (!c_qname.empty()) {
		if (nfound > 1)
			std::cout << (_("{1} removed from {2} processes on queue {3}.") % c_svcname % nfound % qkey.saddr()).str(SGLOCALE);
		else
			std::cout << (_("{1} removed from {2} process on queue {3}.") % c_svcname % nfound % qkey.saddr()).str(SGLOCALE);
	} else {
		if (nfound > 1)
			std::cout << (_("{1} removed from {2} processes.") % c_svcname % nfound).str(SGLOCALE);
		else
			std::cout << (_("{1} removed from {2} process.") % c_svcname % nfound).str(SGLOCALE);
	}
	std::cout << "\n";

	return PARSE_SUCCESS;
}

lp_parser::lp_parser(int flags_, int type_)
	: cmd_parser(flags_),
	  type(type_)
{
	if (type == LOAD) {
		desc.add_options()
			("hostid,h", po::value<string>(&c_lmid)->default_value(d_lmid), (_("change cost on specified hostid")).str(SGLOCALE).c_str())
			("opname,o", po::value<string>(&c_svcname)->required(), (_("change cost on specified operation name")).str(SGLOCALE).c_str())
			("qname,q", po::value<string>(&c_qname)->default_value(d_qname), (_("change cost on specified qname")).str(SGLOCALE).c_str())
			("pgname,G", po::value<string>(&c_grpname)->default_value(""), (_("change cost on specified pgname")).str(SGLOCALE).c_str())
			("pgid,g", po::value<int>(&c_grpid)->default_value(d_grpid), (_("change cost on specified pgid")).str(SGLOCALE).c_str())
			("prcid,p", po::value<int>(&c_svrid)->default_value(d_svrid), (_("chang cost on specified prcid")).str(SGLOCALE).c_str())
			("value,v", po::value<long>(&c_value)->required(), (_("cost value to be changed, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
		;
	} else {
		desc.add_options()
			("hostid,h", po::value<string>(&c_lmid)->default_value(d_lmid), (_("change priority on specified hostid")).str(SGLOCALE).c_str())
			("opname,o", po::value<string>(&c_svcname)->required(), (_("change priority on specified operation name")).str(SGLOCALE).c_str())
			("qname,q", po::value<string>(&c_qname)->default_value(d_qname), (_("change priority on specified qname")).str(SGLOCALE).c_str())
			("pgname,G", po::value<string>(&c_grpname)->default_value(""), (_("change priority on specified pgname")).str(SGLOCALE).c_str())
			("pgid,g", po::value<int>(&c_grpid)->default_value(d_grpid), (_("change priority on specified pgid")).str(SGLOCALE).c_str())
			("prcid,p", po::value<int>(&c_svrid)->default_value(d_svrid), (_("change priority on specified prcid")).str(SGLOCALE).c_str())
			("value,v", po::value<long>(&c_value)->required(), (_("priority value to be changed, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
		;
	}
	pdesc.add("value", 1);
}

lp_parser::~lp_parser()
{
}

parser_result_t lp_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	if (type == LOAD)
		return chg_lp(LOAD, "load", 1, 0x7fff);
	else
		return chg_lp(PRIO, "priority", 1, 100);
}

parser_result_t lp_parser::chg_lp(int type, const string& name, long lrange, long hrange)
{
	/*
	 * This routine will change the load, priority, or time limit
	 * of a service, depending on what type is. Note that if this
	 * service is advertized by a server that is part of a MSSQ set,
	 * then the server id will be ignored, and the change will be made
	 * to all servers in that MSSQ set.
	 */
	sg_scte_t vkey;
	sg_scte_t *cv;
	sg_ste_t skey;
	sg_qte_t qkey;
	int nfound = 0;
	int i;
	int update_type;
	bool valid = false;
	sgid_t sgid;
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);

	if (c_value < lrange || c_value > hrange) {
		err_msg = (_("ERROR: The new {1} is not within the range of {2}-{3}") % name % lrange % hrange).str(SGLOCALE);
		return PARSE_CERR;
	}

	memset(&vkey, 0, sizeof(vkey));
	memset(&skey, 0, sizeof(skey));
	memset(&qkey, 0, sizeof(qkey));

	strcpy(vkey.parms.svc_name, c_svcname.c_str());

	/*
	 * If a machine name was given on the command line, then we are
	 * to change the value on that machine only. Otherwise, the
	 * change is made everywhere.
	 *
	 * Some very important things happen here: The new value of
	 * _tmsetuaddr is valid only for the life of this routine.
	 * Therefore, it must be reset to NULL upon exiting. Also,
	 * _tmsetraddr should be reset to what it was upon entering.
	 */

	if (c_mid == -1) {
		// all machines or sgclst which is equivalent to all machines
		update_type = U_GLOBAL;
		rpc_mgr.setraddr(NULL);
		rpc_mgr.setuaddr(NULL);
	} else {
		update_type = U_LOCAL;
		/*
		 * We need to retrieve the entry of the sgmngr on
		 * the given machine so we can get its TMPROC.
		 */
		skey.grpid() = SGC->mid2grp(c_mid);
		skey.svrid() = MNGR_PRCID;
		if (ste_mgr.retrieve(S_GRPID, &skey, &skey, NULL) != 1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_NOBBM;
			else
				return PARSE_SRERR;
		}
		/*
		 * In this case, where we're changing a particular
		 * machine only, we must set both the raddr and uaddr.
		 */
		rpc_mgr.setraddr(&skey.svrproc());
		rpc_mgr.setuaddr(&skey.svrproc());
	}

	BOOST_SCOPE_EXIT((&rpc_mgr)(&d_mid)(&d_proc)) {
		rpc_mgr.setraddr(d_mid != ALLMID ? &d_proc : NULL);
		rpc_mgr.setuaddr(NULL);
	} BOOST_SCOPE_EXIT_END

	boost::shared_array<sg_scte_t> auto_sctes(new sg_scte_t[SGC->MAXSVCS() + 1]);
	sg_scte_t *sctes = auto_sctes.get();
	if (sctes == NULL)
		return PARSE_MALFAIL;

	if (c_qname.empty() && c_grpid != -1 && c_svrid != -1) {
		/*
		 * Do this only if no queue was given. However, we must
		 * have both the group name and the server id number.
		 * But remember, the server id will be ignored if service
		 * is advertised by server that is part of an MSSQ set.
		 */

		/*
		 * See if MSSQ set. We must retrieve the server entry to
		 * determine the queue this server is on.
		 */
		skey.grpid() = c_grpid;
		skey.svrid() = c_svrid;
		if (ste_mgr.retrieve(S_GRPID, &skey, &skey, &sgid) != 1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADSR; /* no such server */
			else
				return PARSE_SRERR;
		}

		qkey.hashlink.rlink = skey.queue.qlink;
		// now get the queue entry
		if (qte_mgr.retrieve(S_BYRLINK, &qkey, &qkey, NULL) != 1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADQ; /* no such queue */
			else
				return PARSE_QERR;
		}

		/*
		 * Now retrieve the service's entry. Note that the
		 * service name has already been set above.
		 */
		if (qkey.global.svrcnt == 1) {
			// Not MSSQ. Retrieve only specific server id.
			vkey.grpid() = c_grpid;
			vkey.svrid() = c_svrid;
			if ((nfound = scte_mgr.retrieve(S_GRPID, &vkey, sctes, NULL)) != 1) {
				if (SGT->_SGT_error_code == SGENOENT)
					return PARSE_BADSC;
				else
					return PARSE_SCERR;
			}
		} else {
			// MSSQ. Retrieve all entries on that queue.
			vkey.queue.qlink = qkey.hashlink.rlink;
			if ((nfound = scte_mgr.retrieve(S_QUEUE, &vkey, sctes, NULL)) == -1) {
				if (SGT->_SGT_error_code == SGENOENT)
					return PARSE_BADSC;
				else
					return PARSE_SCERR;
			}
		}
	} if (c_qname.empty() && (c_grpid != -1 || c_svrid != -1)) {
		/*
		 * This is the case where no queue was given and that
		 * either the c_gopt or c_iopt was set.
		 * Something's missing, so we'll return
		 * an error.
		 */
		if (c_grpid == -1)
			return PARSE_NOGID;

		if (c_svrid == -1)
			return PARSE_NOSID;
	} else if (!c_qname.empty()) {
		/*
		 * We're given a queue, so we should change the value on all
		 * servers on that queue. In addition to retrieving all
		 * the entries on the queue, we must also check the validity
		 * of the default group and server id, if set.
		 */
		strcpy(qkey.saddr(), c_qname.c_str());
		if (qte_mgr.retrieve(S_QUEUE, &qkey, &qkey, NULL) != 1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADQ;
			else
				return PARSE_QERR;
		}
		// Now retrieve all the entries on that queue.
		vkey.queue.qlink = qkey.hashlink.rlink;
		if ((nfound = scte_mgr.retrieve(S_QUEUE, &vkey, sctes, NULL)) == -1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADSC;
			else
				return PARSE_SCERR;
		}
		if (c_grpid != -1 || c_svrid != -1) {
			/*
			 * Here is where we check the validity of the
			 * default group and server id, if set.
			 */
			i = nfound;	/* we need nfound later */
			cv = sctes;	/* start at beginning of the array */
			while (i--) {
				valid = TRUE;
				if (c_grpid != -1 && cv->grpid() != c_grpid) {
					valid = false;
					break;
				}
				/*
				 * Note that if a server id is set, then we want
				 * to change the value on that server ONLY!
 				 * On the other hand, if MSSQ set, then let's
				 * ignore the server id, since we want to
				 * change all servers in the MSSQ set.
				 */
				if (c_svrid != -1 && qkey.global.svrcnt == 1) {
					if (cv->svrid() != c_svrid) {
						valid = false;
					} else {
						/*
						 * We only want to change one entry, so
						 * place it at beginning of the array
						 * and set nfound to 1.
						 */
						*sctes = *cv;
						nfound = 1;
					}
				}
				if (valid)
					break;
				cv++;
			}

			if (!valid)
				return PARSE_BADCOMB;
		}
	} else {
		// neither queue nor group and/or id specified
		return PARSE_SRNOTSPEC;
	}

	// Here we now make the change
	cv = sctes; // start at the beginning of the array
	i = nfound;
	while (i--) {
		/* Must change the correct field based on "type" */
		if (type == LOAD)
			cv->parms.svc_load = c_value;
		else
			cv->parms.priority = static_cast<int>(c_value);
		cv++;
	}

	if (scte_mgr.update(sctes, nfound, update_type) < 0) {
		err_msg = (_("ERROR: Error changing {1}") % name).str(SGLOCALE);
		return PARSE_CERR;
	}
	if (nfound > 1)
		cout << (_("{1} entries changed.\n") % nfound).str(SGLOCALE);
	else
		cout << (_("{1} entry changed.\n") % nfound).str(SGLOCALE);
	return PARSE_SUCCESS;
}

migg_parser::migg_parser(int flags_)
	: cmd_parser(flags_)
{
}

migg_parser::~migg_parser()
{
}

parser_result_t migg_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return PARSE_SUCCESS;
}

migm_parser::migm_parser(int flags_)
	: cmd_parser(flags_)
{
}

migm_parser::~migm_parser()
{
}

parser_result_t migm_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return PARSE_SUCCESS;
}

status_parser::status_parser(int flags_, int type_)
	: cmd_parser(flags_),
	  type(type_)
{
	if (type == SUSPENDED) {
		desc.add_options()
			("opname,o", po::value<string>(&c_svcname)->default_value(d_svcname), (_("suspend specified operation name")).str(SGLOCALE).c_str())
			("qname,q", po::value<string>(&c_qname)->default_value(d_qname), (_("suspend operation on specified qname")).str(SGLOCALE).c_str())
			("pgname,G", po::value<string>(&c_grpname)->default_value(d_grpname), (_("suspend operation on specified pgname")).str(SGLOCALE).c_str())
			("pgid,g", po::value<int>(&c_grpid)->default_value(d_grpid), (_("suspend operation on specified pgid")).str(SGLOCALE).c_str())
			("prcid,i", po::value<int>(&c_svrid)->default_value(d_svrid), (_("suspend operation on specified prcid")).str(SGLOCALE).c_str())
		;
	} else {
		desc.add_options()
			("opname,o", po::value<string>(&c_svcname)->default_value(d_svcname), (_("resume specified operation name")).str(SGLOCALE).c_str())
			("qname,q", po::value<string>(&c_qname)->default_value(d_qname), (_("resume operation on specified qname")).str(SGLOCALE).c_str())
			("pgname,G", po::value<string>(&c_grpname)->default_value(d_grpname), (_("resume operation on specified pgname")).str(SGLOCALE).c_str())
			("pgid,g", po::value<int>(&c_grpid)->default_value(d_grpid), (_("resume operation on specified pgid")).str(SGLOCALE).c_str())
			("prcid,i", po::value<int>(&c_svrid)->default_value(d_svrid), (_("resume operation on specified prcid")).str(SGLOCALE).c_str())
		;
	}
}

status_parser::~status_parser()
{
}

parser_result_t status_parser::parse(int argc, char **argv)
{
	parser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != PARSE_SUCCESS)
		return retval;

	return chg_status();
}

parser_result_t status_parser::chg_status()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_qte& qte_mgr = sg_qte::instance(SGT);
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_scte& scte_mgr = sg_scte::instance(SGT);
	sg_ste_t skey;
	sg_ste_t *cs = NULL;
	sg_qte_t qkey;
	sg_scte_t vkey;
	sg_scte_t *cv = NULL;
	sgid_t sgid;
	int nfound;
	int i;
	int j;
	int nadmin = 0;
	int scope;
	int qnum = 0;
	bool valid;
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);

	scope = S_MACHINE;		/* default mode of retrieval (all) */
	skey.mid() = ALLMID;
	vkey.mid() = ALLMID;

	// Fill in the service name if set
	if (!c_svcname.empty())
		strcpy(vkey.parms.svc_name, c_svcname.c_str());

	if (!c_qname.empty()) {
		/*
		 * Since a queue is given, any changes made are to
		 * be restricted to that queue. First, we retrieve the
		 * queue entry because we need the rlink.
		 */
		strcpy(qkey.saddr(), c_qname.c_str());
		if (qte_mgr.retrieve(S_QUEUE, &qkey, &qkey, &sgid) != 1) {
			if (SGT->_SGT_error_code == SGENOENT)
				return PARSE_BADQ;
			else
				return PARSE_QERR;
		}

		scope = S_QUEUE;
		if (!c_svcname.empty())
			vkey.queue.qlink = qkey.hashlink.rlink;
		else
			skey.queue.qlink = qkey.hashlink.rlink;
	} else if (c_grpid != -1 || c_svrid != -1) {
		if (c_grpid != -1) {
			scope = S_GROUP;
			if (!c_svcname.empty())
				vkey.grpid() = c_grpid;
			else
				skey.grpid() = c_grpid;
		}

		if (c_svrid != -1) {
			if (c_grpid != -1)
				scope = S_GRPID; /* retrieve by group/id */
			else
				scope = S_SVRID; /* retrieve by id only */
			if (!c_svcname.empty())
				vkey.svrid() = c_svrid;
			else
				skey.svrid() = c_svrid;
		}
	} else if (c_svcname.empty()) {
		return PARSE_RESNOTSPEC;
	}

	// Determine the operation to be performed on the status bits
	int op = (type == SUSPENDED) ? STATUS_OP_OR : STATUS_OP_AND;

	/*
	 * If just a queue is set, then since we already have that queue's
	 * tmid, we don't need to do anything else. However, if something
	 * else is given, we must do retrievals to get the sgids for
	 * all the specified resources.
	 */
	if (c_grpid != -1 || c_svrid != -1 || !c_svcname.empty()) {
		// Need memory to place the returned sgids.
		int cnt = (c_svcname.empty() ? SGC->MAXSVRS() : SGC->MAXSVCS());
		boost::shared_array<sgid_t> auto_sgids(new sgid_t[cnt]);
		sgid_t *sgids = auto_sgids.get();
		if (sgids == NULL)
			return PARSE_MALFAIL;

		boost::shared_array<sg_scte_t> auto_sctes;
		sg_scte_t *sctes = NULL;
		boost::shared_array<sg_ste_t> auto_stes;
		sg_ste_t *stes = NULL;

		// Now retrieve the pertinent entries
		if (!c_svcname.empty()) {
			auto_sctes.reset(new sg_scte_t[SGC->MAXSVCS() + 1]);
			sctes = auto_sctes.get();
			if (sctes == NULL)
				return PARSE_MALFAIL;
			nfound = scte_mgr.retrieve(scope, &vkey, sctes, sgids);
		} else {
			auto_stes.reset(new sg_ste_t[SGC->MAXSVRS() + 1]);
			stes = auto_stes.get();
			if (stes == NULL)
				return PARSE_MALFAIL;
			nfound = ste_mgr.retrieve(scope, &skey, stes, sgids);
		}

		if (nfound == -1) {
			if (SGT->_SGT_error_code == SGENOENT) {
				if (!c_svcname.empty())
					return PARSE_BADSC;
				else
					return PARSE_BADSR;
			} else {
				if (!c_svcname.empty())
					return PARSE_SCERR;
				else
					return PARSE_SRERR;
			}
		}

		/*
		 * If a queue was also given, we must check whether the
		 * default group or id settings are valid (ie, they exist on
		 * this queue.)
		 */
		if (!c_qname.empty()) {
			if (!c_svcname.empty())
				cv = sctes;
			else
				cs = stes;

			/*
			 * If a default group exists, we do not need to go
			 * through the entire array to check it, since all
			 * entries on that queue MUST be in the same group.
			 * Therefore, it suffices to simply check the first
			 * entry in the array. Note that "valid" is set
			 * to TRUE when it is declared above.
			 */
			if (c_grpid != -1) {
				if (c_grpid != ((!c_svcname.empty()) ? cv->grpid() : cs->svrid()))
					valid = false;
			}

			/*
			 * If the test above succeeded, we now must check
			 * whether the default id, if set, is OK. Now we
			 * simply go through the array until we find a
			 * match.
			 */
			if (valid && c_svrid != -1) {
				valid = false;
				for (i = 0; i < nfound; i++) {
					if (!c_svcname.empty()) {
						if (cv->svrid() == c_svrid) {
							valid = true;
							break;
						}
						cv++;
					} else {
						if (cs->svrid() == c_svrid) {
							valid = true;
							break;
						}
						cs++;
					}
				}
			}

			if (!valid)
				return PARSE_BADCOMB;	/* values didn't match */

			/*
			 * Since changes are made across a queue, if a queue
			 * is set, we only need to affect one resource on
			 * that queue, in which case all others will be
			 * affected also. This is easily accomplished by
			 * setting nfound to 1.
			 */

			 nfound = 1;
		}

		/*
		 * Now we can go through and change each element in the
		 * array. While we do this, we also want to
		 * count the number of distinct queues this command would
		 * affect. The array qlist holds the qlinks currently found.
		 * Note that if nfound is 1, then there is no need to
		 * deal with the qlist stuff. However, to make the code
		 * more readable and less cumbersome, this code is
		 * executed even if nfound is one.
		 */
		boost::shared_array<int> auto_qlist(new int[nfound]);
		int *qlist = auto_qlist.get();
		if (qlist == NULL)
			return PARSE_MALFAIL;

		if (!c_svcname.empty())
			cv = sctes;
		else
			cs = stes;

		sgid_t *csgid = sgids;		/* need to save tmem to free it later */

		i = nfound;
		while (i--) {
			sg_proc_t& proc = (!c_svcname.empty()) ? cv->svrproc() : cs->svrproc();
			if (proc.admin_grp()) {
				nadmin++;

				if (!c_svcname.empty())
					cv++;
				else
					cs++;
				csgid++;
				continue;
			}
			/*
			 * When a server doesn't provide any service (for example, WSL), _tmsetstat() will fail, and
			 * the following servers won't have chance to be SUSPENDED/RESUMED. So, there must be a way
			 * to set a global variable inside _tmsetstat() to indicate such an error so that status()
			 * will continue to set the status of other servers. Here TPERRNO & TPERRORDETAIL is
			 * used by value of TPENOENT & (TPED_MAXVAL + 1), they work together to indicate such an
			 * error. The code is ugly, but I can't find a perfect solution.
			 */
			int save_errdetail = SGT->_SGT_error_detail;
			SGT->_SGT_error_detail = SGED_MAXVAL;
			if (!rpc_mgr.set_stat(*csgid, type, op)
				&& (SGT->_SGT_error_code != SGENOENT || SGT->_SGT_error_detail == SGED_MAXVAL + 1)) {
				SGT->_SGT_error_detail = save_errdetail;
				if (type == SUSPENDED)
					return PARSE_ERRSUSP;
				else
					return PARSE_ERRRES;
			}
			SGT->_SGT_error_detail = save_errdetail;

			// queue number of the current entry
			int currq = (!c_svcname.empty()) ? cv->queue.qlink : cs->queue.qlink;

			/*
			 * We need to determine whether this queue has
			 * already been accounted for.
			 */
			valid = false;
			for (j = 0; j < qnum; j++) {
				if (qlist[j] == currq) {
					valid = true;
					break;
				}
			}
			if (!valid) {
				// doesn't exist in qlist, so add it
				qlist[qnum++] = currq;
			}

			if (!c_svcname.empty())
				cv++;
			else
				cs++;
			csgid++;
		}

		if (!c_svcname.empty()) {
			if (qnum > 1)
				cout << (_("Operations {1} {2} on {3} queues.\n")% c_svcname % cmd_desc % qnum).str(SGLOCALE);
			else
				cout << (_("Operation {1} {2} on {3} queue.\n")% c_svcname % cmd_desc % qnum).str(SGLOCALE);
			return PARSE_SUCCESS;
		}
	} else {
		/*
		 * In this case, just a queue was given, so we can
		 * just call _tmsetstat using the sgid we have.
		 */
		if (!rpc_mgr.set_stat(sgid, type, op)) {
			if (type == SUSPENDED)
				return PARSE_ERRSUSP;
			else
				return PARSE_ERRRES;
		}

		qnum = 1; // only one queue was changed
	}

	if (nadmin) {
		if (qnum != 1)
			cout << (_("All non-admin operations on {1} queues have been {2}.") % qnum % cmd_desc).str(SGLOCALE);
		else
			cout << (_("All non-admin operations on {1} queue have been {2}.") % qnum % cmd_desc).str(SGLOCALE);
		cout << "\n";

		if (nadmin > 1)
			cout << nadmin << (_(" admin operations have not been {1}.") % cmd_desc).str(SGLOCALE);
		else
			cout << nadmin << (_(" admin operation has not been {1}.") % cmd_desc).str(SGLOCALE);
	} else {
		if (qnum != 1)
			cout << (_("All operations on {1} queues have been {2}.")% qnum % cmd_desc).str(SGLOCALE);
		else
			cout << (_("All operations on {1} queue have been {2}.")% qnum % cmd_desc).str(SGLOCALE);
	}
	cout << "\n";

	return PARSE_SUCCESS;
}

}
}

