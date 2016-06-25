#if !defined(__SG_XMLCFG_H__)
#define __SG_XMLCFG_H__

#include "sg_internal.h"

namespace bpt = boost::property_tree;

namespace ai
{
namespace sg
{

// CLUSTER缺省值
int const DFLT_PERM = 0660;
int const DFLT_LDBAL = LDBAL_RR;
int const DFLT_MAXNODES = 100;
int const DFLT_MAXNETS = 100;
int const DFLT_MAXCLTS = 10000;
int const DFLT_MAXQUES = 10000;
int const DFLT_MAXPGS = 1000;
int const DFLT_MAXPROCS = 10000;
int const DFLT_MAXOPS = 10000;
int const DFLT_QUEBKTS = 10000;
int const DFLT_PGBKTS = 1000;
int const DFLT_PROCBKTS = 10000;
int const DFLT_OPBKTS = 10000;
int const DFLT_POLLTIME = 60;
int const DFLT_ROBUSTINT = 10;
int const DFLT_STACKSIZE = 32 * 1024 * 1024;
int const DFLT_MSGMNB = 1000;
int const DFLT_MSGMAX = 4096;
size_t const DFLT_CMPRSLIMIT = 100 * 1024;
int const DFLT_COSTFIX = 1000;
int const DFLT_NFILES = 1000;
int const DFLT_BLKTIME = 10;
int const DFLT_PRIORITY = 50;
int const DFLT_CLSTWAIT = 10;
int const DFLT_MNGRQUERY = 10;
int const DFLT_INITWAIT = 3600;

// NODES缺省值
long const DFLT_NETCOST = 100;
int const DFLT_MAXCONN = 1;

// PROCESSSES缺省值
int const DFLT_RSTINT = 60;

// OPERATIONS缺省值
int const POLICY_LF = 0x1;	// 如果本地提供该服务，则无条件选择本地
int const POLICY_RR = 0x2;	// 所有服务中选最优
int const POLICY_IF = 0x4;	// 优先选择空闲节点

long const DFLT_COST = 100;
long const DFLT_COSTLIMIT = 10000;
int const DFLT_TIMEOUT = 60;
int const DFLT_POLICY = POLICY_RR;

class sg_xmlcfg
{
public:
	sg_xmlcfg(const string& xml_file_);
	~sg_xmlcfg();

	void load(bool enable_hidden);
	void save(bool enable_hidden);

	sg_bbparms_t bbparms;
	vector<sg_mchent_t> mchents;
	vector<sg_netent_t> netents;
	vector<sg_sgent_t> sgents;
	vector<sg_svrent_t> svrents;
	vector<sg_svcent_t> svcents;

private:
	void load_bbparms(const bpt::iptree& pt, bool enable_hidden);
	void load_mchents(const bpt::iptree& pt);
	void load_netents(const bpt::iptree& pt);
	void load_sgents(const bpt::iptree& pt);
	void load_svrents(const bpt::iptree& pt);
	void load_svcents(const bpt::iptree& pt);
	void save_bbparms(bpt::iptree& pt, bool enable_hidden);
	void save_mchents(bpt::iptree& pt);
	void save_netents(bpt::iptree& pt);
	void save_sgents(bpt::iptree& pt);
	void save_svrents(bpt::iptree& pt);
	void save_svcents(bpt::iptree& pt);

	static void validate_address(const string& path, const string& addr);
	static bool isdigit(const string& str);
	static void parse_perm(const string& perm_str, int& perm, const string& node_name);

	sgp_ctx *SGP;
	string xml_file;
};

}
}

#endif

