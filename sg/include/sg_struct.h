#if !defined(__SG_STRUCT_H__)
#define __SG_STRUCT_H__

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include <ostream>
#include <setjmp.h>
#include <boost/scope_exit.hpp>
#include <boost/thread.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_recursive_mutex.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include "gpp_ctx.h"
#include "gpt_ctx.h"
#include "sg_align.h"
#include "sg_exception.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

int const SGRELEASE = 10;

int const VSHIFT = 24;
int const VERSMASK = 0xff000000;
#undef VERSION
int const VERSION = (1 << VSHIFT);

int const MAX_SGROOT = 511;
int const MAX_APPROOT = 511;
int const MAX_SGCONFIG = 511;
int const MAX_SVC_NAME = 31;
int const MAX_PATTERN = 1023;
int const MAX_CLASS_NAME = 127;
int const MAX_SHM_NAME = 31;
int const MAX_QUEUE_PREFIX = 15;
int const MAX_QUEUE_NAME = 31;
int const MAX_LSTRING = 255;
int const MAX_IDENT = 31;
int const MAX_STRING = 63;

int const SGMINVAL = 0;			/* minimum error message */
int const SGEABORT = 1;
int const SGEBADDESC = 2;
int const SGEBLOCK = 3;
int const SGEINVAL = 4;
int const SGELIMIT = 5;
int const SGENOENT = 6;
int const SGEOS = 7;
int const SGEPERM = 8;
int const SGEPROTO = 9;
int const SGESVCERR = 10;
int const SGESVCFAIL = 11;
int const SGESYSTEM = 12;
int const SGETIME = 13;
int const SGETRAN = 14;
int const SGGOTSIG = 15;
int const SGERMERR = 16;
int const SGERELEASE = 17;
int const SGEMATCH = 18;
int const SGEDIAGNOSTIC = 19;
int const SGESVRSTAT = 20;		/* bad server status */
int const SGEBBINSANE = 21;		/* BB is insane */
int const SGEDUPENT = 22;		/* duplicate table entry */
int const SGECHILD = 23;		/* child start problem */
int const SGENOMSG = 24;		/* no message */
int const SGENOMNGR = 25;		/* no sgmngr */
int const SGENOCLST = 26;		/* no sgclst */
int const SGENOGWS = 27;		/* no sggws available */
int const SGEAPPINIT = 28;		/* svrinit() failure */
int const SGENEEDPCLEAN = 29;	/* sgmngr failed to boot.. pclean may help */
int const SGENOFUNC = 30;		/* unknown function address */
int const SGMAXVAL = 31;		/* maximum error message */

int const SGED_MINVAL = 0;       /* minimum error message */
int const SGED_SVCTIMEOUT = 1;
int const SGED_TERM = 2;
int const SGED_NOUNSOLHANDLER = 3;
int const SGED_NOCLIENT = 4;
int const SGED_CLIENTDISCONNECTED = 5;
int const SGED_PERM = 6;
int const SGED_OTS_INTERNAL = 7;
int const SGED_INVALIDCONTEXT = 8;
int const SGED_INVALID_XA_TRANSACTION = 9;
int const SGED_GROUP_FORBIDDEN = 10;
int const SGED_MAXVAL = 11;      /* maximum error message */

// bulletin board options
int const SGLAN = 0x01;			/* lan mode */
int const SGMIGALLOWED = 0x02;	/* allow migration */
int const SGHA = 0x04;			/* high availability configuration */
int const SGACCSTATS = 0x08;	/* provide accurate stats */
int const SGCLEXITL = 0x010;	/* client exit - notify last group */
int const SGAA = 0x020;			/* Authorization/Audit security */

// ldbal options
int const LDBAL_RR = 1;			/* Round Robin */
int const LDBAL_RT = 2;			/* Real Time */

// Administrator RTE SLOTs
int const AT_BBM = 0;		/* only one sgmngr per machine */
int const AT_ADMIN = 1;	/* only one ADMIN at a time */
int const AT_RESTART = 2;	/* only one restartsrv at a time */
int const AT_CLEANUP = 3;	/* only one cleanupsrv at a time */
int const AT_SHUTDOWN = 4;/* only one tmshutdown at a time */
int const MAXADMIN = 5;

int const MAXHANDLES = 64;
int const MAX_ENVFILE = 255;

int const SG_NWKILLBIT = 0x08000000;	// Network kill bit - sent to gws

int const CALLRPLYBASE = 0x30000001;	// lowest mtype for a call() reply
int const GWMTYP = 0x30000000;			// GW mtyp for RPCs
const char GWSVC[] = "GWSVCNM";			// SVC name on RPCs

int const SENDBASE = 0x20000000;		// lowest mtype for a send
int const RPLYBASE = 0x10000000;		// lowest mtype for a reply
int const DFLTRPLYTYP = RPLYBASE + 50;	// default reply type
int const NULLMTYPE = 0;				// NULL message type
int const PRIORANGE = 100;				// send priority range
int const ITERBITS = 6;
int const RPLYBITS = 6;

unsigned int const GHANDLE = 0x80000000;	// a good handle outstanding
unsigned int const THANDLE = 0x40000000;	// handle sent in tran mode
unsigned int const STMASK = 0x3ffffc00;	// mask to obtain # of stales
unsigned int const ITMASK = 0x000003ff;	// mask to obtain interation - ITERBITS
unsigned int const IDXMASK = 0x0000003f;	// mask to obtain handle index

typedef long sgid_t;

sgid_t const BADSGID = 0L;

sgid_t const SGID_CRC = 0xfff8000000000000L;// CRC
int const SGID_CRCBITS = 29;	// CRC bits
int const SGID_CRCSHIFT = 35;	// starting bit for CRC
int const SGID_TABSHIFT = 32;	// starting bit for table #

sgid_t const SGID_QUET = 1L << SGID_TABSHIFT;	// queue table
sgid_t const SGID_SGT = 2L << SGID_TABSHIFT;	// server group table
sgid_t const SGID_SVRT = 3L << SGID_TABSHIFT;	// server table
sgid_t const SGID_SVCT = 4L << SGID_TABSHIFT;	// service table
sgid_t const SGID_TMASK = 7L << SGID_TABSHIFT;	// mask for table #

sgid_t const SGID_SIGNBIT = 0x8000000000000000L;
sgid_t const SGID_IDXMASK = 0x000000007fffffffL;

/* settings of status field of a server */
int const IDLE = 0x00000001; 				/* server is awaiting work */
int const BUSY = 0x00000002; 			/* server is doing something */
int const RESTARTING = 0x00000004;		/* server is being restarted */
int const SUSPENDED = 0x00000008;		/* server is in suspended state */
int const CLEANING = 0x00000010; 		/* cleaning up after dead server */
int const SHUTDOWN = 0x00000020; 		/* server is being shutdown */
int const BRSUSPEND = 0x00000040;		/* Suspended BRIDGE entry */
int const MIGRATING = 0x00000080;		/* server is being migrating */
int const PSUSPEND = 0x00000100; 		/* suspend partitioned sgmngr entry*/
int const SPAWNING = 0x00000200; 		/* spawning new server - CONV servers*/
int const SVCTIMEDOUT = 0x00000400;		/* due to SVCTIMEOUT for errordetail*/
int const EXITING = 0x00000800;			/* service routine call tpreturn with TPEXIT */
int const SVRKILLED = 0x00001000;		/* killed due to SVCTIMEDOUT */

/* settings of options field for a server */
int const RESTARTABLE = 0x1;
int const SGSUSPENDED = 0x2;			// Service is suspended

// registry table entry rflags values
int const CLIENT = 0x00000001;			/* process is a client */
int const SERVER = 0x00000002;			/* process is a server */
int const ADMIN = 0x00000004;			/* process is a admin */
int const PROC_REST = 0x00000008;		/* process is restarting */
int const PROC_DEAD = 0x00000010;		/* process is dead and not restarting */
int const GETANY = 0x00002000;			/* receive any message */
int const R_SUSP = 0x00004000;			/* Suspended client */
int const R_DEAD = 0x00008000;			/* Dead/Killed client */
int const SERVER_CTXT = 0x00020000;		/* entry is for a server context */
int const R_SENT_SIGTERM = 0x00080000;	/* SIGTERM sent to svr on SVCTIMEOUT */

// flags for rstatus
int const P_BUSY = 0x00000001;	/* BUSY bit;  0 => IDLE */
int const P_TRAN = 0x00000008;	/* TRAN bit; 0=> not in tran mode */

// Options used in main() abstraction
int const SGRECORD = 0x001;			/* log service start & stop times */
int const SGONETIME = 0x004;		/* handle one service request */
int const SGNOBLOCKING = 0x008;		/* don't block on receive */
int const SGNOSIGRSTRT = 0x010;		/* don't restart on signal */
int const SGDRAIN = 0x020;			/* receive until queue empty */

int const DFLT_PGID = 30000;
int const CLT_PGID = DFLT_PGID;
int const CLST_PGID = DFLT_PGID + 1;
int const BBC_PGID = CLST_PGID + 1;
int const MNGR_PRCID = 0;
int const GWS_PRCID = 1;
int const CLT_PRCID = 2;
int const MAX_RESERVED_PRCID = 2;

int const SPAWN_RESTARTSRV = 0x01;
int const SPAWN_CLEANUPSRV = 0x02;
int const TIMED_CLEAN = 0x04;

const char BBM_GRP_NAME[] = "BBM_GRP";
const char DBBM_GRP_NAME[] = "DBBM_GRP";
const char DBBM_SVC_NAME[] = "..MASTERBB";
const char BBM_SVC_NAME[] = "..ADJUNCTBB";
const char GW_SVC_NAME[] = "..sggws";

/* SGCHK4UNSOL = 0x4 is also passed to enter() and leave() */
int const NML_TSF = 0x08;
int const ATMIENTER_INVALIDCONTEXT_OK = 0x10;

enum ttypes {
	REGTAB,
	QUETAB,
	SVRTAB,
	SVCTAB,
	SGTAB,
	NOTAB
};

// 节点链表
struct sg_syslink_t
{
	int flink;	// forward sibling link
	int blink;	// backward sibling link
};

// 同一个hash值的节点链表
struct sg_hashlink_t
{
	sgid_t sgid;	// sgid
	int rlink;		// recursive link
	int fhlink;		// forward link
	int bhlink;		// backward link

	int get_index() const;
	void make_sgid(sgid_t type, const void *entry);
	static int get_index(sgid_t sgid);
};

// 相关节点之间的链表
// STE与SCTE之间的关系
struct sg_famlink_t
{
	int plink;	// parent link
	int clink;	// child link
	int fslink;	// forward sibling link
	int bslink;	// backward sibling link
};

//QTE节点链表
struct sg_quelink_t
{
	int qlink;	// 指向QTE节点的索引
};

// 进程信息
struct sg_proc_t
{
	int mid;		// 进程所在主机
	int grpid;		// 进程归属的组
	int svrid;		// 进程的svrid
	pid_t pid;		// 进程号
	int rqaddr;		// 请求消息队列地址
	int rpaddr;		// 应答消息队列地址

	void clear() {
		mid = -1;
		grpid = -1;
		svrid = -1;
		pid = 0;
		rqaddr = -1;
		rpaddr = -1;
	}

	bool operator==(const sg_proc_t& rhs) const { return (grpid == rhs.grpid && svrid == rhs.svrid); }
	bool operator!=(const sg_proc_t& rhs) const { return (grpid != rhs.grpid || svrid != rhs.svrid); }

	bool admin_grp() const { return grpid > DFLT_PGID; }
	bool is_bbm() const { return (grpid != CLST_PGID && svrid == MNGR_PRCID); }
	bool is_dbbm() const { return (grpid == CLST_PGID && svrid == MNGR_PRCID); }
	bool is_gws() const { return svrid == GWS_PRCID; }
};

ostream& operator<<(ostream& os, const sg_proc_t& proc);

#define FRIEND_CLASS_DECLARE \
	friend class sg_pte; \
	friend class sg_nte; \
	friend class sg_rte; \
	friend class sg_qte; \
	friend class sg_sgte; \
	friend class sg_ste; \
	friend class sg_scte; \
	friend class sg_rpc; \
	friend class log_file; \
	friend class sg_svr; \
	friend class sg_meta; \
	friend class sg_handle; \
	friend class sg_bbasw; \
	friend class sg_ldbsw; \
	friend class sg_authsw; \
	friend class sg_switch; \
	friend class sgp_ctx; \
	friend class sgc_ctx; \
	friend class sgt_ctx; \
	friend class bs_base; \
	friend class sg_boot; \
	friend class sg_shut; \
	friend class bbm_svc; \
	friend class sgclear; \
	friend class sgrecover; \
	friend class admin_server; \
	friend class system_parser; \
	friend class adv_parser; \
	friend class una_parser;

class sg_manager;
class sg_config;
class sg_pte;
class sg_nte;
class sg_rte;
class sg_qte;
class sg_sgte;
class sg_ste;
class sg_scte;
class sg_rpc;
class log_file;
class sg_api;
class sg_svc;
class sg_svr;
class sg_meta;
class sg_handle;
class sg_viable;
class sg_bbasw;
class sg_ldbsw;
class sg_authsw;
class sg_switch;
class sgp_ctx;
class sgc_ctx;
class sgt_ctx;
class sg_message;
class sg_rpc_rqst_t;
class sg_rpc_rply_t;
class sg_bbasw_t;
class sg_ldbsw_t;
class sg_authsw_t;

typedef boost::shared_ptr<bi::message_queue> queue_pointer;
typedef boost::shared_ptr<sg_svc> svc_pointer;
typedef boost::shared_ptr<sg_svr> svr_pointer;
typedef boost::shared_ptr<sgc_ctx> sgc_pointer;

// API中用到的标识
int const SGNOBLOCK = 0x00000001;	/* non-blocking send/rcv */
int const SGSIGRSTRT = 0x00000002;	/* restart rcv on interrupt */
int const SGNOREPLY = 0x00000004;	/* no reply expected */
int const SGNOTRAN = 0x00000008;	/* not sent in transaction mode */
int const SGTRAN = 0x00000010;		/* sent in transaction mode */
int const SGNOTIME = 0x00000020;	/* no timeout */
int const SGABSOLUTE = 0x00000040;	/* absolute value on tmsetprio */
int const SGGETANY = 0x00000080;	/* get any valid reply */
int const SGKEEPCMPRS = 0x00000100;	/* keep compress */

// msghdr flags
int const SGADVMETA = 0x80000000;	/* advertise new service */
int const SGNACK = 0x40000000;		/* failure nack */
int const SGSHUTQUEUE = 0x20000000;	/* shutdown queue bit */
int const SGSHUTSERVER = 0x10000000;/* shutdown server bit */
int const SGMETAMSG = 0xF0000000;	/* Meta-msg mask */
int const SGINTERNAL = 0x08000000;	/* internal message */
int const SGFORWARD = 0x04000000;	/* message generated by SGFORWARD */
int const SGCALL = 0x02000000;		/* message generated by SGCALL */
int const SGREPLY = 0x01000000;		/* message is a reply */
int const SGANYGROUP = 0x00800000;	/* choose from any group */
int const SGAWAITRPLY = 0x00400000;	/* wait for reply */
int const SGBYPASS = 0x00200000;	/* do not change meta header */

int const SGMULTICONTEXTS = 0x1;

int const S_SVRID = 0x0001;		/* by server id */
int const S_GRPID = 0x0002;		/* by server group *and* server id */
int const S_GROUP = 0x0004;		/* by server group id only */
int const S_QUEUE = 0x0008;		/* by ascii queue address */
int const S_MACHINE = 0x0010;	/* by machine id */
int const S_BYSGIDS = 0x0020;	/* by SGID */
int const S_BYRLINK = 0x0040;	/* use rlink to access table entries */
int const S_ANY = 0x0080;		/* any match on service name */
int const S_GRPNAME = 0x0100;	/* by server group name */

int const SHMMID = 0;			/* SHM mode machine specification */
int const ALLMID = -1;			/* wild card machine specification */
int const BADMID = -2;			/* error machine specification */

int const MCHIDSHIFT = 18;		/* start bit pos of MCHID in ipckey */

int const U_LOCAL = 0x1;	/* update local BB only */
int const U_GLOBAL = 0x2;	/* update all BBs */
int const U_VIEW = 0x4;		/* update part of a table entry */

int const SGIN_INIT = 0x1;
int const SGIN_SVC = 0x2;
int const SGIN_DONE = 0x4;

int const PT_CLIENT = 0x02;	/* process type is client */
int const PT_BBM = 0x04;	/* process type is sgmngr */
int const PT_GWS = 0x08;	/* process type is sgclst */
int const PT_ADM = 0x10;	/* process type is administrator with
							  * registered slot. will never need to pass
							  * authentication when joining the app.
							  */
int const PT_SYSCLT = 0x20;	/* process type is system client w/o registered
							  * slot. will need to pass authentication
							  * when joining the app.
							  */
int const PT_ADMCLT = 0x40;	/* process type is administrative client w/o
							  * registered slot. will never need to pass
							  * authentication when joining the app.
							  */

int const MA_INVALID = 0x0001;

int const NP_UNUSED =0x0001;	/* Marks entry as unused */
int const NP_1PROC = 0x0002;	/* uniprocessor */
int const NP_NETWORK = 0x0004;	/* was specified in NETWORK section */
int const NP_ACCESS = 0x0008;	/* loc accessible via bridges */
int const NP_SYS1 = 0x0010;		/* Internally reserved value */
int const NP_SYS2 = 0x0020;		/* Internally reserved value */
int const NP_NLSERR = 0x0040;	/* Error contacting NLS on indicated machine */
int const NP_INVALID = 0x0080;	/* Used but invalid entry */

int const NT_UNUSED = 0x1;		/* Marks entry as unused */

// bbtype flags
int const BBF_DBBM = 0x10000;	/* am a sgclst */
int const BBF_LAN = 0x20000;	/* LAN Model */
int const BBF_ALRM = 0x40000;	/* Enable alarm */

int const QUEUE_SYSBIT = 0x60000000;
int const QUEUE_DBBM = 0x20000000;
int const QUEUE_BOOT = 0x40000000;
int const QUEUE_GWS = 0x60000000;
int const QUEUE_VBITS = 9;
int const QUEUE_VMASK = (1 << QUEUE_VBITS) - 1;
int const QUEUE_SHIFT = 20;
int const QUEUE_MASK = (1 << QUEUE_SHIFT) - 1;
int const QUEUE_KMASK = QUEUE_SYSBIT | QUEUE_MASK;
int const QUEUE_EXPIRES = 600;	// 超过10分钟的队列可以重用

// RESOURCE parameters
int const MAX_PASSWD = 32;
int const MAX_MASTERS = 2;
int const MAXBBM = 1024;

struct sg_bbparms_t
{
	int magic;
	int bbrelease;		// BB产品号
	int bbversion;		// BB版本
	int sgversion;		// SGPROFILE version
	pid_t pid;			// 创建者pid
	size_t bbsize;		// 整个内存大小
	int ipckey;			// IPC key
	uid_t uid;			// 用户归属
	uid_t gid;			// 组归属
	int perm;			// 访问模式
	int options;		// 选项
	int ldbal;			// 负载均衡选项
	char passwd[MAX_PASSWD + 1];	// 密码
	char master[MAX_MASTERS][MAX_IDENT + 1];	// 主节点及其备用节点
	int current;		// 当前主节点
	int maxpes;			// 最大允许的主机节点数
	int maxnodes;		// 最大允许的网络节点数
	int maxaccsrs;		// 最大允许的并发访问节点数
	int maxques;		// 最大允许的消息队列数
	int maxsgt;			// 最大服务分组数
	int maxsvrs;		// 最大允许的服务进程节点数
	int maxsvcs;		// 最大允许的服务节点数
	int quebkts;		// QTE hash table大小
	int sgtbkts;		// SGTE hash table大小
	int svrbkts;		// STE hash table大小
	int svcbkts;		// SCTE hash table大小
	int scan_unit;		// GTT检查间隔
	int sanity_scan;	// BB检查间隔次数
	int stack_size;		// 线程栈大小
	size_t max_num_msg;	// 队列最大消息数
	size_t max_msg_size;// 消息最大字节数
	size_t cmprs_limit;	// 压缩阀值
	int remedy;			// 多少次调用后进行修正
	int max_open_queues;// 最大允许打开的队列数
	int block_time;		// 调用阻塞时间
	int dbbm_wait;		// DBBM等待响应时间
	int bbm_query;		// BBM查询等待时间
	int init_wait;		// 初始化等待时间
};

struct sg_bbmeters_t
{
	int status;			// Overall BB status
	long currload;		// Total work outstanding for BB
	int cques;			// # of queues in use
	int csvrs;			// # of registered servers
	int csvcs;			// # of offered services
	int csgt;			// # of server groups
	int caccsrs;		// # of registered accessors
	int maxmachines;	// Max # of active machines
	int maxques;		// Max # of queues in use
	int maxsvrs;		// Max # of registered servers
	int maxsvcs;		// Max # of offered services
	int maxsgt;			// Max # of server groups
	int maxaccsrs;		// Max # of registered accessors
	int wkinitiated;	// work initiated from this BB
	int wkcompleted;	// work completed on this BB
	int lanversion;		// LAN version number
	int rreqmade;		// Number of tp[a]call()'s initiated here
	time_t timestamp;	// recent timestamp (within scan_unit)

	void refresh_data(const sg_bbmeters_t& bbmeters);
};

struct sg_bbmap_t
{
	long pte_root;
	long nte_root;
	long qte_root;
	long qte_hash;
	int qte_free;
	long sgte_root;
	long sgte_hash;
	int sgte_free;
	long ste_root;
	long ste_hash;
	int ste_free;
	long scte_root;
	long scte_hash;
	int scte_free;
	long rte_root;
	int rte_free;
	int rte_use;
	long misc_root;
};

struct sg_bboard_t
{
	sg_bbparms_t bbparms;	/* static stuff */
	bi::interprocess_upgradable_mutex bb_mutex;
	bi::interprocess_mutex sys_mutex;
	pid_t bblock_pid;
	pid_t syslock_pid;
	sg_bbmeters_t bbmeters;	/* occupancy stats */
	sg_bbmap_t bbmap;		/* navigation map */

	sg_bboard_t(const sg_bbparms_t& parms_);
	~sg_bboard_t();

	static size_t bbsize(const sg_bbparms_t& bbparms);
	static size_t pbbsize(const sg_bbparms_t& bbparms);
};

template<typename T>
class sg_entry_manage
{
public:
	// 从空闲列表获取一个节点
	template<typename MT>
	static T * get(MT *mgr, int& free_offset, int& usecnt, int& maxcnt)
	{
		if (free_offset == -1)
			throw sg_exception(__FILE__, __LINE__, SGEOS, 0, (_("ERROR: Out of memory")).str(SGLOCALE));

		T *entry = mgr->link2ptr(free_offset);
		free_offset = entry->hashlink.fhlink;
		if (++usecnt > maxcnt)
			maxcnt = usecnt;
		return entry;
	}

	// 释放节点到空闲列表
	template<typename MT>
	static void put(MT *mgr, T *entry, int& free_offset, int& usecnt)
	{
		entry->hashlink.sgid = BADSGID;
		entry->hashlink.fhlink = free_offset;
		free_offset = mgr->ptr2link(entry);
		--usecnt;
	}

	// 把节点加入hash列表中
	template<typename MT>
	static void insert_hash(MT *mgr, T *entry, int& hash_root)
	{
		if (hash_root == -1) {
			entry->hashlink.bhlink = -1;
			entry->hashlink.fhlink = -1;
			hash_root = entry->hashlink.rlink;
		} else {
			T *root = mgr->link2ptr(hash_root);
			root->hashlink.bhlink = entry->hashlink.rlink;
			entry->hashlink.bhlink = -1;
			entry->hashlink.fhlink = hash_root;
			hash_root = entry->hashlink.rlink;
		}
	}

	// 从hash列表中删除
	template<typename MT>
	static void erase_hash(MT *mgr, T *entry, int& hash_root)
	{
		if (entry->hashlink.bhlink == -1) {
			hash_root = entry->hashlink.fhlink;
		} else {
			T *prev = mgr->link2ptr(entry->hashlink.bhlink);
			prev->hashlink.fhlink = entry->hashlink.fhlink;
		}

		if (entry->hashlink.fhlink != -1) {
			T *next = mgr->link2ptr(entry->hashlink.fhlink);
			next->hashlink.bhlink = entry->hashlink.bhlink;
		}
	}
};

// MACHINE parameters
struct sg_mchent_t
{
	char pmid[MAX_IDENT + 1];			// 物理主机名
	char lmid[MAX_IDENT + 1];			// 逻辑主机名
	char sgdir[MAX_SGROOT + 1];			// SGROOT
	char appdir[MAX_APPROOT + 1];		// APPROOT
	char sgconfig[MAX_SGCONFIG + 1];	// SGPROFILE
	char clopt[MAX_LSTRING + 1];		// CLOPT for sggws
	char envfile[MAX_LSTRING + 1];		// ENVFILE
	char ulogpfx[MAX_LSTRING + 1];		// 日志前缀
	int mid;							// machine id
	int flags;							// 状态
	int maxaccsrs;						// max accessors for pe
	int perm;						// 访问模式
	uid_t uid;							// 用户归属
	uid_t gid;							// 组归属
	long netload;						// 调用服务需要的网络开销
	int maxconn;						// 最大允许的连接数
};

struct sg_pte_t
{
	int flags;					/* misc flags */
	int link;					/* index of next NodePeTbl enty */
	int nlink;					/* index of NodeTbl enty */
	int mid;					/* Full machine id for this entry */
	int maxaccsrs;				/* max accessors for pe */
	char pname[MAX_IDENT + 1];	/* physical name */
	char lname[MAX_IDENT + 1];	/* logical name */
	int perm;
	int uid;
	int gid;
	long netload;				/* Netload going out from here */
	int maxconns;
};

// NETWORK parameters
struct sg_netgroup_t
{
	char netgroup[MAX_IDENT + 1];
	int ng_id;
	int ng_priority;
};

const char NM_DEFAULTNET[] = "DEFAULTNET";

struct sg_netent_t
{
	char lmid[MAX_IDENT + 1];
	char naddr[MAX_STRING + 1];
	char nlsaddr[MAX_STRING + 1];
	char netgroup[MAX_IDENT + 1];
	bool tcpkeepalive;
	bool nodelay;
};

struct sg_nte_t
{
	int flags;
	int link;					// 指向PE节点头
	int nprotocol;
	char pnode[MAX_IDENT + 1];
};

// QUEUE parameters
struct sg_queparms_t
{
	int options;					// 进程选项
	int grace;						// 进程重启时间间隔
	int maxgen;						// 最大重启次数
	int paddr;						// 消息队列索引
	char saddr[MAX_IDENT + 1];		// 请求消息队列逻辑地址
	char rcmd[MAX_LSTRING + 1];		// 重启命令
	char filename[MAX_LSTRING + 1];	// 可执行文件名称
};

struct sg_lqueue_t
{
	int totnqueued;		/* avg queue length */
	int nqueued;			// 队列长度
	long totwkqueued;		/* avg queue workload */
	long wkqueued;			// 队列负载

	sg_lqueue_t();
};

struct sg_gqueue_t
{
	int status;			// 队列状态
	int clink;			// 指向第一个进程
	int svrcnt;			// 进程数
	int spawn_state;	// spawn状态

	sg_gqueue_t();
};

struct sg_qte_t
{
	sg_hashlink_t hashlink;
	sg_queparms_t parms;
	sg_lqueue_t local;
	sg_gqueue_t global;

	sg_qte_t();
	// 在共享内存初始化时由BBM调用
	sg_qte_t(int rlink);
	// 在QTE节点创建时调用
	sg_qte_t(const sg_queparms_t& parms_);
	~sg_qte_t();

	sgid_t& sgid() { return hashlink.sgid; }
	const sgid_t& sgid() const { return hashlink.sgid; }
	int& rlink() { return hashlink.rlink; }
	const int& rlink() const { return hashlink.rlink; }
	int& paddr() { return parms.paddr; }
	const int& paddr() const { return parms.paddr; }
	char * saddr() { return parms.saddr; }
	const char * saddr() const { return parms.saddr; }
};

// SERVER GROUP parameters
int const MAXLMIDSINLIST = 2;

struct sg_sgparms_t
{
	char sgname[MAX_IDENT + 1];		// server group name
	int grpid;						// server group id
	int midlist[MAXLMIDSINLIST];	// machine group may reside on
	int curmid;
};

struct sg_sgent_t
{
	int flags;
	int grpid;
	int curmid;
	char sgname[MAX_IDENT + 1];	// server group name
	char lmid[MAXLMIDSINLIST][MAX_IDENT + 1];	// LMID values
	char envfile[MAX_LSTRING + 1];	// ENVFILE
};

struct sg_sgte_t
{
	sg_hashlink_t hashlink;
	sg_sgparms_t parms;

	sg_sgte_t();
	// 在共享内存初始化时由BBM调用
	sg_sgte_t(int rlink);
	// 在SGTE节点创建时调用
	sg_sgte_t(const sg_sgparms_t& parms_);
	~sg_sgte_t();

	int curmid() const { return parms.midlist[parms.curmid]; }
	int& curmid() { return parms.midlist[parms.curmid]; }
};

// SERVER parameters
struct sg_svrparms_t
{
	sg_queparms_t rqparms;			// request queue parameters
	sg_proc_t svrproc;				// server's process id
	int rqperm;						// request queue permissions
	int rpperm;						// reply queue permissions
	int sequence;					// 启动/停止的序列号
	int svridmax;					// 创建进程的最大svrid
	time_t ctime;					// 启动时间
	size_t max_num_msg;				// 队列最大消息数
	size_t max_msg_size;			// 消息最大字节数
	size_t cmprs_limit;				// 压缩阀值
	int remedy;						// 多少次调用后进行修正
	char sgname[MAX_IDENT + 1];		// server group name
};

struct sg_bootparms_t
{
	int flags;
	int bootmin;					// minimum server to boot
	char clopt[MAX_LSTRING + 1];	// command line option string
	char envfile[MAX_LSTRING + 1];	// name of startup environment file
};

struct sg_svrent_t
{
	sg_svrparms_t sparms;
	sg_bootparms_t bparms;

	bool operator<(const sg_svrent_t& rhs) const {
		if (sparms.sequence < rhs.sparms.sequence)
			return true;
		else if (sparms.sequence > rhs.sparms.sequence)
			return false;

		if (sparms.svrproc.grpid < rhs.sparms.svrproc.grpid)
			return true;
		else if (sparms.svrproc.grpid > rhs.sparms.svrproc.grpid)
			return false;

		return sparms.svrproc.svrid < rhs.sparms.svrproc.svrid;
	}
};

struct sg_nserver_t
{
	int total_idle;		// 空闲服务数量
	int total_busy;		// 忙服务数量
	long ncompleted;	// 完成请求数
	long wkcompleted;	// 总完成负载数
};

struct sg_lserver_t
{
	int total_idle;		// 空闲服务数量
	int total_busy;		// 忙服务数量
	long ncompleted;	// 完成请求数
	long wkcompleted;	// 总完成负载数

	sg_proc_t currclient;				/* pid of current client */
	int currrtype;						/* reply type of current request */
	int currriter;						/* reply iteration of current request */
	int curroptions;					/* options on current request */
	int currmsgid;						/* user sequence # */
	char currservice[MAX_SVC_NAME + 1];	/* active service */

	sg_lserver_t();
};

struct sg_gserver_t
{
	sg_proc_t svrproc;	// server的process信息
	int maxmtype;		// maximum message type to dequeue
	time_t ctime;		// 创建时间
	int status;			// 当前服务状态
	time_t mtime;		// 最近一次更新时间
	int srelease;		// release no
	int gen;			// generation
	int svridmax;		// 创建进程的最大svrid

	sg_gserver_t();
};

struct sg_ste_t
{
	sg_hashlink_t hashlink;
	sg_famlink_t family;
	sg_quelink_t queue;
	sg_lserver_t local;
	sg_gserver_t global;

	sg_ste_t();
	// 在共享内存初始化时由BBM调用
	sg_ste_t(int rlink);
	// 在STE节点创建时调用
	sg_ste_t(const sg_svrparms_t& parms);
	~sg_ste_t();

	// 比较两个STE节点是否一致
	bool operator==(const sg_ste_t& rhs) const;

	sgid_t& sgid() { return hashlink.sgid; }
	const sgid_t& sgid() const { return hashlink.sgid; }
	int& rlink() { return hashlink.rlink; }
	const int& rlink() const { return hashlink.rlink; }
	int& mid() { return global.svrproc.mid; }
	const int& mid() const { return global.svrproc.mid; }
	int& grpid() { return global.svrproc.grpid; }
	const int& grpid() const { return global.svrproc.grpid; }
	int& svrid() { return global.svrproc.svrid; }
	const int& svrid() const { return global.svrproc.svrid; }
	pid_t& pid() { return global.svrproc.pid; }
	const pid_t& pid() const { return global.svrproc.pid; }
	int& rqaddr() { return global.svrproc.rqaddr; }
	const int& rqaddr() const { return global.svrproc.rqaddr; }
	int& rpaddr() { return global.svrproc.rpaddr; }
	const int& rpaddr() const { return global.svrproc.rpaddr; }
	sg_proc_t& svrproc() { return global.svrproc; }
	const sg_proc_t& svrproc() const { return global.svrproc; }
};


// SERVICE parameters
struct sg_svcparms_t
{
	char svc_name[MAX_SVC_NAME + 1];	// 服务名称
	char svc_cname[MAX_CLASS_NAME + 1];	// 服务对象名称
	int svc_index;						// 服务对象索引
	int priority;						// 服务优先级
	int svc_policy;							// 服务调用策略
	long svc_load;						// 服务负载
	long load_limit;					// 调用本地服务的负载阀值
	int svctimeout;						// 服务超时
	int svcblktime;						// Per service block time

	bool admin_name() const { return svc_name[0] == '.'; }
	bool int_name() const { return (svc_name[0] == '.' && svc_name[1] == '.'); }
};

struct sg_svcent_t
{
	char svc_pattern[MAX_PATTERN + 1];	// 服务名称匹配规则
	int grpid;								// 进程归属的组
	int priority;							// 服务优先级
	int svc_policy;							// 服务调用策略
	long svc_load;							// 服务负载
	long load_limit;						// 调用本地服务的负载阀值
	int svctimeout;							// 服务超时
	int svcblktime;							// 服务调用阻塞时间
};

struct sg_lservice_t
{
	int ncompleted;		// 完成的服务数
	int nqueued;		// 入queue的请求数

	sg_lservice_t();
};

struct sg_gservice_t
{
	sg_proc_t svrproc;	// 服务的进程地址
	int status;			// 服务状态

	sg_gservice_t();
};

struct sg_scte_t
{
	sg_hashlink_t hashlink;
	sg_famlink_t family;
	sg_quelink_t queue;
	sg_svcparms_t parms;
	sg_lservice_t local;
	sg_gservice_t global;

	sg_scte_t();
	// 在共享内存初始化时由BBM调用
	sg_scte_t(int rlink);
	// 在SCTE节点创建时调用
	sg_scte_t(const sg_svcparms_t& parms_);
	~sg_scte_t();

	sgid_t& sgid() { return hashlink.sgid; }
	const sgid_t& sgid() const { return hashlink.sgid; }
	int& rlink() { return hashlink.rlink; }
	const int& rlink() const { return hashlink.rlink; }
	int& mid() { return global.svrproc.mid; }
	const int& mid() const { return global.svrproc.mid; }
	int& grpid() { return global.svrproc.grpid; }
	const int& grpid() const { return global.svrproc.grpid; }
	int& svrid() { return global.svrproc.svrid; }
	const int& svrid() const { return global.svrproc.svrid; }
	pid_t& pid() { return global.svrproc.pid; }
	const pid_t& pid() const { return global.svrproc.pid; }
	int& rqaddr() { return global.svrproc.rqaddr; }
	const int& rqaddr() const { return global.svrproc.rqaddr; }
	int& rpaddr() { return global.svrproc.rpaddr; }
	const int& rpaddr() const { return global.svrproc.rpaddr; }
	sg_proc_t& svrproc() { return global.svrproc; }
	const sg_proc_t& svrproc() const { return global.svrproc; }
};

// REGISTRY parameters
struct sg_handle_t
{
	short numghandle;		/* num of good replies outstanding */
	short numshandle;		/* num of stale replies outstanding */
	int gen;				/* generation, total # of restarts */
	int qaddr;				/* queue process is reading from */
	int bbscan_mtype[MAXHANDLES];		/* mtype process is waiting for */
	int bbscan_rplyiter[MAXHANDLES];	/* message reply iteration */
	unsigned int bbscan_timeleft[MAXHANDLES]; /* time left before timeout */
};

enum lock_type_t
{
	UNLOCK = 0x0,
	SHARABLE_BBLOCK = 0x1,
	EXCLUSIVE_BBLOCK = 0x2,
	EXCLUSIVE_SYSLOCK = 0x4
};

struct sg_rte_t
{
	int nextreg;
	int slot;			// 注册节点ID
	pid_t pid;			// 该节点归属的进程pid
	int mid;			// 节点所在主机
	int qaddr;			// 客户端消息队列地址
	int lastgrp;		// last group to which message sent
	char usrname[MAX_IDENT + 1];	// user name
	char cltname[MAX_IDENT + 1];	// client name
	char grpname[MAX_IDENT + 1];	// group name
	int rflags;				// internal flag bits
	time_t rtimestamp;		// timestamp
	int rstatus;			// entry status
	int rreqmade;			// # of requests made
	int lock_type;			// lock type for current entry
	int rt_svctimeout;		// Time left until svctimeout or 0
	sg_handle_t hndl;		// handle information
	int rt_timestamp;		// Client: time of last activity
	int rt_release;			// Client: software release of clt
	int rt_blktime;			// Client: Current blocktime
	int rt_svcexectime;     // service execution time
	int rt_grpid;			// Server group of assoc. WSL for WSH
	int rt_svrid;			// Server id of assoc. WSL for WSH

	sg_rte_t(int slot_);
	~sg_rte_t();
};

// MISC parameters
struct sg_qitem_t
{
	int rlink;		// 队列索引
	int flags;		// 标识
	int sequence;	// 当前序列号
	int qaddr;		// 队列ID
	pid_t pid;		// 创建者
	int prev;
	int next;

	void init(int rlink_) {
		rlink = rlink_;
		flags = 0;
		sequence = 0;
		qaddr = 0;
		pid = 0;
		prev = rlink - 1;
		next = rlink + 1;
	}
};

struct sg_misc_t
{
	int maxques;	// 总节点数
	int que_free;	// 空闲节点头
	int que_tail;	// 空闲节点尾
	int que_used;	// 使用节点头
	sg_qitem_t items[1];	// 节点数据开始
};

struct sg_queue_key_t
{
	int mid;
	int qid;

	bool operator<(const sg_queue_key_t& rhs) const {
		if (mid < rhs.mid)
			return true;
		else if (mid > rhs.mid)
			return false;

		return qid < rhs.qid;
	}

	bool operator==(const sg_queue_key_t& rhs) const {
		return (mid == rhs.mid && qid == rhs.qid);
	}
};

size_t hash_value(const sg_queue_key_t& item);

struct sg_queue_info_t
{
	int qaddr;
	queue_pointer queue;
};

typedef boost::unordered_map<sg_queue_key_t, sg_queue_info_t>::iterator queue_iterator;

struct sg_svcdsp_t
{
	int index;
	const char *svc_name;
	const char *class_name;
	sg_svc * (*instance)();
};

// INIT parameters
struct sg_init_t
{
	char usrname[MAX_IDENT + 1];	// client user name
	char cltname[MAX_IDENT + 1];	// application client name
	char passwd[MAX_IDENT + 1];		// application client password
	char grpname[MAX_IDENT + 1];	// client group name
	int flags;						// initialization flags

	sg_init_t();
};

// special context value that can be passed to set_ctx
int const SGNULLCONTEXT = -2;

/*
 * special context in which a thread is placed another thread
 * terminates the context in which the thread had been
 */
int const SGINVALIDCONTEXT = -1;

/*
 * context in which a process is placed when it sucessfully
 * executes init() without the SGMULTICONTEXTS flag.
 */
int const SGSINGLECONTEXT = 0;

template<typename VALUE, typename BASE>
VALUE sg_roundup(VALUE value, BASE base)
{
	return (value + base - 1) / base * base;
}

}
}

#endif

