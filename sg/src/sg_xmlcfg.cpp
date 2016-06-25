#include "sg_internal.h"

namespace ai
{
namespace sg
{

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

sg_xmlcfg::sg_xmlcfg(const string& xml_file_)
{
	SGP = sgp_ctx::instance();
	xml_file = xml_file_;
}

sg_xmlcfg::~sg_xmlcfg()
{
}

void sg_xmlcfg::load(bool enable_hidden)
{
	bpt::iptree pt;

	bpt::read_xml(xml_file, pt);

	load_bbparms(pt, enable_hidden);
	load_mchents(pt);
	load_netents(pt);
	load_sgents(pt);
	load_svrents(pt);
	load_svcents(pt);
}

void sg_xmlcfg::save(bool enable_hidden)
{
	bpt::iptree pt;

	save_bbparms(pt, enable_hidden);
	save_mchents(pt);
	save_netents(pt);
	save_sgents(pt);
	save_svrents(pt);
	save_svcents(pt);
	bpt::write_xml(xml_file, pt, std::locale(), bpt::xml_writer_settings<char>('\t', 1));
}

void sg_xmlcfg::load_bbparms(const bpt::iptree& pt, bool enable_hidden)
{
	gpenv& env_mgr = gpenv::instance();
	boost::char_separator<char> sep(" \t\b");
	int i;

	try {
		memset(&bbparms, '\0', sizeof(sg_bbparms_t));

		bbparms.magic = VERSION;
		bbparms.bbrelease = SGRELEASE;
		int current = time(0);

		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("servicegalaxy")) {
			if (v.first != "cluster")
				continue;

			const bpt::iptree& v_pt = v.second;

			if (enable_hidden) {
				bbparms.bbversion = get_value(v_pt, "bbversion", current);
				bbparms.sgversion = get_value(v_pt, "sgversion", current);
			} else {
				bbparms.bbversion = current;
				bbparms.sgversion = current;
			}

			string clstid;
			env_mgr.expand_var(clstid, v_pt.get<string>("clstid"));
			try {
				bbparms.ipckey = boost::lexical_cast<int>(clstid);
			} catch (exception& ex) {
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.clstid invalid, {1}.") % ex.what()).str(SGLOCALE));
			}
			if (bbparms.ipckey <= 0 || bbparms.ipckey >= (1 << MCHIDSHIFT))
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.clstid is invalid, should be positive but less than {1}.") % (1 << MCHIDSHIFT)).str(SGLOCALE));

			bbparms.uid = get_value(v_pt, "uid", ::getuid());
			bbparms.gid = get_value(v_pt, "gid", ::getgid());

			parse_perm(get_value(v_pt, "perm", string("")), bbparms.perm, "cluster.perm");

			bbparms.options = SGLAN;	// default to LAN mode
			bbparms.ldbal = DFLT_LDBAL;

			string options = get_value(v_pt, "options", string(""));
			tokenizer options_tokens(options, sep);
			for (tokenizer::iterator iter = options_tokens.begin(); iter != options_tokens.end(); ++iter) {
				string str = *iter;

				if (strcasecmp(str.c_str(), "LAN") == 0)
					bbparms.options |= SGLAN;
				else if (strcasecmp(str.c_str(), "MIGALLOWED") == 0)
					bbparms.options |= SGMIGALLOWED;
				else if (strcasecmp(str.c_str(), "HA") == 0)
					bbparms.options |= SGHA;
				else if (strcasecmp(str.c_str(), "ACCSTATS") == 0)
					bbparms.options |= SGACCSTATS;
				else if (strcasecmp(str.c_str(), "CLEXITL") == 0)
					bbparms.options |= SGCLEXITL;
				else if (strcasecmp(str.c_str(), "AA") == 0)
					bbparms.options |= SGAA;
				else if (strcasecmp(str.c_str(), "RR") == 0)
					bbparms.ldbal = LDBAL_RR;
				else if (strcasecmp(str.c_str(), "RT") == 0)
					bbparms.ldbal = LDBAL_RT;
			}

			i = 0;
			string master = v_pt.get<string>("master");
			tokenizer master_tokens(master, sep);
			for (tokenizer::iterator iter = master_tokens.begin(); iter != master_tokens.end(); ++iter) {
				string str = *iter;

				if (str.length() > MAX_IDENT)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.master too long for master node.")).str(SGLOCALE));

				strcpy(bbparms.master[i++], str.c_str());
				if (i > MAX_MASTERS)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.master too many masters.")).str(SGLOCALE));
			}
			bbparms.current = get_value(v_pt, "curid", 0);
			if (bbparms.current < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.curid must be zero or positive.")).str(SGLOCALE));

			bbparms.maxpes = get_value(v_pt, "maxnodes", DFLT_MAXNODES);
			if (bbparms.maxpes <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.maxnodes must be positive.")).str(SGLOCALE));

			bbparms.maxnodes = get_value(v_pt, "maxnets", DFLT_MAXNETS);
			if (bbparms.maxnodes <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.maxnets must be positive.")).str(SGLOCALE));

			bbparms.maxaccsrs = get_value(v_pt, "maxclts", DFLT_MAXCLTS);
			if (bbparms.maxaccsrs <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.maxclts must be positive.")).str(SGLOCALE));

			bbparms.maxques = get_value(v_pt, "maxques", DFLT_MAXQUES);
			if (bbparms.maxques <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.maxques must be positive.")).str(SGLOCALE));

			bbparms.maxsgt = get_value(v_pt, "maxpgs", DFLT_MAXPGS);
			if (bbparms.maxsgt <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.maxpgs must be positive.")).str(SGLOCALE));

			bbparms.maxsvrs = get_value(v_pt, "maxprocs", DFLT_MAXPROCS);
			if (bbparms.maxsvrs <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.maxprocs must be positive.")).str(SGLOCALE));

			bbparms.maxsvcs = get_value(v_pt, "maxops", DFLT_MAXOPS);
			if (bbparms.maxsvcs <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.maxops must be positive.")).str(SGLOCALE));

			bbparms.quebkts = get_value(v_pt, "quebkts", DFLT_QUEBKTS);
			if (bbparms.quebkts <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.quebkts must be positive.")).str(SGLOCALE));

			bbparms.sgtbkts = get_value(v_pt, "pgbkts", DFLT_PGBKTS);
			if (bbparms.sgtbkts <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.pgbkts must be positive.")).str(SGLOCALE));

			bbparms.svrbkts = get_value(v_pt, "procbkts", DFLT_PROCBKTS);
			if (bbparms.svrbkts <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.procbkts must be positive.")).str(SGLOCALE));

			bbparms.svcbkts = get_value(v_pt, "opbkts", DFLT_OPBKTS);
			if (bbparms.svcbkts <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.opbkts must be positive.")).str(SGLOCALE));

			bbparms.scan_unit = get_value(v_pt, "polltime", DFLT_POLLTIME);
			if (bbparms.scan_unit < 6)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.polltime must be at least 6.")).str(SGLOCALE));

			bbparms.sanity_scan = get_value(v_pt, "robustint", DFLT_ROBUSTINT);
			if (bbparms.sanity_scan <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.robustint must be positive.")).str(SGLOCALE));

			bbparms.stack_size = get_value(v_pt, "stacksize", DFLT_STACKSIZE);
			if (bbparms.stack_size <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.stacksize must be positive.")).str(SGLOCALE));

			bbparms.max_num_msg = get_value(v_pt, "msgmnb", DFLT_MSGMNB);
			if (bbparms.max_num_msg <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.msgmnb must be positive.")).str(SGLOCALE));

			bbparms.max_msg_size = get_value(v_pt, "msgmax", DFLT_MSGMAX);
			if (bbparms.max_msg_size <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.msgmax must be positive.")).str(SGLOCALE));

			bbparms.cmprs_limit = get_value(v_pt, "cmprslimit", DFLT_CMPRSLIMIT);
			if (bbparms.cmprs_limit < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.cmprslimit must be zero or positive.")).str(SGLOCALE));

			bbparms.remedy = get_value(v_pt, "costfix", DFLT_COSTFIX);
			if (bbparms.remedy < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.costfix must be zero or positive.")).str(SGLOCALE));

			bbparms.max_open_queues = get_value(v_pt, "nfiles", DFLT_NFILES);
			if (bbparms.max_open_queues <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.nfiles must be positive.")).str(SGLOCALE));

			bbparms.block_time = get_value(v_pt, "blktime", DFLT_BLKTIME);
			if (bbparms.block_time <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.blktime must be positive.")).str(SGLOCALE));

			bbparms.dbbm_wait = get_value(v_pt, "clstwait", DFLT_CLSTWAIT);
			if (bbparms.dbbm_wait <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.clstwait must be positive.")).str(SGLOCALE));

			bbparms.bbm_query = get_value(v_pt, "mngrquery", DFLT_MNGRQUERY);
			if (bbparms.bbm_query < bbparms.sanity_scan)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.mngrquery must be no less than robustint.")).str(SGLOCALE));

			bbparms.init_wait = get_value(v_pt, "initwait", DFLT_INITWAIT);
			if (bbparms.init_wait <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: cluster.initwait must be positive.")).str(SGLOCALE));
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section cluster failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section cluster failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

void sg_xmlcfg::load_mchents(const bpt::iptree& pt)
{
	gpenv& env_mgr = gpenv::instance();

	try {
		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("servicegalaxy.nodes")) {
			if (v.first != "node")
				continue;

			const bpt::iptree& v_pt = v.second;
			sg_mchent_t item;
			memset(&item, '\0', sizeof(sg_mchent_t));

			string hostname = v_pt.get<string>("hostname");
			if (hostname.length() > MAX_IDENT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.hostname is too long.")).str(SGLOCALE));
			strcpy(item.pmid, hostname.c_str());

			string hostid = v_pt.get<string>("hostid");
			if (hostid.length() > MAX_IDENT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.hostid is too long.")).str(SGLOCALE));
			strcpy(item.lmid, hostid.c_str());

			string sgroot;
			env_mgr.expand_var(sgroot, v_pt.get<string>("sgroot"));
			if (sgroot.length() > MAX_SGROOT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.sgroot is too long.")).str(SGLOCALE));
			strcpy(item.sgdir, sgroot.c_str());

			string approot;
			env_mgr.expand_var(approot, v_pt.get<string>("approot"));
			if (approot.length() > MAX_APPROOT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.approot is too long.")).str(SGLOCALE));
			strcpy(item.appdir, approot.c_str());

			string sgprofile;
			env_mgr.expand_var(sgprofile, v_pt.get<string>("sgprofile"));
			if (sgprofile.length() > MAX_SGCONFIG)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.sgprofile is too long.")).str(SGLOCALE));
			strcpy(item.sgconfig, sgprofile.c_str());

			string args;
			env_mgr.expand_var(args, get_value(v_pt, "args", string("")));
			if (args.length() > MAX_LSTRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.args is too long.")).str(SGLOCALE));
			strcpy(item.clopt, args.c_str());

			string profile;
			env_mgr.expand_var(profile, get_value(v_pt, "profile", string("")));
			if (profile.length() > MAX_LSTRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.profile is too long.")).str(SGLOCALE));
			strcpy(item.envfile, profile.c_str());

			string logname;
			env_mgr.expand_var(logname, get_value(v_pt, "logname", string("")));
			if (logname.length() > MAX_LSTRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.logname is too long.")).str(SGLOCALE));
			strcpy(item.ulogpfx, logname.c_str());

			item.maxaccsrs = get_value(v_pt, "maxclts", 0);
			if (item.maxaccsrs < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.maxclts must be zero or positive.")).str(SGLOCALE));

			parse_perm(get_value(v_pt, "perm", string("")), item.perm, "nodes.node.perm");

			item.uid = get_value(v_pt, "uid", 0);
			if (item.uid < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.uid must be zero or positive.")).str(SGLOCALE));

			item.gid = get_value(v_pt, "gid", 0);
			if (item.gid < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.gid must be zero or positive.")).str(SGLOCALE));

			item.netload = get_value(v_pt, "netcost", DFLT_NETCOST);
			if (item.netload <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.netcost must be positive.")).str(SGLOCALE));

			item.maxconn = get_value(v_pt, "maxconn", DFLT_MAXCONN);
			if (item.maxconn <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: nodes.node.maxconn must be positive.")).str(SGLOCALE));

			BOOST_FOREACH(const sg_mchent_t& v, mchents) {
				if (strcmp(v.pmid, item.pmid) == 0
					|| strcmp(v.lmid, item.lmid) == 0)
					throw sg_exception(__FILE__, __LINE__, SGEPROTO, 0, (_("ERROR: duplicate node entry.")).str(SGLOCALE));
			}

			mchents.push_back(item);
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section nodes failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section nodes failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

void sg_xmlcfg::load_netents(const bpt::iptree& pt)
{
	gpenv& env_mgr = gpenv::instance();

	try {
		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("servicegalaxy.networks")) {
			if (v.first != "network")
				continue;

			const bpt::iptree& v_pt = v.second;
			sg_netent_t item;
			memset(&item, '\0', sizeof(sg_netent_t));

			string hostid = v_pt.get<string>("hostid");
			if (hostid.length() > MAX_IDENT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: networks.network.hostid is too long.")).str(SGLOCALE));
			strcpy(item.lmid, hostid.c_str());

			string gwsaddr;
			env_mgr.expand_var(gwsaddr, get_value(v_pt, "gwsaddr", string("")));
			if (gwsaddr.length() > MAX_STRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: networks.network.gwsaddr is too long.")).str(SGLOCALE));
			validate_address("networks.network.gwsaddr", gwsaddr);
			strcpy(item.naddr, gwsaddr.c_str());

			string agtaddr;
			env_mgr.expand_var(agtaddr, get_value(v_pt, "agtaddr", string("")));
			if (agtaddr.length() > MAX_STRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: networks.network.agtaddr is too long.")).str(SGLOCALE));
			validate_address("networks.network.agtaddr", agtaddr);
			strcpy(item.nlsaddr, agtaddr.c_str());

			string netname = get_value(v_pt, "netname", string(""));
			if (netname.empty())
				netname = NM_DEFAULTNET;
			if (netname.length() > MAX_IDENT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: networks.network.netname is too long.")).str(SGLOCALE));
			strcpy(item.netgroup, netname.c_str());

			string tcpkeepalive = get_value(v_pt, "tcpkeepalive", string("Y"));
			if (strcasecmp(tcpkeepalive.c_str(), "Y") == 0)
				item.tcpkeepalive = true;
			else if (strcasecmp(tcpkeepalive.c_str(), "N") == 0)
				item.tcpkeepalive = false;
			else
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: networks.network.tcpkeepalive value invalid.")).str(SGLOCALE));

			string nodelay = get_value(v_pt, "nodelay", string("N"));
			if (strcasecmp(nodelay.c_str(), "Y") == 0)
				item.nodelay = true;
			else if (strcasecmp(nodelay.c_str(), "N") == 0)
				item.nodelay = false;
			else
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: networks.network.nodelay value invalid.")).str(SGLOCALE));

			BOOST_FOREACH(const sg_netent_t& netent, netents) {
				if (hostid == netent.lmid && netname == netent.netgroup)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: networks.network.netgroup must be unique on same node.")).str(SGLOCALE));
			}

			netents.push_back(item);
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section networks failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section networks failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

void sg_xmlcfg::load_sgents(const bpt::iptree& pt)
{
	boost::char_separator<char> sep(" \t\b");
	gpenv& env_mgr = gpenv::instance();
	int i;

	try {
		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("servicegalaxy.pgroups")) {
			if (v.first != "pgroup")
				continue;


			const bpt::iptree& v_pt = v.second;
			sg_sgent_t item;
			memset(&item, '\0', sizeof(sg_sgent_t));

			string pgname = v_pt.get<string>("pgname");
			if (pgname.length() > MAX_IDENT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: pgroups.pgroup.pgname is too long.")).str(SGLOCALE));
			strcpy(item.sgname, pgname.c_str());

			item.curmid = get_value(v_pt, "curid", 0);
			if (item.curmid < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: pgroups.pgroup.curid must be zero or positive.")).str(SGLOCALE));

			i = 0;
			string hostid = v_pt.get<string>("hostid");
			tokenizer tokens(hostid, sep);
			for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter) {
				string str = *iter;

				if (str.length() > MAX_IDENT)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: pgroups.pgroup.hostid too long for master node.")).str(SGLOCALE));

				strcpy(item.lmid[i++], str.c_str());
				if (i > MAXLMIDSINLIST)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: pgroups.pgroup.hostid too many hostids.")).str(SGLOCALE));
			}

			item.grpid = v_pt.get<int>("pgid");
			if (item.grpid < 0 || item.grpid >= DFLT_PGID)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: pgroups.pgroup.pgid value must be between 0 and {1}") % DFLT_PGID).str(SGLOCALE));

			BOOST_FOREACH(const sg_sgent_t& v, sgents) {
				if (v.grpid == item.grpid)
					throw sg_exception(__FILE__, __LINE__, SGEPROTO, 0, (_("ERROR: duplicate process group.")).str(SGLOCALE));
			}

			string profile;
			env_mgr.expand_var(profile, get_value(v_pt, "profile", string("")));
			if (profile.length() > MAX_LSTRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: pgroups.pgroup.profile is too long.")).str(SGLOCALE));
			strcpy(item.envfile, profile.c_str());

			sgents.push_back(item);
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section pgroups failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section pgroups failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

void sg_xmlcfg::load_svrents(const bpt::iptree& pt)
{
	boost::char_separator<char> sep(" \t\b");
	gpenv& env_mgr = gpenv::instance();

	try {
		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("servicegalaxy.processes")) {
			if (v.first != "process")
				continue;

			const bpt::iptree& v_pt = v.second;
			sg_svrent_t item;
			memset(&item, '\0', sizeof(sg_svrent_t));

			item.sparms.rqparms.options = RESTARTABLE;
			item.bparms.flags = 0;

			string options = get_value(v_pt, "options", string(""));
			tokenizer options_tokens(options, sep);
			for (tokenizer::iterator iter = options_tokens.begin(); iter != options_tokens.end(); ++iter) {
				string str = *iter;

				if (strcasecmp(str.c_str(), "NONRESTART ") == 0)
					item.sparms.rqparms.options &= ~RESTARTABLE;
				else if (strcasecmp(str.c_str(), "REPLYQ") == 0)
					item.bparms.flags |= BF_REPLYQ;
				else if (strcasecmp(str.c_str(), "PRESERVE_FD") == 0)
					item.bparms.flags |= BF_PRESERVE_FD;
			}

			item.sparms.rqparms.grace = get_value(v_pt, "rstint", DFLT_RSTINT);
			if (item.sparms.rqparms.grace <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.rstint must be positive.")).str(SGLOCALE));

			item.sparms.rqparms.maxgen = get_value(v_pt, "maxrst", std::numeric_limits<int>::max());
			if (item.sparms.rqparms.maxgen <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.maxrst must be positive.")).str(SGLOCALE));

			string lqname = get_value(v_pt, "lqname", string(""));
			if (lqname.length() > MAX_IDENT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.lqname is too long.")).str(SGLOCALE));
			strcpy(item.sparms.rqparms.saddr, lqname.c_str());

			string exitcmd = get_value(v_pt, "exitcmd", string(""));
			if (exitcmd.length() > MAX_LSTRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.exitcmd is too long.")).str(SGLOCALE));
			strcpy(item.sparms.rqparms.rcmd, exitcmd.c_str());

			string procname = v_pt.get<string>("procname");
			if (procname.empty() || procname.length() > MAX_LSTRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.procname is empty or too long.")).str(SGLOCALE));
			strcpy(item.sparms.rqparms.filename, procname.c_str());

			item.sparms.svrproc.svrid = v_pt.get<int>("prcid");
			if (item.sparms.svrproc.svrid <= MAX_RESERVED_PRCID)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.prcid must be no less than {1}.") % MAX_RESERVED_PRCID).str(SGLOCALE));

			parse_perm(get_value(v_pt, "rqperm", string("")), item.sparms.rqperm, "processes.process.rqperm");
			parse_perm(get_value(v_pt, "rpperm", string("")), item.sparms.rpperm, "processes.process.rpperm");

			item.sparms.sequence = get_value(v_pt, "priority", 0);
			if (item.sparms.sequence < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.priority must be zero or positive.")).str(SGLOCALE));

			item.bparms.bootmin = get_value(v_pt, "min", 1);
			if (item.bparms.bootmin < 0 || item.bparms.bootmin > 1000)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.min must be between 0 and 1000.")).str(SGLOCALE));

			item.sparms.svridmax = get_value(v_pt, "max", 1);
			if (item.sparms.svridmax < 1 || item.sparms.svridmax > 1000)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.max must be between 1 and 1000.")).str(SGLOCALE));

			if (item.bparms.bootmin > item.sparms.svridmax)
				item.sparms.svridmax = item.sparms.svrproc.svrid + item.bparms.bootmin - 1;
			else
				item.sparms.svridmax += item.sparms.svrproc.svrid - 1;

			item.sparms.max_num_msg = get_value(v_pt, "msgmnb", 0);
			if (item.sparms.max_num_msg < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.msgmnb must be zero or positive.")).str(SGLOCALE));

			item.sparms.max_msg_size = get_value(v_pt, "msgmax", 0);
			if (item.sparms.max_msg_size < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.msgmax must be zero or positive.")).str(SGLOCALE));

			item.sparms.cmprs_limit = get_value(v_pt, "cmprslimit", 0);
			if (item.sparms.cmprs_limit < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.cmprslimit must be zero or positive.")).str(SGLOCALE));

			item.sparms.remedy = get_value(v_pt, "costfix", 0);
			if (item.sparms.remedy < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.costfix must be zero or positive.")).str(SGLOCALE));

			string pgname = get_value(v_pt, "pgname", string(""));
			if (pgname.length() > MAX_IDENT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.pgname is too long.")).str(SGLOCALE));
			strcpy(item.sparms.sgname, pgname.c_str());

			if (item.sparms.sgname[0] == '\0') {
				item.sparms.svrproc.grpid = DFLT_PGID;
			} else {
				bool found = false;
				BOOST_FOREACH(const sg_sgent_t& v, sgents) {
					if (v.sgname == pgname) {
						item.sparms.svrproc.grpid = v.grpid;
						found = true;
					}
				}
				if (!found)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: Can't find process group for {1}") % pgname).str(SGLOCALE));
			}

			// TGWS需要独立的应答队列
			if (strcmp(item.sparms.rqparms.filename, "sgtgws") == 0)
				SGP->_SGP_boot_flags |= BF_REPLYQ;

			string args;
			env_mgr.expand_var(args, get_value(v_pt, "args", string("-A")));
			if (args.length() > MAX_LSTRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.args is too long.")).str(SGLOCALE));
			strcpy(item.bparms.clopt, args.c_str());

			string profile;
			env_mgr.expand_var(profile, get_value(v_pt, "profile", string("")));
			if (profile.length() > MAX_LSTRING)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: processes.process.profile is too long.")).str(SGLOCALE));
			strcpy(item.bparms.envfile, profile.c_str());

			BOOST_FOREACH(const sg_svrent_t& v, svrents) {
				if (v.sparms.svrproc.grpid == item.sparms.svrproc.grpid
					&& ((v.sparms.svrproc.svrid <= item.sparms.svrproc.svrid
						&& v.sparms.svridmax >= item.sparms.svrproc.svrid)
						|| (v.sparms.svrproc.svrid <= item.sparms.svridmax
						&& v.sparms.svridmax >= item.sparms.svridmax)))
					throw sg_exception(__FILE__, __LINE__, SGEPROTO, 0, (_("ERROR: duplicate process entry.")).str(SGLOCALE));
			}

			svrents.push_back(item);
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section processes failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section processes failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

void sg_xmlcfg::load_svcents(const bpt::iptree& pt)
{
	boost::char_separator<char> sep(" \t\b");

	try {
		boost::optional<const bpt::iptree&> node = pt.get_child_optional("servicegalaxy.operations");
		if (!node)
			return;

		BOOST_FOREACH(const bpt::iptree::value_type& v, *node) {
			if (v.first != "operation")
				continue;

			const bpt::iptree& v_pt = v.second;
			sg_svcent_t item;
			memset(&item, '\0', sizeof(sg_svcent_t));

			string pattern = v_pt.get<string>("pattern");
			if (pattern.length() > MAX_PATTERN)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: operations.operation.pattern is too long.")).str(SGLOCALE));
			strcpy(item.svc_pattern, pattern.c_str());

			string pgname = get_value(v_pt, "pgname", string(""));
			if (pgname.length() > MAX_IDENT)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: operations.operation.pgname is too long.")).str(SGLOCALE));

			if (pgname.empty()) {
				item.grpid = DFLT_PGID;
			} else {
				bool found = false;
				BOOST_FOREACH(const sg_sgent_t& v, sgents) {
					if (pgname == v.sgname) {
						item.grpid = v.grpid;
						found = true;
					}
				}
				if (!found)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: Can't find process group for {1}") % pgname).str(SGLOCALE));
			}

			item.priority = get_value(v_pt, "priority", DFLT_PRIORITY);
			if (item.priority < 0 || item.priority > 100)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: operations.operation.priority must be between 0 and 100.")).str(SGLOCALE));

			item.svc_policy = 0;
			string policy = get_value(v_pt, "policy", string(""));
			tokenizer policy_tokens(policy, sep);
			for (tokenizer::iterator iter = policy_tokens.begin(); iter != policy_tokens.end(); ++iter) {
				string str = *iter;

				if (strcasecmp(str.c_str(), "LF") == 0)
					item.svc_policy |= POLICY_LF;
				else if (strcasecmp(str.c_str(), "RR") == 0)
					item.svc_policy |= POLICY_RR;
				else if (strcasecmp(str.c_str(), "IF") == 0)
					item.svc_policy |= POLICY_IF;
			}

			if (item.svc_policy == 0)
				item.svc_policy = DFLT_POLICY;

			if ((item.svc_policy & POLICY_LF) && (item.svc_policy & POLICY_RR))
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: operations.operation.policy value invalid, LF and RR are conflict.")).str(SGLOCALE));

			item.svc_load = get_value(v_pt, "cost", DFLT_COST);
			if (item.svc_load <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: operations.operation.cost must be positive.")).str(SGLOCALE));

			item.load_limit = get_value(v_pt, "costlimit", DFLT_COSTLIMIT);
			if (item.load_limit < 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: operations.operation.costlimit must be zero or positive.")).str(SGLOCALE));

			item.svctimeout = get_value(v_pt, "timeout", DFLT_TIMEOUT);
			if (item.svctimeout <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: operations.operation.timeout must be positive.")).str(SGLOCALE));

			item.svcblktime = get_value(v_pt, "blktime", DFLT_BLKTIME);
			if (item.svcblktime <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: operations.operation.blktime must be positive.")).str(SGLOCALE));

			svcents.push_back(item);
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section operations failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section operations failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

void sg_xmlcfg::save_bbparms(bpt::iptree& pt, bool enable_hidden)
{
	char buf[32];
	bpt::iptree v_pt;

	if (enable_hidden) {
		v_pt.put("bbversion", bbparms.bbversion);
		v_pt.put("sgversion", bbparms.sgversion);
	}

	v_pt.put("clstid", bbparms.ipckey);
	if (bbparms.uid != ::getuid())
		v_pt.put("uid", bbparms.uid);
	if (bbparms.gid != ::getgid())
		v_pt.put("gid", bbparms.gid);

	if (bbparms.perm != DFLT_PERM) {
		sprintf(buf, "0%o", bbparms.perm);
		v_pt.put("perm", buf);
	}

	ostringstream fmt;
	fmt << sg_resource_options_t(bbparms.options);
	string options = fmt.str();
	if (!options.empty())
		options += ",";
	if (bbparms.ldbal == LDBAL_RR)
		options += "RR";
	else
		options += "RT";

	v_pt.put("options", options);

	string master;
	for (int i = 0; i < MAX_MASTERS; i++) {
		if (bbparms.master[i][0] == '\0')
			break;
		if (!master.empty())
			master += ',';
		master += bbparms.master[i];
	}
	v_pt.put("master", master);
	if (bbparms.current != 0)
		v_pt.put("curid", bbparms.current);
	if (bbparms.maxpes != DFLT_MAXNODES)
		v_pt.put("maxnodes", bbparms.maxpes);
	if (bbparms.maxnodes != DFLT_MAXNETS)
		v_pt.put("maxnets", bbparms.maxnodes);
	if (bbparms.maxaccsrs != DFLT_MAXCLTS)
		v_pt.put("maxclts", bbparms.maxaccsrs);
	if (bbparms.maxques != DFLT_MAXQUES)
		v_pt.put("maxques", bbparms.maxques);
	if (bbparms.maxsgt != DFLT_MAXPGS)
		v_pt.put("maxpgs", bbparms.maxsgt);
	if (bbparms.maxsvrs != DFLT_MAXPROCS)
		v_pt.put("maxprocs", bbparms.maxsvrs);
	if (bbparms.maxsvcs != DFLT_MAXOPS)
		v_pt.put("maxops", bbparms.maxsvcs);
	if (bbparms.quebkts != DFLT_QUEBKTS)
		v_pt.put("quebkts", bbparms.quebkts);
	if (bbparms.sgtbkts != DFLT_PGBKTS)
		v_pt.put("pgbkts", bbparms.sgtbkts);
	if (bbparms.svrbkts != DFLT_PROCBKTS)
		v_pt.put("procbkts", bbparms.svrbkts);
	if (bbparms.svcbkts != DFLT_OPBKTS)
		v_pt.put("opbkts", bbparms.svcbkts);
	if (bbparms.scan_unit != DFLT_POLLTIME)
		v_pt.put("polltime", bbparms.scan_unit);
	if (bbparms.sanity_scan != DFLT_ROBUSTINT)
		v_pt.put("robustint", bbparms.sanity_scan);
	if (bbparms.stack_size != DFLT_STACKSIZE)
		v_pt.put("stacksize", bbparms.stack_size);
	if (bbparms.max_num_msg != DFLT_MSGMNB)
		v_pt.put("msgmnb", bbparms.max_num_msg);
	if (bbparms.max_msg_size != DFLT_MSGMAX)
		v_pt.put("msgmax", bbparms.max_msg_size);
	if (bbparms.cmprs_limit != DFLT_CMPRSLIMIT)
		v_pt.put("cmprslimit", bbparms.cmprs_limit);
	if (bbparms.remedy != DFLT_COSTFIX)
		v_pt.put("costfix", bbparms.remedy);
	if (bbparms.max_open_queues != DFLT_NFILES)
		v_pt.put("nfiles", bbparms.max_open_queues);
	if (bbparms.block_time != DFLT_BLKTIME)
		v_pt.put("blktime", bbparms.block_time);
	if (bbparms.dbbm_wait != DFLT_CLSTWAIT)
		v_pt.put("clstwait", bbparms.dbbm_wait);
	if (bbparms.bbm_query != DFLT_MNGRQUERY)
		v_pt.put("mngrquery", bbparms.bbm_query);
	if (bbparms.init_wait != DFLT_INITWAIT)
		v_pt.put("initwait", bbparms.init_wait);

	pt.add_child("servicegalaxy.cluster", v_pt);
}

void sg_xmlcfg::save_mchents(bpt::iptree& pt)
{
	char buf[32];

	BOOST_FOREACH(const sg_mchent_t& v, mchents) {
		bpt::iptree v_pt;

		v_pt.put("hostname", v.pmid);
		v_pt.put("hostid", v.lmid);
		v_pt.put("sgroot", v.sgdir);
		v_pt.put("approot", v.appdir);
		v_pt.put("sgprofile", v.sgconfig);
		if (v.clopt[0] != '\0')
			v_pt.put("args", v.clopt);
		if (v.envfile[0] != '\0')
			v_pt.put("profile", v.envfile);
		if (v.ulogpfx[0] != '\0')
			v_pt.put("logname", v.ulogpfx);
		if (v.maxaccsrs != bbparms.maxaccsrs && v.maxaccsrs != 0)
			v_pt.put("maxclts", v.maxaccsrs);
		if (v.perm != bbparms.perm && v.perm != 0) {
			sprintf(buf, "0%o", v.perm);
			v_pt.put("perm", buf);
		}
		if (v.uid != bbparms.uid && v.uid != 0)
			v_pt.put("uid", v.uid);
		if (v.gid != bbparms.gid && v.gid != 0)
			v_pt.put("gid", v.gid);
		if (v.netload != DFLT_NETCOST)
			v_pt.put("netcost", v.netload);
		if (v.maxconn != DFLT_MAXCONN)
			v_pt.put("maxconn", v.maxconn);

		pt.add_child("servicegalaxy.nodes.node", v_pt);
	}
}

void sg_xmlcfg::save_netents(bpt::iptree& pt)
{
	BOOST_FOREACH(const sg_netent_t& v, netents) {
		bpt::iptree v_pt;

		v_pt.put("hostid", v.lmid);
		if (v.naddr[0] != '\0')
			v_pt.put("gwsaddr", v.naddr);
		if (v.nlsaddr[0] != '\0')
			v_pt.put("agtaddr", v.nlsaddr);
		if (v.netgroup[0] != '\0')
			v_pt.put("netname", v.netgroup);
		if (!v.tcpkeepalive)
			v_pt.put("tcpkeepalive", "N");
		if (v.nodelay)
			v_pt.put("nodelay", "Y");

		pt.add_child("servicegalaxy.networks.network", v_pt);
	}
}

void sg_xmlcfg::save_sgents(bpt::iptree& pt)
{
	BOOST_FOREACH(const sg_sgent_t& v, sgents) {
		bpt::iptree v_pt;

		v_pt.put("pgname", v.sgname);
		v_pt.put("curid", v.curmid);

		string lmid;
		for (int i = 0; i < MAXLMIDSINLIST; i++) {
			if (v.lmid[i][0] == '\0')
				break;
			if (!lmid.empty())
				lmid += ',';
			lmid += v.lmid[i];
		}
		v_pt.put("hostid", lmid);
		v_pt.put("pgid", v.grpid);
		if (v.envfile[0] != '\0')
			v_pt.put("profile", v.envfile);

		pt.add_child("servicegalaxy.pgroups.pgroup", v_pt);
	}
}

void sg_xmlcfg::save_svrents(bpt::iptree& pt)
{
	char buf[32];

	BOOST_FOREACH(const sg_svrent_t& v, svrents) {
		bpt::iptree v_pt;

		string options;
		if (v.sparms.rqparms.options & RESTARTABLE) {
			if (!options.empty())
				options += " ";
			options += "RESTART";
		}

		if (v.bparms.flags & BF_REPLYQ) {
			if (!options.empty())
				options += " ";
			options += "REPLYQ";
		}

		if (v.bparms.flags & BF_PRESERVE_FD) {
			if (!options.empty())
				options += " ";
			options += "PRESERVE_FD";
		}

		v_pt.put("options", options);
		v_pt.put("rstint", v.sparms.rqparms.grace);
		v_pt.put("maxrst", v.sparms.rqparms.maxgen);
		if (v.sparms.rqparms.saddr[0] != '\0')
			v_pt.put("lqname", v.sparms.rqparms.saddr);
		v_pt.put("exitcmd", v.sparms.rqparms.rcmd);
		v_pt.put("procname", v.sparms.rqparms.filename);
		v_pt.put("prcid", v.sparms.svrproc.svrid);
		if (v.sparms.rqperm != 0) {
			sprintf(buf, "0%o", v.sparms.rqperm);
			v_pt.put("rqperm", buf);
		}
		if (v.sparms.rpperm != 0) {
			sprintf(buf, "0%o", v.sparms.rpperm);
			v_pt.put("rpperm", buf);
		}
		if (v.sparms.sequence != 0)
			v_pt.put("priority", v.sparms.sequence);
		v_pt.put("min", v.bparms.bootmin);
		v_pt.put("max", v.sparms.svridmax - v.sparms.svrproc.svrid + 1);
		if (v.sparms.max_num_msg != 0)
			v_pt.put("msgmnb", v.sparms.max_num_msg);
		if (v.sparms.max_msg_size != 0)
			v_pt.put("msgmax", v.sparms.max_msg_size);
		if (v.sparms.cmprs_limit != 0)
			v_pt.put("cmprslimit", v.sparms.cmprs_limit);
		if (v.sparms.remedy != 0)
			v_pt.put("costfix", v.sparms.remedy);
		v_pt.put("pgname", v.sparms.sgname);
		v_pt.put("args", v.bparms.clopt);
		if (v.bparms.envfile[0] != '\0')
			v_pt.put("profile", v.bparms.envfile);

		pt.add_child("servicegalaxy.processes.process", v_pt);
	}
}

void sg_xmlcfg::save_svcents(bpt::iptree& pt)
{
	BOOST_FOREACH(const sg_svcent_t& v, svcents) {
		bpt::iptree v_pt;

		v_pt.put("pattern", v.svc_pattern);

		if (v.grpid != DFLT_PGID) {
			bool found = false;
			BOOST_FOREACH(const sg_sgent_t& sgent, sgents) {
				if (sgent.grpid == v.grpid) {
					v_pt.put("pgname", sgent.sgname);
					found = true;
					break;
				}
			}

			if (!found)
				throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Can't find pgid {1} in pgroups") % v.grpid).str(SGLOCALE));
		}

		if (v.priority != DFLT_PRIORITY)
			v_pt.put("priority", v.priority);

		string policy;
		if (v.svc_policy & POLICY_LF) {
			if (!policy.empty())
				policy += " ";
			policy += "LF";
		}

		if (v.svc_policy & POLICY_RR) {
			if (!policy.empty())
				policy += " ";
			policy += "RR";
		}

		if (v.svc_policy & POLICY_IF) {
			if (!policy.empty())
				policy += " ";
			policy += "IF";
		}

		v_pt.put("policy", policy);

		if (v.svc_load != DFLT_COST)
			v_pt.put("cost", v.svc_load);
		if (v.load_limit != DFLT_COSTLIMIT)
			v_pt.put("costlimit", v.load_limit);
		if (v.svctimeout != DFLT_TIMEOUT)
			v_pt.put("timeout", v.svctimeout);
		if (v.svcblktime != DFLT_BLKTIME)
			v_pt.put("blktime", v.svcblktime);

		pt.add_child("servicegalaxy.operations.operation", v_pt);
	}
}

void sg_xmlcfg::validate_address(const string& path, const string& addr)
{
	if (addr.size() < 5)
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Invalid {1} {2} parameter given, at least 5 characters long") % path % addr).str(SGLOCALE));

	if (memcmp(addr.c_str(), "//", 2) != 0)
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Invalid {1} {2} parameter given, should start with \"//\"") % path % addr).str(SGLOCALE));

	string::size_type pos = addr.find(":");
	if (pos == string::npos)
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Invalid {1} {2} parameter given, missing ':'") % path % addr).str(SGLOCALE));

	string hostname = addr.substr(2, pos - 2);
	if (hostname.empty())
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Invalid {1} {2} parameter given, missing hostname") % path % addr).str(SGLOCALE));

	string port = addr.substr(pos + 1, addr.length() - pos - 1);
	if (port.empty())
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Invalid {1} {2} parameter given, missing port") % path % addr).str(SGLOCALE));

	BOOST_FOREACH(char ch, port) {
		if (!::isdigit(ch))
			throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Invalid {1} {2} parameter given, invalid port") % path % addr).str(SGLOCALE));
	}
}

bool sg_xmlcfg::isdigit(const string& str)
{
	BOOST_FOREACH(char ch, str) {
		if (!::isdigit(ch))
			return false;
	}

	return true;
}

void sg_xmlcfg::parse_perm(const string& perm_str, int& perm, const string& node_name)
{
	if (perm_str.empty()) {
		perm = DFLT_PERM;
	} else if (perm_str[0] != '0') {
		perm = 0;
		BOOST_FOREACH(char ch, perm_str) {
			if (ch < '0' || ch > '9')
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: {1} must be numeric.") % node_name).str(SGLOCALE));
			perm *= 10;
			perm += ch - '0';
		}
	} else {
		perm = 0;
		BOOST_FOREACH(char ch, perm_str) {
			if (ch < '0' || ch > '8')
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: {1} must be numeric.") % node_name).str(SGLOCALE));
			perm *= 8;
			perm += ch - '0';
		}
	}

	if (perm <= 0 || perm > 0777)
		throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: {1} must be between 1 and 0777.") % node_name).str(SGLOCALE));

	if ((perm & 0600) != 0600)
		throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: {1} must have permission 0600.") % node_name).str(SGLOCALE));
}

}
}

