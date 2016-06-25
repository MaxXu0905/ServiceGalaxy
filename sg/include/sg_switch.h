#if !defined(__SG_SWITCH_H__)
#define __SG_SWITCH_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

int const BBT_SHM = 0x100;		/* shared memory based */
int const BBT_MSG = 0x200;		/* message based */
int const BBT_MP = 0x400;		/* multi-processor based */
int const BBT_MASK = 0xff00;	/* bbtype mask */
int const BBT_SHIFT = 8;		/* starting bit for bbtype */

struct sg_bbasw_t
{
	const char *name;
	int bbtype;

	// PTE
	int (sg_pte::*cpte)(sg_mchent_t& parms, int flags);
	// NTE
	int (sg_nte::*cnte)(sg_netent_t& parms, int flags);
	// QTE
	sgid_t (sg_qte::*cqte)(sg_queparms_t& parms);
	int (sg_qte::*dqte)(sgid_t *sgids, int cnt);
	int (sg_qte::*gdqte)(sgid_t *sgids, int cnt);
	int (sg_qte::*uqte)(sg_qte_t *qtes, int cnt, int flags);
	int (sg_qte::*guqte)(sg_qte_t *qtes, int cnt, int flags);
	int (sg_qte::*rqte)(int scope, const sg_qte_t *key, sg_qte_t *qtes, sgid_t *sgids);
	// SGTE
	sgid_t (sg_sgte::*csgte)(sg_sgparms_t& parms);
	int (sg_sgte::*dsgte)(sgid_t *sgids, int cnt);
	int (sg_sgte::*usgte)(sg_sgte_t *sgtes, int cnt, int flags);
	int (sg_sgte::*rsgte)(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids);
	// STE
	sgid_t (sg_ste::*cste)(sgid_t qsgid, sg_svrparms_t& parms);
	int (sg_ste::*dste)(sgid_t *sgids, int cnt);
	int (sg_ste::*gdste)(sgid_t *sgids, int cnt);
	int (sg_ste::*uste)(sg_ste_t *stes, int cnt, int flags);
	int (sg_ste::*guste)(sg_ste_t *stes, int cnt, int flags);
	int (sg_ste::*rste)(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids);
	sgid_t (sg_ste::*ogsvc)(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt);
	int (sg_ste::*rmste)(sgid_t sgid);
	// SCTE
	sgid_t (sg_scte::*cscte)(sgid_t ssgid, sg_svcparms_t& parms, int flags);
	int (sg_scte::*dscte)(sgid_t *sgids, int cnt);
	int (sg_scte::*gdscte)(sgid_t *sgids, int cnt);
	int (sg_scte::*uscte)(sg_scte_t *sctes, int cnt, int flags);
	int (sg_scte::*guscte)(sg_scte_t *sctes, int cnt, int flags);
	int (sg_scte::*rscte)(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids);
	sgid_t (sg_scte::*ascte)(sgid_t ssgid, sg_svcparms_t& parms);
	// RPC
	int (sg_rpc::*ustat)(sgid_t *sgids, int cnt, int status, int flag);
	int (sg_rpc::*ubbver)();
	int (sg_rpc::*laninc)();
};

class sg_bbasw
{
public:
	void set_switch(const string& name);
	virtual ~sg_bbasw() {}

protected:
	virtual sg_bbasw_t * get_switch();
};

struct sg_ldbsw_t
{
	const char *name;

	// increase server work stats
	bool (sg_qte::*enqueue)(sg_scte_t *scte);
	// undo effect of enqueue
	bool (sg_qte::*dequeue)(sg_scte_t *scte, bool undo);
	// return indication of server integrity
	int (sg_viable::*viable)(sg_ste_t *ste, int& spawn_flag);
	// select best of 2 servers
	int (sg_qte::*choose)(sg_qte_t *curr, sg_qte_t *best, long curr_load, long best_load);
	// synchronize algorithm for resetting load totals
	void (sg_qte::*syncq)();
};

class sg_ldbsw
{
public:
	void set_switch(const string& name);
	virtual ~sg_ldbsw() {}

protected:
	virtual sg_ldbsw_t * get_switch();
};

struct sg_authsw_t
{
	const char *name;

	bool (sg_rte::*enter)();
	void (sg_rte::*leave)(sg_rte_t *rte);
	bool (sg_rte::*update)(sg_rte_t *rte);
	bool (sg_rte::*clean)(pid_t pid, sg_rte_t *rte);
};

class sg_authsw
{
public:
	void set_switch(const string& name);
	virtual ~sg_authsw() {}

protected:
	virtual sg_authsw_t * get_switch();
};

class sg_switch
{
public:
	static sg_switch& instance();

	virtual ~sg_switch();
	virtual bool svrstop(sgt_ctx *SGT);
	virtual void bbclean(sgt_ctx *SGT, int mid, ttypes sect, int& spawn_flags);
	virtual bool getack(sgt_ctx *SGT, message_pointer& msg, int flags, int timeout);
	virtual void shutrply(sgt_ctx *SGT, message_pointer& msg, int status, sg_bbparms_t *bbparms);

protected:
	sg_switch();

	gpp_ctx *GPP;
	sgp_ctx *SGP;

	friend class base_switch_initializer;
};

}
}

#endif

