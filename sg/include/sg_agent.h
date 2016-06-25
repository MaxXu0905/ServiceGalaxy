#if !defined(__SG_AGENT_H__)
#define __SG_AGENT_H__

#include "sg_struct.h"
#include "sg_message.h"
#include "sg_manager.h"
#include <boost/asio.hpp>

namespace basio = boost::asio;

namespace ai
{
namespace sg
{

/*
 * Tagent rpc reply error codes.
 */
int const TGESND = 1;		/* Error sending to tagent (set in _tmtagentsnd) */
int const TGERCV = 2;		/* Error receiving from tagent (ditto) */
int const TGESYNC = 3;		/* Error synchronizing network connection */
int const TGEBUF = 4;		/* Error allocating message buffer */
int const TGENET = 5;		/* Network access error in tagent */
int const TGEALLOC = 6;		/* Error allocating memory */
int const TGEPERM = 8;		/* Error setting uid/gid */
int const TGEVERS = 9;		/* Illegal version number on opcode */
int const TGEOPCODE = 11;	/* Illegal opcode error */
int const TGEINVAL = 12;	/* Invalid arguments for opcode */
int const TGEFILE = 13;		/* Error creating tmp file for propagation */
int const TGELOADCF = 14;	/* Error running sgpack(1/T) */
int const TGEENV = 15;		/* Error establishing environment */
int const TGEPARMS = 16;	/* Error getting app parms */
int const TGEREAD = 25;		/* Error reading file */
int const TGESEEK = 26;		/* Error seeking in file */

struct fdrel_t
{
	basio::ip::tcp::socket socket;
	char *bridge;
	string *nlsaddr;
	int got_ack;
	int tl_flags;
};

struct tasync_t {
	int flags;			/* _tmsync_proc() flags */
	sg_mchent_t mchent;	/* Servers *MACHINES entry */
	char argvs[1];		/* place holder for s<NULL>...s<NULL><NULL> */
};

/*
 * SYNCPROC reply data structure.
 */
struct tasyncr_t
{
	pid_t pid;	/* Pid of newly booted process */
	int spret;	/* return code from _tmsync_proc() call */
};

/*
 * SGPROP request data structure. There is no reply data structure other than
 * the standard rpc_rply structure. Only error codes are required for reply.
 */
struct taprop_t
{
	sg_mchent_t mchent;
	char passwd[MAX_PASSWD + 1];
	int size;
	char data[1];		/* File data */
};

struct tafmast_t
{
	int mid;
	int ipckey;
};

struct g_sctx_t
{
	int rqopcode;	/* Last rpopcode on this connection */
	int rprelease;	/* Release speaking to on this conn */
};

class sg_agent : public sg_manager
{
public:
	static sg_agent& instance(sgt_ctx *SGT, bool persist = true);

	sg_rpc_rply_t * send(int mid, int opcode, const void *dataptr, int datasize);
	void ta_process(basio::ip::tcp::socket& socket, sg_message& msg);
	void close(int mid);

private:
	sg_agent();
	virtual ~sg_agent();

	bool send_internal(int mid, int opcode);
	sg_rpc_rply_t * tag_err_ret(int error_code, int op, int rtn);
	bool run_one(basio::ip::tcp::socket& socket);
	void handle_timeout(const boost::system::error_code& error);
	void handle_connect(basio::ip::tcp::socket& socket, const boost::system::error_code& error);
	void handle_read_hdr(basio::ip::tcp::socket& socket, const boost::system::error_code& error);
	void handle_read_body(basio::ip::tcp::socket& socket, const boost::system::error_code& error);
	void exitmsg(basio::ip::tcp::socket& socket, int error, int xtra);
	void tamsg(int error, int xtra);
	int rplymsg(basio::ip::tcp::socket& socket, int error, const void *data, int datasz, int taerror, int uerr);
	void tasyncproc(basio::ip::tcp::socket& socket, tasync_t *args);
	void tafmaster(basio::ip::tcp::socket& socket, tafmast_t *args);
	void tasgprop(basio::ip::tcp::socket& socket, taprop_t *args);
	void tasgversion(basio::ip::tcp::socket& socket, sg_mchent_t *args);
	void tarelease(basio::ip::tcp::socket& socket);
	bool write_socket(basio::ip::tcp::socket& socket, const char *raw_data, size_t msg_size);

	bool persist;
	sg_netent_t rnte;
	sg_netent_t lnte;
	int rpe;
	int lpe;

	map<int, fdrel_t> fdrel;
	basio::io_service io_svc;
	bool timeout;
	bool handled;

	message_pointer rqst_msg;
	message_pointer rply_msg;
	g_sctx_t ctx;

	friend class sgt_ctx;
};

}
}

#endif

