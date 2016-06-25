#if !defined(__SG_RPC_H__)
#define __SG_RPC_H__

#include <limits>
#include "sg_struct.h"
#include "sg_message.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

enum sg_opcode_t
{
	O_BASE = 0,
	O_CPTE,
	O_CNTE,
	O_CQTE,
	O_DQTE,
	O_UQTE,
	O_RQTE,
	O_CSGTE,
	O_DSGTE,
	O_USGTE,
	O_RSGTE,
	O_CSTE,
	O_DSTE,
	O_RMSTE,
	O_USTE,
	O_RSTE,
	O_CSCTE,
	O_ASCTE,
	O_DSCTE,
	O_USCTE,
	O_RSCTE,
	O_OGSVCS,
	O_USTAT,
	O_VUSTAT,
	O_BBCLEAN,
	O_SNDSTAT,
	O_LANINC,
	O_RGSTR,
	O_URGSTR,
	O_ARGSTR,
	O_AURGSTR,
	O_BBMS,
	O_CDBBM,
	O_MIGDBBM,
	O_PSUSPEND,
	O_PUNSUSPEND,
	O_PCLEAN,
	O_UDBBM,
	O_RRTE,
	O_UPLOAD,
	O_REFRESH,
	O_IMOK,
	O_UPLOCAL,

	// Define BRIDGE specific opcodes.
	OB_BASE = 200,
	OB_EXIT,
	OB_FINDDBBM,
	OB_RKILL,
	OB_SUSPEND,
	OB_UNSUSPEND,
	OB_QCTL,
	OB_STAT,
	OB_PCLEAN,

	// Define tagent specific opcodes.
	OT_BASE = 300,
	OT_FMASTER,
	OT_RELEASE,
	OT_CHECKALIVE,
	OT_EXIT,
	OT_SGVERSION,
	OT_SGPROP,
	OT_SYNCPROC
};

int const RPC_FLAGS_FULL_SYNC = 0x1;

//RPC_RQUST arg_3 flags bits
int const RQF_TMIB_RPLY = 0x00000001;		/* Reply with TMIB FML32 buffer */
int const KILL_WITH_BBLOCK = 0x00000002;	/* for OB_RKILL, lock BB */

// rpc reply flag bits
int const SGRPTABENTS = 0x00000001;	/* table entries in reply */
int const SGRPSGIDS = 0x00000002;	/* SGIDs in reply */

// O_CDBBM flag bits
int const DR_TEMP = 0x00000001;	/* Create temporary sgclst */

int const MAXRPC = 4096;		/* max rpc request/reply data sent via message */

struct sg_rpc_rqst_t
{
	int opcode;
	int datalen;
	union arg_1 {		/* The declaration of these unions	*/
		int scope;	/* takes advantage of the fact that	*/
		int flag;	/* TMID, MCHID, and ADDR are typedef	*/
		sgid_t sgid;	/* TM32I, thus simplifying encoding.	*/
		int size;	/* Should these definitions change,	*/
		int from;	/* either this structure will have to	*/
		int addr;	/* change or the encode/decode of	*/
		int sig;	/* rpc_rqust must change.		*/
		int count;
		int host;
		int cltslot;	/* client slot */
		int vers;	/* version timestamp */
	} arg1;
	union arg_2 {
		int tabents;
		int status;
		int to;
		int count;
		int cmd;
		int uid;	/* needs to be TM32I (!uid_t) for encoding */
		int gwflags;	/* gateway server flag used with OG_STAT */
		int clttmstmp; /* client timestamp */
		int vers;	/* version timestamp */
		int serial1;	/* high-order part of serial number */
	} arg2;
	union arg_3 {
		int flags;
		int sgids;
		int gid;	/* needs to be TM32I (!gid_t) for encoding */
		int grpid;
		int origiter;	/* originator iteration for conversation */
		int serial2;	/* low-order part of serial number */
		int usrcnt;	/* active user count */
	} arg3;
	long placeholder;

	const char * data() const;
	char * data();
	static size_t need(int size) { return (sizeof(sg_rpc_rqst_t) - sizeof(long) + size); }
};

struct sg_rpc_rply_t
{
	int opcode;
	int error_code;
	int rtn;
	int num;
	int flags;
	int datalen;
	long placeholder;

	const char * data() const;
	char * data();
	static size_t need(int size) { return (sizeof(sg_rpc_rply_t) - sizeof(long) + size); }
};

class sg_rpc : public sg_manager
{
public:
	static sg_rpc& instance(sgt_ctx *SGT);

	// 初始化RPC请求
	void init_rqst_hdr(message_pointer& msg);
	// 创建消息内存
	message_pointer create_msg(size_t data_size);
	// 发送消息，并同步等待返回sgid
	sgid_t tsnd(message_pointer& msg);
	int asnd(message_pointer& msg);
	bool send_msg(message_pointer& msg, sg_proc_t *where, int flags);

	int dsgid(int type, sgid_t *sgids, int cnt);

	template<typename T>
	int rtbl(int opcode, int scope, const T *key, T *ents, sgid_t *sgids)
	{
		int tblarg = 0;
		int sgidarg = 0;
		size_t rqsz;

		switch (scope) {
		case S_ANY:
		case S_SVRID:
		case S_GRPID:
		case S_GROUP:
		case S_GRPNAME:
		case S_QUEUE:
		case S_MACHINE:
			if (!key) {
				if (SGT->_SGT_error_code == 0) {
					SGT->_SGT_error_code = SGESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Missing search key")).str(SGLOCALE));
				}
				return -1;
			}
			if (ents)
				tblarg = 1;
			if (sgids)
				sgidarg = 1;
			rqsz = sizeof(T);
			break;
		case S_BYSGIDS:
			if (!ents || !sgids) {
				if (SGT->_SGT_error_code == 0) {
					SGT->_SGT_error_code = SGESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Missing search key")).str(SGLOCALE));
				}
				return -1;
			}

			if (!key) {
				sgidarg = 1;
				for (const sgid_t *sgid = sgids; *sgid != BADSGID; ++sgid)
					sgidarg++;
				if (sgidarg < 2) {
					if (SGT->_SGT_error_code == 0) {
						SGT->_SGT_error_code = SGESYSTEM;
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid search key")).str(SGLOCALE));
					}
					return -1;
				}
			} else {
				sgidarg = 1;
			}
			tblarg = 1;
			rqsz = sgidarg * sizeof(sgid_t);
			break;
		case S_BYRLINK:
			if (!ents || sgids) {
				if (SGT->_SGT_error_code == 0) {
					SGT->_SGT_error_code = SGESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Missing search key")).str(SGLOCALE));
				}
				return -1;
			}

			if (key) {
				tblarg = 1;
				sgidarg = 1;
				rqsz = sizeof(T);
			} else {
				tblarg = 1;
				for (const T *entp = ents; entp->hashlink.rlink != -1; entp++)
					tblarg++;
				if (tblarg < 2) {
					if (SGT->_SGT_error_code == 0) {
						SGT->_SGT_error_code = SGESYSTEM;
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid search key")).str(SGLOCALE));
					}
					return -1;
				}
				rqsz = (tblarg - 1) * sizeof(T) + sizeof(sg_hashlink_t);
			}
			break;
		default:
			if (SGT->_SGT_error_code == 0) {
				SGT->_SGT_error_code = SGESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: [Parameter error in internal routine]")).str(SGLOCALE));
			}
			return -1;
		}

		message_pointer msg = create_msg(sg_rpc_rqst_t::need(rqsz));
		if (msg == NULL) {
			// error_code already set
			return BADSGID;
		}

		sg_rpc_rqst_t *rqst = msg->rpc_rqst();
		rqst->opcode = opcode | VERSION;
		rqst->arg1.scope = scope;
		rqst->arg2.tabents = tblarg;
		rqst->arg3.sgids = sgidarg;
		rqst->datalen = rqsz;

		if (tblarg > 1)
			memcpy(rqst->data(), ents, rqsz);
		else if (scope == S_BYSGIDS)
			memcpy(rqst->data(), sgids, rqsz);
		else
			memcpy(rqst->data(), key, rqsz);

		sgc_ctx *SGC = SGT->SGC();
		if (!send_msg(msg, SGC->_SGC_mbrtbl_raddr, SGAWAITRPLY | SGSIGRSTRT))
			return -1;

		sg_rpc_rply_t *rply = msg->rpc_rply();
		if (rply->rtn < 0) {
			SGT->_SGT_error_code = rply->error_code;
			if (SGT->_SGT_error_code == SGEOS) {
				sg_msghdr_t& msghdr = *msg;
				SGT->_SGT_native_code = msghdr.native_code;
			}
			return -1;
		}

		int tblsz = 0;
		int sgidsz = 0;
		char *tblp = NULL;
		char *sgdp = NULL;

		if (tblarg > 0)
			tblsz = rply->rtn * sizeof(T);

		if (sgidarg == 1 && scope != S_BYRLINK && scope != S_BYSGIDS)
			sgidsz = rply->rtn * sizeof(sgid_t);

		if (tblarg > 0) {
			tblp = rply->data();
			memcpy(ents, tblp, tblsz);
		}

		if (sgidsz) {
			if (tblp)	 /* sgids follow table entries */
				sgdp = tblp + tblsz;
			else		 /* only sgids */
				sgdp = rply->data();
			memcpy(sgids, sgdp, sgidsz);
		}

		return rply->rtn;
	}

	// 设置节点状态
	int ustat(sgid_t *sgids, int cnt, int status, int flag);
	int smustat(sgid_t *sgids, int cnt, int status, int flag);
	int mbustat(sgid_t *sgids, int cnt, int status, int flag);
	// 更新BB版本
	int ubbver();
	int smubbver();
	int nullubbver();
	// 更新LAN版本
	int laninc();
	int smlaninc();
	int mblaninc();
	// 设置状态
	bool set_stat(sgid_t sgid, int status, int flags);
	void setraddr(const sg_proc_t *up);
	void setuaddr(const sg_proc_t *up);
	int cdbbm(int mid, int flags);
	int nwsuspend(int *onpes, int count);
	int nwunsuspend(int *onpes, int count);

private:
	sg_rpc();
	virtual ~sg_rpc();

	int tobridges(int opcode, int *onpes, int count);
	int tohost(int opcode, int mid);
	int findgws();

	friend class sgt_ctx;
};

}
}

#endif

