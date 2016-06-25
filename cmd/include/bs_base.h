#if !defined(__BS_BASE_H__)
#define __BS_BASE_H__

#include "sg_internal.h"
#include "bs_struct.h"

namespace ai
{
namespace sg
{

int const CF_HIDDEN = 0x1;

struct grpchk_t
{
	int grpid;
	int mid;
};

struct sg_grpinfo_t
{
	int seq;
	int grpid;
};

class bs_base
{
public:
	bs_base();
	virtual ~bs_base();

	void init(int argc, char **argv);
	void run();

protected:
	virtual int do_admins() = 0;
	virtual int do_servers() = 0;

	void init_node(bool value);
	void set_node(int mid, bool value);
	void sort();
	bool bbmok(int mid);
	bool locok(const sg_svrent_t& svrent);
	bool grpok(const sg_svrent_t& svrent);
	bool idok(const sg_svrent_t& svrent);
	bool aoutok(const sg_svrent_t& svrent);
	bool seqok(const sg_svrent_t& svrent);
	void do_uerror();
	void warning(const string& msg);
	void fatal(const string& msg);

	static bool ignintr();
	static void onintr(int signo);
	static void onalrm(int signo);

	gpp_ctx *GPP;
	sgp_ctx *SGP;
	sgt_ctx *SGT;

	bool prompt;
	int opflags;
	bool byid;
	int bootmin;
	int boottimeout;
	int who;
	int dbbm_mid;
	bool dbbm_act;
	int exit_code;
	int kill_signo;
	int delay_time;

	boost::dynamic_bitset<> bitmap;
	vector<sg_tditem_t> tdlist;
	vector<grpchk_t> groups;
	sg_proc_t master_proc;

	static int bsflags;
	static bool gotintr;
	static bool handled;
	static bool hurry;
};

}
}

#endif

