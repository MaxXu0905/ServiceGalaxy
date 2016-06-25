#if !defined(__RESTARTSVR_H__)
#define __RESTARTSVR_H__

#include "sg_internal.h"

namespace ai
{
namespace sg
{

int const SUCCESS = 0;	/* everything OK */
int const FAILURE = 1;	/* tried but couldn't - call cleanupsrv */
int const ERROR = 2;	/* fatal error - don't do anything */
int const MAXECODE = 3;	/* to size stats array */

int const RST_SEQNO_PROCESSED = -12; /* Processed = "restarted or tried to" */
int const RST_SEQNO_DEFAULT = -10;   /* Restarted last */
int const RST_SEQNO_DBBM = -8;
int const RST_SEQNO_BBM = -6;
int const RST_SEQNO_GWS = -4;

struct svr_info_t
{
	int grpid;
	int svrid;
	int seq_no;

	bool operator<(const svr_info_t& rhs) const {
		return seq_no < rhs.seq_no;
	}
};

class sgrecover : public sg_manager
{
public:
	sgrecover();
	~sgrecover();

	int run(int argc, char **argv);

private:
	int init(int argc, char **argv);

	int do_restart();
	bool morework(int& cursor);
	int start_bsgws();
	int rstrt_setenv();
	int setadmenv();
	int setsvrenv();
	void cleanup(int errcode);
	int get_sequenced_svrs();
	int get_restartable_svrs(int cmdsid, int cmdgid);
	int sort_restartable_by_sequence();
	void tixe();
	void logmsg(const char *filename, int linenum, const string& msg);
	static void onintr(int signo);
	static bool ignintr();

	int svrid;
	int grpid;

	bool notty;
	sg_ste_t ste;
	sg_svrent_t admin;
	pid_t oldpid;
	pid_t newpid;
	int stats[MAXECODE];
	std::vector<svr_info_t> seq_svrs;
	std::vector<svr_info_t> res_svrs;
	bool called_once;
	char **cmdv;

	static bool gotintr;
};

}
}

#endif

