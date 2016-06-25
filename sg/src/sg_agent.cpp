#include "sg_internal.h"

namespace bp = boost::posix_time;
namespace basio = boost::asio;

namespace ai
{
namespace sg
{

sg_agent& sg_agent::instance(sgt_ctx *SGT, bool persist)
{
	sg_agent& agent_mgr = *reinterpret_cast<sg_agent *>(SGT->_SGT_mgr_array[AGENT_MANAGER]);
	agent_mgr.persist = persist;
	return agent_mgr;
}

sg_rpc_rply_t * sg_agent::send(int mid, int opcode, const void *dataptr, int datasize)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_config& config_mgr = sg_config::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("mid={1}, opcode={2}, datasize={3}") % mid % opcode % datasize).str(SGLOCALE), NULL);
#endif

	SGT->_SGT_error_code = 0;

	/*
	 *  make sure that the arguments are kosher before we go to far.
	 */
	if ((dataptr == NULL && datasize > 0) || SGC->_SGC_bbp == NULL
		|| mid < 0 || SGC->midpidx(mid) >= SGC->MAXPES()
		|| (SGC->_SGC_ptes[SGC->midpidx(mid)].flags & (NP_UNUSED|NP_INVALID))) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Illegal arguments for send to AGENT on {1}")
			% SGC->mid2lmid(mid)).str(SGLOCALE));
		return tag_err_ret(SGT->_SGT_error_code, TGESND, -1);
	}

	if (opcode != OT_CHECKALIVE) {
		/*
		 * First allocate and populate the message to be sent to the
		 * agent. Note that "need" represents the total data size
		 * including the RPC_RQUST structure itself whereas datasize
		 * represents only the opcode specific data.
		 */
		size_t need = sg_rpc_rqst_t::need(datasize);
		if (rqst_msg == NULL)
			rqst_msg = rpc_mgr.create_msg(need);
		else
			rqst_msg->set_length(need);

		sg_metahdr_t *metahdr = *rqst_msg;
		metahdr->flags = 0;

		sg_rpc_rqst_t *rqst = rqst_msg->rpc_rqst();
		rqst->opcode = opcode | VERSION;
		rqst->datalen = datasize;
		rqst->arg1.scope = SGPROTOCOL;

		// uid and gid et below
		if (dataptr != NULL) // some opcodes may have no data
			memcpy(rqst->data(), dataptr, datasize);
	}

	/*
	 * At this point we need to get information about the local
	 * endpoint in order to process error conditions, even if
	 * have already connected to the tlisten process.
	 * First find a PE entry for the remote node that is a network entry
	 * AND has an AGTADDR specified for contacting the NLS process.
	 */
	rpe = SGC->_SGC_ntes[SGC->midnidx(mid)].link;

	while (1) {
		for (; rpe != -1; rpe = SGC->_SGC_ptes[rpe].link) {
			if (!(SGC->_SGC_ptes[rpe].flags & NP_NETWORK)
				|| (SGC->_SGC_ptes[rpe].flags & NP_NLSERR)) {
				continue;
			}

			strcpy(rnte.lmid, SGC->_SGC_ptes[rpe].lname);
			strcpy(rnte.netgroup, NM_DEFAULTNET);
			if (!config_mgr.find(rnte) || rnte.nlsaddr[0] == '\0')
				continue;

			/*
			 * Now look for a local pe that has the same NETID value as
			 * this remote PE. Then we can use that local pe to access the
			 * remote NLS/agent.
			 */
			for (lpe = SGC->_SGC_ntes[SGC->midnidx(SGC->_SGC_proc.mid)].link; lpe != -1; lpe = SGC->_SGC_ptes[lpe].link) {
				if (!(SGC->_SGC_ptes[lpe].flags & NP_NETWORK))
					continue;

				strcpy(lnte.lmid, SGC->_SGC_ptes[lpe].lname);
				strcpy(lnte.netgroup, NM_DEFAULTNET);
				if (!config_mgr.find(lnte))
					continue;

				break;
			}
			if (lpe != -1)
				break;
		}

		if (rpe == -1) {
			if (SGT->_SGT_error_code == 0)
				SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: No AGENT available for remote node {1}")
				% SGC->mid2lmid(mid)).str(SGLOCALE));
			return tag_err_ret(SGT->_SGT_error_code, TGESND, -1);
		}

		if (send_internal(mid, opcode))
			return rply_msg->rpc_rply();

		break;
	}

	return NULL;
}

/*
 * agent - This program is the /T remote agent for network processing. It is used
 * mostly for booting operations requiring network access. Specific opcodes are
 * handled within the routine ta_process().
 */
void sg_agent::ta_process(basio::ip::tcp::socket& socket, sg_message& msg)
{
	int rval = 1;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif
	sg_rpc_rqst_t *rqst = msg.rpc_rqst();

	ctx.rqopcode = rqst->opcode & ~VERSMASK;

	// remember the release to reply by
	ctx.rprelease = rqst->arg1.scope;
	if (ctx.rprelease > SGPROTOCOL)
		ctx.rprelease = SGPROTOCOL;

	/*
	 * Now check the version part of the opcode to make sure we're on the
	 * right release.
	 */
	if ((rqst->opcode & VERSMASK) != VERSION) {
		exitmsg(socket, TGEVERS, 0);
		return;
	}

	/*
	 * Now switch based on the opcode
	 */
	switch(ctx.rqopcode) {
	case OT_SYNCPROC:
		tasyncproc(socket, reinterpret_cast<tasync_t *>(rqst->data()));
		break;
	case OT_FMASTER:
		tafmaster(socket, reinterpret_cast<tafmast_t *>(rqst->data()));
		break;
	case OT_SGPROP:
		tasgprop(socket, reinterpret_cast<taprop_t *>(rqst->data()));
		break;
	case OT_SGVERSION:
		tasgversion(socket, reinterpret_cast<sg_mchent_t *>(rqst->data()));
		break;
	case OT_RELEASE:
		tarelease(socket);
		break;
	case OT_EXIT:
		rplymsg(socket, 0, NULL, 0, 0, 0);
		sleep(10);
		kill(getpid(), SIGTERM);
		break;	/* NOT REACHED */
	default:
		tamsg(TGEOPCODE, ctx.rqopcode);
		rval = rplymsg(socket, SGESYSTEM, NULL, 0, TGEOPCODE, 0);
		break;
	}

	if (rval == -1)
		socket.close();
}

void sg_agent::close(int mid)
{
	sgc_ctx *SGC = SGT->SGC();
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("mid={1}") % mid).str(SGLOCALE), NULL);
#endif
	map<int, fdrel_t>::iterator iter;

	// If mid is ALLMID, then loop through entries to disconnect
	if (mid == ALLMID) {
		for (iter = fdrel.begin(); iter != fdrel.end(); ++iter) {
			fdrel_t& item = iter->second;
			item.socket.close();
		}
		fdrel.clear();
	} else {
		if (mid < 0 || SGC->midpidx(mid) >= SGC->MAXPES())
			return;

		iter = fdrel.find(SGC->midnidx(mid));
		if (iter != fdrel.end()) {
			fdrel_t& item = iter->second;
			item.socket.close();
			fdrel.erase(iter);
		}
	}
}

sg_agent::sg_agent()
{
}

sg_agent::~sg_agent()
{
}

bool sg_agent::send_internal(int mid, int opcode)
{
	// Attempt to connect to the NLS process on the remote site.
	sgc_ctx *SGC = SGT->SGC();
	basio::ip::tcp::socket socket(io_svc);
	string nlsaddr = rnte.nlsaddr;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("mid={1}") % mid).str(SGLOCALE), &retval);
#endif

	if (strncmp(nlsaddr.c_str(), "//", 2) != 0) {
		SGT->_SGT_error_code = SGEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid agtaddr {1} given") % nlsaddr).str(SGLOCALE));
		return retval;
	}

	string::size_type pos = nlsaddr.find(':');
	if (pos == string::npos) {
		SGT->_SGT_error_code = SGEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid agtaddr {1} given") % nlsaddr).str(SGLOCALE));
		return retval;
	}

	string host(nlsaddr.begin() + 2, nlsaddr.begin() + pos);
	string port(nlsaddr.begin() + pos + 1, nlsaddr.end());

	// 发起连接
	basio::ip::tcp::resolver resolver(io_svc);
	basio::ip::tcp::resolver::query query(host, port);
	basio::ip::tcp::resolver::iterator resolve_iter = resolver.resolve(query);
	basio::async_connect(socket, resolve_iter,
		boost::bind(&sg_agent::handle_connect, this, boost::ref(socket), basio::placeholders::error));

	if (!run_one(socket))
		return retval;

	if (opcode == OT_CHECKALIVE) {
		tag_err_ret(0, 0, 1);
		retval = true;
		return retval;
	}

	SGC->_SGC_ntes[SGC->midnidx(mid)].nprotocol = SGPROTOCOL;

	sg_rpc_rqst_t *rqst = rqst_msg->rpc_rqst();
	rqst->arg2.uid = SGC->_SGC_ptes[SGC->midpidx(mid)].uid;
	rqst->arg3.gid = SGC->_SGC_ptes[SGC->midpidx(mid)].gid;

	rqst_msg->encode();
	if (!write_socket(socket, rqst_msg->raw_data(), rqst_msg->size())) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Error sending to AGENT on {1}") % SGC->mid2lmid(mid)).str(SGLOCALE));

		if (persist && strcmp(SGC->_SGC_authsw->name, "SGBOOT") == 0)
			SGC->_SGC_ptes[rpe].flags |= NP_NLSERR;
		rpe = SGC->_SGC_ptes[rpe].link;
		return retval;
	}

	/*
	 * OK, so far so good. We've sent the message successfully, now its
	 * time to wait for the reply from the agent.
	 */
	if (rply_msg == NULL)
		rply_msg = sg_message::create(SGT);

	basio::async_read(socket, basio::buffer(rply_msg->raw_data(), MSGHDR_LENGTH),
		boost::bind(&sg_agent::handle_read_hdr, this, boost::ref(socket), basio::placeholders::error));

	if (!run_one(socket))
		return retval;

	sg_metahdr_t& metahdr = *rply_msg;
	size_t data_len = metahdr.size - MSGHDR_LENGTH;
	rply_msg->reserve(data_len);
	basio::async_read(socket, basio::buffer(rply_msg->raw_data() + MSGHDR_LENGTH, data_len),
		boost::bind(&sg_agent::handle_read_body, this, boost::ref(socket), basio::placeholders::error));

	if (!run_one(socket))
		return retval;

	retval = true;
	return retval;
}

sg_rpc_rply_t * sg_agent::tag_err_ret(int error_code, int op, int rtn)
{
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("error_code={1}, op={2}, rtn={3}") % error_code % op % rtn).str(SGLOCALE), NULL);
#endif

	if (rply_msg == NULL)
		rply_msg = rpc_mgr.create_msg(sg_rpc_rply_t::need(MAXRPC));

	sg_rpc_rply_t *rply = rply_msg->rpc_rply();
	rply->error_code = error_code;
	rply->num = op;
	rply->rtn = rtn;

	return rply;
}

bool sg_agent::run_one(basio::ip::tcp::socket& socket)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	timeout = false;
	basio::deadline_timer timer(io_svc);
	timer.expires_from_now(bp::seconds(bbp->bbparms.init_wait));
	timer.async_wait(boost::bind(&sg_agent::handle_timeout, this, basio::placeholders::error));

	BOOST_SCOPE_EXIT((&timer)) {
		timer.cancel();
	} BOOST_SCOPE_EXIT_END

	handled = false;
	do {
		io_svc.run_one();

		if (timeout) {
			if (SGT->_SGT_error_code == 0)
				SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not contact AGENT on {1}") % rnte.lmid).str(SGLOCALE));

			if (persist && strcmp(SGC->_SGC_authsw->name, "SGBOOT") == 0)
				SGC->_SGC_ptes[rpe].flags |= NP_NLSERR;
			rpe = SGC->_SGC_ptes[rpe].link;
			return retval;
		}
	} while (!handled);

	if (!socket.is_open())
		return retval;

	retval = true;
	return retval;
}

void sg_agent::handle_timeout(const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("error={1}") % error).str(SGLOCALE), NULL);
#endif

	if (!error)
		timeout = true;
}

void sg_agent::handle_connect(basio::ip::tcp::socket& socket, const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("error={1}") % error).str(SGLOCALE), NULL);
#endif

	if (!error) {
		basio::socket_base::keep_alive option1(true);
		socket.set_option(option1);

		basio::ip::tcp::no_delay option2(true);
	    socket.set_option(option2);
	} else {
		if (error != basio::error::eof)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: connect error, {1}") % error).str(SGLOCALE));
		socket.close();
	}

	handled = true;
}

void sg_agent::handle_read_hdr(basio::ip::tcp::socket& socket, const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("error={1}") % error).str(SGLOCALE), NULL);
#endif

	if (error) {
		if (error != basio::error::eof)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: connect error, {1}") % error).str(SGLOCALE));
		socket.close();
	}

	handled = true;
}

void sg_agent::handle_read_body(basio::ip::tcp::socket& socket, const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("error={1}") % error).str(SGLOCALE), NULL);
#endif

	if (error) {
		if (error != basio::error::eof)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: connect error, {1}") % error).str(SGLOCALE));
		socket.close();
	}

	handled = true;
}

void sg_agent::exitmsg(basio::ip::tcp::socket& socket, int error, int xtra)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("error={1}, xtra={2}") % error % xtra).str(SGLOCALE), NULL);
#endif

	tamsg(error, xtra);
	if (rplymsg(socket, SGESYSTEM, NULL, 0, error, 0) == 0) {
		// Allow for the network message to get there
		sleep(10);
	}
}

void sg_agent::tamsg(int error, int xtra)
{
	switch (error) {
	case TGEBUF:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure for receive buffer")).str(SGLOCALE));
		break;
	case TGENET:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Network access error")).str(SGLOCALE));
		break;
	case TGEALLOC:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failure")).str(SGLOCALE));
		break;
	case TGEPERM:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not set uid/gid for request")).str(SGLOCALE));
		break;
	case TGEVERS:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid version number for request")).str(SGLOCALE));
		break;
	case TGEOPCODE:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Illegal opcode ({1}) sent to agent") % xtra).str(SGLOCALE));
		break;
	case TGEINVAL:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid arguments for opcode")).str(SGLOCALE));
		break;
	case TGEFILE:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not creat temp file for SGPROFILE")).str(SGLOCALE));
		break;
	case TGELOADCF:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgpack failed when creating new configuration file")).str(SGLOCALE));
		break;
	case TGEENV:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not set environment variable")).str(SGLOCALE));
		break;
    case TGEPARMS:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not retrieve parameters from SGPROFILE file")).str(SGLOCALE));
		break;
    default:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown error, terminating")).str(SGLOCALE));
		break;
	}
}

int sg_agent::rplymsg(basio::ip::tcp::socket& socket, int error, const void *data, int datasz, int taerror, int uerr)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(100, __PRETTY_FUNCTION__, (_("error={1}, datasz={2}, taerror={3}, uerr={4}") % error % datasz % taerror % uerr).str(SGLOCALE), &retval);
#endif

	/*
	 * If there was no incoming RPC, we can't possibly respond.
	 */
	if (ctx.rqopcode < 0) {
		retval = 0;
		return retval;
	}

	/*
	 * Figure out how much data space we need. Its the size of the
	 * size of the standard rpc_rply struct plus the size of our data
	 * minus the size of the dataloc space holder.
	 */
	size_t need = sg_rpc_rply_t::need(datasz);
	if (rply_msg == NULL)
		rply_msg = sg_message::create(SGT);
 	rply_msg->set_length(need);

	/*
	 * Now we're sure we've got enough space for the unencoded version.
	 * Let's fill it up with the needed data. All that's needed in the
	 * header is the appropriate flags, the total message size and the
	 * datalen.
	 */
	sg_metahdr_t& metahdr = *rply_msg;
	metahdr.flags = 0;

	/*
	 * Now fill in the rpc_rply information. This includes the opcode which
	 * is used for decoding/encoding purposes, the error code passed to
	 * us, a return code (-1 if a non-zero error is being returned and
	 * 0 otherwise).
	 */
	sg_rpc_rply_t *rply = rply_msg->rpc_rply();
	rply->opcode = ctx.rqopcode | VERSION;
	rply->error_code = error;
	rply->rtn = error ? -1 : 0;
	rply->num = taerror;
	rply->datalen = datasz;

	sg_msghdr_t& msghdr = *rply_msg;
	msghdr.native_code = uerr;

	if (data != NULL)
		memcpy(rply->data(), data, datasz);

	rply_msg->encode();
	if (!write_socket(socket, rply_msg->raw_data(), rply_msg->size())) {
		tamsg(TGENET, 0);
		return retval;
	}

	ctx.rqopcode = -1;	/* disable replies until next request comes in */
	retval = 1;
	return retval;
}

void sg_agent::tasyncproc(basio::ip::tcp::socket& socket, tasync_t *args)
{
	int	i;
	char *argv[256];
	char *p;
	tasyncr_t tasyncr;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	p = args->argvs;
	for (i = 0; i < 256 && *p; i++) {
		argv[i] = p;
		p += strlen(p) + 1;
	}
	if (i == 256) {
		tamsg(TGEINVAL, 0);
		rplymsg(socket, SGEINVAL, NULL, 0, TGEINVAL, 0);
		return;
	}
	argv[i] = NULL;

	tasyncr.spret = sg_proc::instance(SGT).sync_proc(argv, args->mchent, NULL, args->flags, tasyncr.pid, NULL);
	rplymsg(socket, 0, &tasyncr, sizeof(tasyncr), 0, 0);
}

void sg_agent::tafmaster(basio::ip::tcp::socket& socket, tafmast_t *args)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_proc_t proc;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	SGC->_SGC_proc.mid = args->mid;
	SGC->_SGC_wka = args->ipckey;
	if (SGC->fmaster(args->mid, &proc) == NULL) {
		rplymsg(socket, SGESYSTEM, NULL, 0, 0, 0);
		return;
	}

	rplymsg(socket, 0, &proc, sizeof(proc), 0, 0);
}

void sg_agent::tasgprop(basio::ip::tcp::socket& socket, taprop_t *args)
{
	char xmlname[L_tmpnam];
	char tmloutput[L_tmpnam];
	char *outdata = NULL;
	int fd;
	struct stat buf;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (tmpnam(xmlname) == NULL || tmpnam(tmloutput) == NULL
		|| (fd = open(xmlname, O_WRONLY | O_CREAT, 0600)) < 0) {
		tamsg(TGEFILE, 0);
		rplymsg(socket, SGEOS, NULL, 0, TGEFILE, UOPEN);
		return;
	}

	BOOST_SCOPE_EXIT((&fd)(&xmlname)(&tmloutput)) {
		::close(fd);
		unlink(xmlname);
		unlink(tmloutput);
	} BOOST_SCOPE_EXIT_END

	int wlen = args->size;
	if (write(fd, args->data, wlen) != wlen) {
		tamsg(TGEFILE,0);
		rplymsg(socket, SGEOS, NULL, 0, TGEFILE, UWRITE);
		return;
	}

	string cmd;
	if (args->passwd[0] == '\0') {
		cmd = (boost::format("SGPROFILE=%1% %2%/bin/sgpack -s -H %3% > %4% 2>&1")
			% args->mchent.sgconfig % args->mchent.sgdir % xmlname % tmloutput).str();
	} else {
		cmd = (boost::format("SGPROFILE=%1% %2%/bin/sgpack -s -H -p %3% %4% > %5% 2>&1")
			% args->mchent.sgconfig % args->mchent.sgdir % args->passwd % xmlname % tmloutput).str();
	}

	sig_action sig(SIGCHLD, SIG_DFL);
	mode_t umsave = umask(0033);	/* Group/Other don't need write/execute */
	int sysrv = sys_func::instance().new_task(cmd.c_str(), NULL, 0);
	umask(umsave);

	if (sysrv != 0) {
		tamsg(TGELOADCF, 0);

		if (stat(tmloutput, &buf) < 0) {
			buf.st_size = 0;
		} else {
			boost::shared_array<char> outdata(new char[buf.st_size]);
			if ((fd = open(tmloutput, O_RDONLY, 0)) < 0)
				buf.st_size = 0;
			else if (read(fd, outdata.get(), buf.st_size) != buf.st_size)
				buf.st_size = 0;
		}

		rplymsg(socket, SGESYSTEM, outdata, buf.st_size, TGELOADCF, 0);
		return;
	}

	rplymsg(socket, 0, NULL, 0, 0, 0);
}

void sg_agent::tasgversion(basio::ip::tcp::socket& socket, sg_mchent_t *args)
{
	sg_config& config_mgr = sg_config::instance(SGT, false);
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	try {
		if (!config_mgr.reopen(args->sgconfig)) {
			rplymsg(socket, SGESYSTEM, NULL, 0, TGEPARMS, 0);
			return;
		}

		sg_bbparms_t& bbp = config_mgr.get_bbparms();
		rplymsg(socket, 0, &bbp.sgversion, sizeof(int), 0, 0);
	} catch (exception& ex) {
		rplymsg(socket, SGESYSTEM, NULL, 0, TGEPARMS, 0);
	}
}

void sg_agent::tarelease(basio::ip::tcp::socket& socket)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	int protocol = SGPROTOCOL;
	rplymsg(socket, 0, &protocol, sizeof(protocol), 0, 0);
}

bool sg_agent::write_socket(basio::ip::tcp::socket& socket, const char *raw_data, size_t msg_size)
{
	size_t sent_size = 0;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("msg_size={1}") % msg_size).str(SGLOCALE), &retval);
#endif

	while (sent_size < msg_size) {
		boost::system::error_code ignore_error;
		size_t size = socket.write_some(basio::buffer(raw_data + sent_size, msg_size - sent_size), ignore_error);
		if (size < 0)
			return retval;

		sent_size += size;
	}

	retval = true;
	return retval;
}

}
}

