#if !defined(__SGC_CTX_H__)
#define __SGC_CTX_H__

#include "sg_struct.h"
#include "sg_message.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

int const STARVPROTECT = 10;

int const PIDNSHIFT = 16;
int const PIDNMASK = 0xffff;

int const PMID2MID = 1;
int const LMID2MID = 2;
int const NODE2MID = 3;

int const CALL_HANDLE = 0x01;
int const ACALL_HANDLE = 0x02;

class sgc_ctx
{
private:
	boost::shared_ptr<bi::shared_memory_object> mapping_mgr;
	boost::shared_ptr<bi::mapped_region> region_mgr;

	int _SGC_ctx_level;
	pthread_t _SGC_thrid;
	boost::recursive_mutex _SGC_ctx_mutex;

public:
	sgc_ctx();
	~sgc_ctx();

	bool enter(sgt_ctx *SGT, int flags);
	bool leave(sgt_ctx *SGT, int flags);
	int lock(sgt_ctx *SGT, int ctx_level);
	int unlock(sgt_ctx *SGT);
	void get_defaults();
	bool get_here(sg_svrent_t& svrent);
	bool remote(int mid) const;
	bool set_bbtype(const string& name);
	bool set_atype(const string& name);
	bool attach();
	void detach();
	bool hookup(const string& name);
	bool buildpe();
	int getmid();
	void setmid(int mid);
	int getkey();
	int netkey();
	bool get_bbparms();
	void qcleanup(bool rmflag);
	sg_proc_t * fmaster(int mid, sg_proc_t *c_proc);

	int MAXPES() const { return _SGC_bbp->bbparms.maxpes; }
	int MAXNODES() const { return _SGC_bbp->bbparms.maxnodes; }
	int MAXACCSRS() const { return _SGC_bbp->bbparms.maxaccsrs; }
	int MAXQUES() const { return _SGC_bbp->bbparms.maxques; }
	int MAXSGT() const { return _SGC_bbp->bbparms.maxsgt; }
	int MAXSVRS() const { return _SGC_bbp->bbparms.maxsvrs; }
	int MAXSVCS() const { return _SGC_bbp->bbparms.maxsvcs; }
	int QUEBKTS() const { return _SGC_bbp->bbparms.quebkts; }
	int SGTBKTS() const { return _SGC_bbp->bbparms.sgtbkts; }
	int SVRBKTS() const { return _SGC_bbp->bbparms.svrbkts; }
	int SVCBKTS() const { return _SGC_bbp->bbparms.svcbkts; }

	int lmid2mid(const char *lmid) const;
	int pmid2mid(const char *pmid) const;
	const char * mid2lmid(int mid) const;
	const char * mid2pmid(int mid) const;
	sg_pte_t * mid2pte(int mid);
	const sg_pte_t * mid2pte(int mid) const;
	const char * pmtolm(int mid);

	int idx2mid(int nidx, int pidx) const { return ((nidx << PIDNSHIFT) | (pidx & PIDNMASK)); }
	int midnidx(int mid) const { return ((mid >> PIDNSHIFT) & PIDNMASK); }
	int midpidx(int mid) const { return (mid & PIDNMASK); }
	int mid2grp(int mid) const { return midpidx(mid + CLST_PGID + 1); }
	int grp2mid(int grpid) const { return _SGC_ptes[grpid - CLST_PGID - 1].mid; }
	int midkey(int mid) const { return ((mid + 1) << MCHIDSHIFT) | _SGC_wka; }
	long netload(int mid) const;

	const char * mid2node(int mid) const;

	bool innet(int mid) const { return (mid == ALLMID ? false : _SGC_ptes[midpidx(mid)].flags & NP_NETWORK); }
	bool tstnode(int mid) const { return (_SGC_ntes[midnidx(mid)].flags & NP_ACCESS); }

	const char * admcnm(int slot);
	const char * admuname();

	bool set_bba(const string& name);
	void inithdr(sg_message& msg) const;
	void clear();

	int _SGC_ctx_id;				// Context在数组中的标识

	sg_bboard_t *_SGC_bbp;			// 共享内存头开始位置
	sg_bbmap_t *_SGC_bbmap; 		// 各个关键的节点的偏移量数组
	sg_pte_t *_SGC_ptes;			// 主机节点开始位置
	sg_nte_t *_SGC_ntes;			// 网络节点开始位置
	sg_rte_t *_SGC_rtes;			// 注册节点开始位置
	sg_qte_t *_SGC_qtes;			// 消息队列节点开始位置
	sg_sgte_t *_SGC_sgtes;			// SERVER GROUP节点开始位置
	sg_ste_t *_SGC_stes;			// SERVER节点开始位置
	sg_scte_t *_SGC_sctes;			// 服务节点开始位置
	int *_SGC_qte_hash; 			// 消息队列hash数组
	int *_SGC_sgte_hash;			// SERVER GROUP hash数组
	int *_SGC_ste_hash; 			// SERVER hash数组
	int *_SGC_scte_hash;			// 服务hash数组
	sg_misc_t *_SGC_misc;			// 使用的队列情况

	int _SGC_slot;					// 注册节点slot
	sg_rte_t *_SGC_crte;			// 当前上下文的注册节点
	long _SGC_rplyq_serial;			// 当大消息通过共享内存传递时的名称序列号

	int _SGC_handleflags[MAXHANDLES];	// 句柄是否使用标识，为1标识未使用
	int _SGC_handles[MAXHANDLES];	// 句柄数组
	int _SGC_hdllastfreed;			// 最后一次释放的句柄

	sg_proc_t _SGC_proc;	 	// 当前上下文进程
	bool _SGC_amclient;			// 当前上下文是客户端
	bool _SGC_amadm;			// 当前上下文是ADMIN
	int _SGC_svrstate;			// 当前进程的状态
	bool _SGC_here_gotten;		// 是否创建了消息队列
	bool _SGC_attach_envread;	// 是否已经读取环境变量
	bool _SGC_didregister;
	int _SGC_blktime;

	bool _SGC_gotdflts;
	int _SGC_gotparms;
	pid_t _SGC_sginit_pid;
	pid_t _SGC_sginit_savepid;

	size_t _SGC_max_num_msg;
	size_t _SGC_max_msg_size;
	size_t _SGC_cmprs_limit;
	uid_t _SGC_uid;
	gid_t _SGC_gid;
	mode_t _SGC_perm;

	bool _SGC_sndmsg_wkidset;
	int _SGC_sndmsg_wkid;
	sg_proc_t _SGC_sndmsg_wkproc;
	int _SGC_wka;
	int _SGC_findgw_version;
	int _SGC_findgw_nextcache;
	std::vector<sg_ste_t> _SGC_findgw_cache;

	int _SGC_do_remedy;
	int _SGC_svc_rounds;
	sg_proc_t _SGC_mbrtbl_curraddr;
	sg_proc_t *_SGC_mbrtbl_raddr;

	int _SGC_getmid_mid;
	std::string _SGC_getmid_pmid;

	int _SGC_asvcindx;
	int _SGC_svcindx;
	bool _SGC_sssq;
	bool _SGC_svstats_doingwork;
	int _SGC_sgidx;
	sgid_t _SGC_asvcsgid;

	std::string _SGC_master_sgconfig;
	std::string _SGC_master_pmid;

	int _SGC_enter_inatmi;
	int _SGC_handlesvcindex[MAXHANDLES];
	int _SGC_getrply_count;
	int _SGC_receive_getanycnt;
	std::multimap<int, message_pointer> _SGC_rcved_msgs;
	int _SGC_rcvrq_countdown;
	int _SGC_rcvrq_cpending;

	sg_bbasw_t *_SGC_bbasw;
	sg_ldbsw_t *_SGC_ldbsw;
	sg_authsw_t *_SGC_authsw;

	sg_bbparms_t _SGC_getbbparms_curbbp;
	int _SGC_mkmsg_ritercount;
	bool _SGC_enable_alrm;

	// 服务调用时指定主机ID
	int _SGC_special_mid;

private:
	int computemid(const char *name, int scope) const;
	bool setpeval(sg_bbparms_t& bbparms);
	bool set_ldb(const string& name);
	bool set_auth(const string& name);
	void bbfail();

	static void sigalrm(int signo);
};

}
}

#endif

