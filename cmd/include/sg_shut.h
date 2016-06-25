#if !defined(__SG_SHUT_H__)
#define __SG_SHUT_H__

#include "bs_base.h"

namespace ai
{
namespace sg
{

struct sg_process_t
{
	int proctype;	/* sgmngr, sgclst or anything else */
	bool bridge;		/* bridge on machine?? */
	int mid;		/* machine id */
	int grpid;		/* process's group */
	int svrid;		/* process's id */
	bool hopt;		/* do the -H option?? */
};

struct sg_lan_t
{
	int mid;		/* machine id */
	int status;		/* status */
};

// list of servers to be stopped
struct sg_ulist_t
{
	sg_proc_t proc;		/* srvgrp, srvid, etc. */
	sgid_t sgid;		/* sgid of STE */
};

class sg_shut : public bs_base
{
public:
	sg_shut();
	~sg_shut();

protected:
	virtual int do_admins();
	virtual int do_servers();

	void find_dbbm();
	bool get_process(const std::string& proc_name, int mid);
	int stopadms();
	bool gws_suspended(int mid);
	void getlanstat();
	bool no_other_lan(int mid);
	void drain_queue();
	void setsvstat(int status, int flag);
	int stopproc(int scope, sg_ste_t& ste);
	bool restartable(const sg_ste_t& ste);
	bool oktoreloc(const sg_ste_t& ste);
	bool partition(const sg_process_t& process);
	int killadm(const sg_process_t& process);

	sg_process_t dbbm_proc;
	bool dbbm_listed;
	std::list<sg_process_t> procs;
	std::vector<sg_lan_t> lans;
	std::list<sg_ulist_t> ulist;
};

}
}

#endif

