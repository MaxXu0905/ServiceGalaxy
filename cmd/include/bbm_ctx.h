#if !defined(__BBM_CTX_H__)
#define __BBM_CTX_H__

#include "sg_internal.h"

namespace ai
{
namespace sg
{

// sgclst registry_t table status settings
int const ACK_DONE = 0x01;		/* Broadcast RPC reqst accepted */
int const NACK_DONE = 0x02;		/* Broadcast RPC reqst rejected, send new BB */
int const BCSTFAIL = 0x04;		/* Broadcast failed, force sgmngr restart */
int const PARTITIONED = 0x08;	/* sgmngr is down or unreachable */
int const SGCFGFAIL = 0x10;		/* SGPROFILE file out of date */

int const SGFATAL = -2;

int const DBBM_STATE_NA = 0;
int const DBBM_STATE_OK = 1;
int const DBBM_STATE_MayDown = 2;
int const DBBM_STATE_Down = 3;

struct regstr_t
{
	int orig;
	int mark;	// 1: valid myexitproc
	sg_proc_t myexitproc;	/* replace the exitproc in the TM TCM */
};

// The sgclst's cache of sgmngr addresses
struct registry_t
{
	sg_proc_t proc;
	sgid_t sgid;
	int status;
	time_t timestamp;
	int tardy;		/* Reset to 0 if sgmngr heard from,
					  *  and incremented each sgmngr check.
					  */
	int release;	// Set to SGRELEASE
	int protocol;	// protocol version for node
	int bbvers;		// Last reported BB version timestamp
	int sgvers;		// Last reported SGPROFILE version timestamp
	int usrcnt;		// active user count

	registry_t() {
		proc.clear();
	}
};

class bbm_ctx
{
public:
	static bbm_ctx * instance();
	~bbm_ctx();

	string _BBM_rec_file;
	bool _BBM_inbbclean;
	bool _BBM_sig_ign;
	bool _BBM_restricted;

	boost::unordered_map<int, registry_t> _BBM_regmap;	// sgclst's cache of sgmngr addresses
	registry_t _BBM_regadm[MAXADMIN];

	size_t _BBM_pbbsize;
	bool _BBM_inmasterbb;

	int _BBM_oldmday;
	long _BBM_checkiter;
	long _BBM_noticed;
	bool _BBM_upgrade;

	int _BBM_lbbvers;
	int _BBM_lsgvers;

	int _BBM_cleantime;
	int _BBM_querytime;
	bool _BBM_oldflag;
	sg_ste_t _BBM_olddbbm;

	bool _BBM_amregistered;
	bool _BBM_force_check;

	sg_ste_t _BBM_gwste;
	sg_proc_t *_BBM_gwproc;

	int _BBM_upload;
	time_t _BBM_interval_start_time;
	boost::dynamic_bitset<> _BBM_rte_scan_marks;

	int _BBM_IMOK_norply_count;
	int _BBM_IMOK_rply_interval;
	sgid_t _BBM_master_BBM_sgid;
	int _BBM_DBBM_status;
	bool _BBM_master_suspended;

private:
	bbm_ctx();

	static bbm_ctx _instance;
};

}
}

#endif

