#include "sg_internal.h"

namespace ai
{
namespace sg
{

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

boost::thread_specific_ptr<sgt_ctx> sgt_ctx::_instance;

sgt_ctx * sgt_ctx::instance()
{
	sgt_ctx *SGT = _instance.get();
	if (SGT == NULL) {
		SGT = new sgt_ctx();
		_instance.reset(SGT);

		// 创建管理对象数组
		SGT->_SGT_mgr_array[PTE_MANAGER] = new sg_pte();
		SGT->_SGT_mgr_array[NTE_MANAGER] = new sg_nte();
		SGT->_SGT_mgr_array[RTE_MANAGER] = new sg_rte();
		SGT->_SGT_mgr_array[SGTE_MANAGER] = new sg_sgte();
		SGT->_SGT_mgr_array[STE_MANAGER] = new sg_ste();
		SGT->_SGT_mgr_array[SCTE_MANAGER] = new sg_scte();
		SGT->_SGT_mgr_array[QTE_MANAGER] = new sg_qte();
		SGT->_SGT_mgr_array[PROC_MANAGER] = new sg_proc();
		SGT->_SGT_mgr_array[IPC_MANAGER] = new sg_ipc();
		SGT->_SGT_mgr_array[VIABLE_MANAGER] = new sg_viable();
		SGT->_SGT_mgr_array[AGENT_MANAGER] = new sg_agent();
		SGT->_SGT_mgr_array[RPC_MANAGER] = new sg_rpc();
		SGT->_SGT_mgr_array[SVCDSP_MANAGER] = new sg_svcdsp();
		SGT->_SGT_mgr_array[SVR_MANAGER] = NULL;
		SGT->_SGT_mgr_array[HANDLE_MANAGER] = new sg_handle();
		SGT->_SGT_mgr_array[META_MANAGER] = new sg_meta();
		SGT->_SGT_mgr_array[API_MANAGER] = new sg_api();
		SGT->_SGT_mgr_array[CHK_MANAGER] = NULL;
		SGT->_SGT_mgr_array[BRPC_MANAGER] = NULL;
		SGT->_SGT_mgr_array[GRPC_MANAGER] = NULL;
		SGT->_SGT_mgr_array[FRPC_MANAGER] = new file_rpc();
		SGT->_SGT_mgr_array[ALIVE_MANAGER] = new alive_rpc();
	}
	return SGT;
}

sgt_ctx::sgt_ctx()
{
	ctx_exiting = false;
	_SGT_thrid = pthread_self();

	_SGT_thrinit_called = false;
	_SGT_active_scte = NULL;
	_SGT_asvc_sgid = -1;

	_SGT_error_code = 0;
	_SGT_native_code = 0;
	_SGT_ur_code = 0;
	_SGT_error_detail = 0;

	_SGT_lock_type_by_me = UNLOCK;
	_SGT_blktime = 0;

	_SGT_context_invalid = true;
	_SGT_curctx = 0;
	_SGT_intcall = 0;

	_SGT_nwkill_counter = 0;
	_SGT_time_left = 0;

	_SGT_fini_called = false;
	_SGT_fini_rval = 0;
	_SGT_fini_urcode = 0;
	_SGT_fini_cmd = 0;
	_SGT_forward_called = false;

	memset(_SGT_setargv_argv, '\0', sizeof(_SGT_setargv_argv));

	_SGT_sndprio = 0;
	_SGT_lastprio = 0;
	_SGT_getrply_inthandle = 0;

	memset(_SGT_mgr_array, '\0', sizeof(_SGT_mgr_array));
	_SGT_svcdsp_inited = false;
	_SGT_sgconfig_opened = false;
}

sgt_ctx::~sgt_ctx()
{
	ctx_exiting = true;
	_SGT_cached_msgs.clear();
	_SGT_fini_msg.reset();

	BOOST_FOREACH(sg_svc *& svc, _SGT_svcdsp) {
		delete svc;
	}
	_SGT_svcdsp.clear();

	for (int i = 0; i < TOTAL_MANAGER; i++) {
		// We'll not free SVR_MANAGER since it's just referred.
		if (i == SVR_MANAGER)
			continue;

		if (_SGT_mgr_array[i] != NULL) {
			delete _SGT_mgr_array[i];
			_SGT_mgr_array[i] = NULL;
		}
	}
}

sgc_ctx * sgt_ctx::SGC()
{
	sgc_ctx *ctx = _SGT_SGC.get();
	if (ctx != NULL)
		return ctx;

	gpp_ctx *GPP = gpp_ctx::instance();
	sgp_ctx *SGP = sgp_ctx::instance();
	bi::scoped_lock<boost::recursive_mutex> lock(SGP->_SGP_ctx_mutex);

	if (SGP->_SGP_ctx_state == CTX_STATE_UNINIT) {
		SGP->_SGP_ctx_state = CTX_STATE_SINGLE;
		GPP->_GPP_do_threads = false;
	} else {
		GPP->_GPP_do_threads = true;
	}

	if (SGP->_SGP_ctx_state == CTX_STATE_SINGLE) {
		if (SGP->_SGP_ctx_array.size() > SGSINGLECONTEXT && SGP->_SGP_ctx_array[SGSINGLECONTEXT] != NULL) {
			_SGT_curctx = SGSINGLECONTEXT;
			_SGT_SGC = SGP->_SGP_ctx_array[SGSINGLECONTEXT];
		} else {
			_SGT_curctx = SGP->create_ctx();
			_SGT_SGC = SGP->_SGP_ctx_array[_SGT_curctx];
		}
	} else if (SGP->_SGP_ctx_state == CTX_STATE_MULTI) {
		_SGT_curctx = SGP->create_ctx();
		_SGT_SGC = SGP->_SGP_ctx_array[_SGT_curctx];
	} else {
		gpp_ctx *GPP = gpp_ctx::instance();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error.")).str(SGLOCALE));
		exit(1);
	}

	_SGT_context_invalid = false;
 	return _SGT_SGC.get();
}

/*
 * Builds command line arguments for a server based on its sgsvrent_t.
 * Prepends hidden options (-h -g -u -h and optionally -R) to the
 * beginning of the arguments and then adds the server specific clopt.
 * All space is malloc'd and can be freed by the caller.
 */
string sgt_ctx::formatopts(const sg_svrent_t& svrent, sg_ste_t *ste)
{
	string clopt;
	sg_mchent_t mchent;
	sg_config& config_mgr = sg_config::instance(this);

	boost::char_separator<char> sep(" \t\b");
	string tmp_clopt = svrent.bparms.clopt;
	tokenizer tokens(tmp_clopt, sep);
	bool has_ulogpfx = false;
	for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter) {
		string str = *iter;
		if (memcmp(str.c_str(), "-L", 2) == 0 || memcmp(str.c_str(), "--logname=", 10) == 0) {
			has_ulogpfx = true;
			break;
		}
	}

	if (svrent.bparms.flags & BF_PRESERVE_FD)
		clopt += "-f 000 ";

	// Add the " -g pgid" option used to identify the process group #
	clopt += (boost::format("-g %1%") % svrent.sparms.svrproc.grpid).str();

	// Add the "-p prcid" option used to identify the process id
	clopt += (boost::format(" -p %1%") % svrent.sparms.svrproc.svrid).str();

	mchent.mid = svrent.sparms.svrproc.mid;
	if (config_mgr.find(mchent)) {
		string pnode = mchent.pmid;
		string::size_type pos = pnode.find_last_of('.');
		if (pos != string::npos && pos + 1 < pnode.size()
			&& pnode[pos + 1] >= '0' && pnode[pos + 1] <= '9')
			pnode.resize(pos);

		/*
		 * Since NT machine names are allowed to contain
		 * spaces, quote the name if it contains one -
		 * else leave it alone.
		 */
		if (pnode.find(' ') != string::npos)
			clopt += (boost::format(" -u \"%1%\"") % pnode).str();
		else
			clopt += (boost::format(" -u %1%") % pnode).str();

		if (!has_ulogpfx) {
			string tbuf;

			if (mchent.ulogpfx[0] == '\0') {
				char *ptr = ::getenv("SGLOGNAME");
				if (ptr == NULL)
					tbuf = (boost::format("%1%/LOG") % mchent.appdir).str();
				else if (ptr[0] == '/')
					tbuf = ptr;
				else
					tbuf = (boost::format("%1%/%2%") % mchent.appdir % ptr).str();
			} else if (mchent.ulogpfx[0] == '/') {
				tbuf = mchent.ulogpfx;
			} else {
				tbuf = (boost::format("%1%/%2%") % mchent.appdir % mchent.ulogpfx).str();
			}

			if (tbuf.find(' ') != string::npos)
				clopt += (boost::format(" -L \"%1%\"") % tbuf).str();
			else
				clopt += (boost::format(" -L %1%") % tbuf).str();
		}
	}

	// Add the "-h hostid" option to identify the mid to the server
	clopt += (boost::format(" -h %1%") % svrent.sparms.svrproc.mid).str();

	if (svrent.bparms.flags & BF_SPAWN)
		clopt += " -D";

	// Restart passes a STE for the restarting server, add -R, et al
	if (ste != NULL) {
		clopt += " -R";
		if (ste->global.status & MIGRATING) {
			clopt += " 0";		// take any registry entry
			clopt += " -M";		// migrating
		} else {
			clopt += (boost::format(" %1%") % ste->pid()).str();
		}
	}

	// Now add the server specific arguments
	clopt += " ";
	clopt += svrent.bparms.clopt;

	// The argument string is now complete, return it to caller
	return clopt;
}

char ** sgt_ctx::setargv(const char *aout, const string& clopt)
{
	int i;

	/*
	 * Free up space malloc'd for the old vector.
	 */
	for (i = 0; _SGT_setargv_argv[i]; i++)
		delete []_SGT_setargv_argv[i];

	i = 0;
	_SGT_setargv_argv[i] = new char[strlen(aout) + 1];
	if (_SGT_setargv_argv[i] != NULL)
		strcpy(_SGT_setargv_argv[i++], aout);

	boost::escaped_list_separator<char> sep('\\', ' ', '\"');
	boost::tokenizer<boost::escaped_list_separator<char> > tok(clopt, sep);
	for (boost::tokenizer<boost::escaped_list_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter) {
		if (i >= 256)
			break;

		// 如果字段全是空白，则跳过
		string arg = *iter;
		bool skip = true;
		BOOST_FOREACH(const char& ch, arg) {
			if (!isspace(ch)) {
				skip = false;
				break;
			}
		}

		if (skip)
			continue;

		_SGT_setargv_argv[i] = new char[arg.length() + 1];
		if (_SGT_setargv_argv[i] != NULL)
			strcpy(_SGT_setargv_argv[i++], arg.c_str());
	}

	if (i < 256)
		_SGT_setargv_argv[i] = NULL;
	else
		return NULL;

	return _SGT_setargv_argv;
}

void sgt_ctx::clear()
{
	_SGT_error_code = 0;
	_SGT_blktime = 0;
	_SGT_curctx = 0;
	_SGT_time_left = 0;

	// 关闭sgconfig文件
	sg_config& config_mgr = sg_config::instance(this, false);
	config_mgr.close();
}

queue_pointer sgt_ctx::get_queue(int mid, int qaddr)
{
	sg_queue_key_t key;
	sg_queue_info_t item;
	gpp_ctx *GPP = gpp_ctx::instance();
	sgp_ctx *SGP = sgp_ctx::instance();
	sgc_ctx *_SGC = _SGT_SGC.get();
	sg_bboard_t *bbp = _SGC->_SGC_bbp;
	sg_bbparms_t& bbparms = bbp->bbparms;
	sg_bbmeters_t& bbmeters = bbp->bbmeters;
	queue_iterator iter;

	key.mid = mid;
	key.qid = qaddr & QUEUE_KMASK;

	if (!GPP->_GPP_do_threads) {
		if (SGP->_SGP_queue_expire <= bbmeters.timestamp) {
			SGP->_SGP_queue_map.clear();
			SGP->_SGP_queue_expire = bbmeters.timestamp + QUEUE_EXPIRES;
		} else {
			iter = SGP->_SGP_queue_map.find(key);
			if (iter != SGP->_SGP_queue_map.end()) {
				sg_queue_info_t& item = iter->second;
				if (item.qaddr == qaddr)
					return item.queue;

				// 队列已经过期
				SGP->_SGP_queue_map.erase(iter);
			}
		}

		if (SGP->_SGP_queue_map.size() >= bbparms.max_open_queues) {
			SGP->_SGP_queue_map.clear();

			if (!SGP->_SGP_queue_warned) {
				SGP->_SGP_queue_warned = true;
				GPP->write_log(__FILE__, __LINE__, (_("WARN: queue cache too small.")).str(SGLOCALE));
			}
		}

		// 创建qaddr对应的对象
		string qname = (boost::format("%1%.%2%") % _SGC->midkey(mid) % qaddr).str();
		queue_pointer queue(new bi::message_queue(bi::open_only, qname.c_str()));

		item.qaddr = qaddr;
		item.queue = queue;
		SGP->_SGP_queue_map[key] = item;
		return queue;
	} else {
		if (SGP->_SGP_queue_expire <= bbmeters.timestamp) {
			// 需要在加锁的条件下再次判断
			bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(SGP->_SGP_queue_mutex);

			if (SGP->_SGP_queue_expire <= bbmeters.timestamp) {
				SGP->_SGP_queue_map.clear();
				SGP->_SGP_queue_expire = bbmeters.timestamp + QUEUE_EXPIRES;
			}
		}

		{
			bi::sharable_lock<bi::interprocess_upgradable_mutex> slock(SGP->_SGP_queue_mutex);

			iter = SGP->_SGP_queue_map.find(key);
			if (iter != SGP->_SGP_queue_map.end()) {
				sg_queue_info_t& item = iter->second;
				if (item.qaddr == qaddr)
					return item.queue;
			}
		}

		bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(SGP->_SGP_queue_mutex);

		iter = SGP->_SGP_queue_map.find(key);
		if (iter != SGP->_SGP_queue_map.end()) {
			sg_queue_info_t& item = iter->second;
			if (item.qaddr == qaddr)
				return item.queue;

			// 队列已经过期，删除该队列
			SGP->_SGP_queue_map.erase(iter);
		}

		if (SGP->_SGP_queue_map.size() >= bbparms.max_open_queues) {
			SGP->_SGP_queue_map.clear();

			if (!SGP->_SGP_queue_warned) {
				SGP->_SGP_queue_warned = true;
				GPP->write_log(__FILE__, __LINE__, (_("WARN: queue cache too small.")).str(SGLOCALE));
			}
		}

		// 创建qaddr对应的对象
		string qname = (boost::format("%1%.%2%") % _SGC->midkey(mid) % qaddr).str();
		queue_pointer queue(new bi::message_queue(bi::open_only, qname.c_str()));

		item.qaddr = qaddr;
		item.queue = queue;
		SGP->_SGP_queue_map[key] = item;
		return queue;
	}
}

const char * sgt_ctx::strerror() const
{
	return strerror(_SGT_error_code);
}

const char * sgt_ctx::strnative() const
{
	return uunix::strerror(_SGT_native_code);
}

const char * sgt_ctx::strerror(int error_code)
{
	static const string emsgs[] = {
		"",
		(_("SGEABORT - transaction can not commit")).str(SGLOCALE),			/* SGEABORT */
		(_("SGEBADDESC - bad communication descriptor")).str(SGLOCALE),		/* SGEBADDESC */
		(_("SGEBLOCK - blocking condition found")).str(SGLOCALE),				/* SGEBLOCK */
		(_("SGEINVAL - invalid arguments given")).str(SGLOCALE),				/* SGEINVAL */
		(_("SGELIMIT - a system limit has been reached")).str(SGLOCALE),		/* SGELIMIT */
		(_("SGENOENT - no entry found")).str(SGLOCALE),						/* SGENOENT */
		(_("SGEOS - operating system error")).str(SGLOCALE),					/* SGEOS */
		(_("SGEPERM - bad permissions")).str(SGLOCALE),						/* SGEPERM */
		(_("SGEPROTO - protocol error")).str(SGLOCALE),						/* SGEPROTO */
		(_("SGESVCERR - process error while handling request")).str(SGLOCALE),	/* SGESVCERR */
		(_("SGESVCFAIL - application level operation failure")).str(SGLOCALE),	/* SGESVCFAIL */
		(_("SGESYSTEM - internal system error")).str(SGLOCALE),				/* SGESYSTEM */
		(_("SGETIME - timeout occured")).str(SGLOCALE),						/* SGETIME */
		(_("SGETRAN - error starting transaction")).str(SGLOCALE),			/* SGETRAN */
		(_("SGGOTSIG - signal received and SGSIGRSTRT not specified")).str(SGLOCALE),/* SGGOTSIG */
		(_("SGERMERR - resource manager error")).str(SGLOCALE),				/* SGERMERR */
		(_("SGERELEASE - invalid release")).str(SGLOCALE),					/* SGERELEASE */
		(_("SGEMATCH - operation name cannot be advertised due to matching conflict")).str(SGLOCALE), /* SGEMATCH */
		(_("SGEDIAGNOSTIC - function failed - check diagnostic value")).str(SGLOCALE), 	/* SGEDIAGNOSTIC */
		(_("SGESVRSTAT - bad process status")).str(SGLOCALE),					/* SGESVRSTAT */
		(_("SGEBBINSANE - BB is insane")).str(SGLOCALE),						/* SGEBBINSANE */
		(_("SGEDUPENT - duplicate table entry")).str(SGLOCALE),				/* SGEDUPENT */
		(_("SGECHILD - child start problem")).str(SGLOCALE),					/* SGECHILD */
		(_("SGENOMSG - no message")).str(SGLOCALE),							/* SGENOMSG */
		(_("SGENOMNGR - no sgmngr")).str(SGLOCALE),								/* SGENOMNGR */
		(_("SGENOCLST - no sgclst")).str(SGLOCALE),								/* SGENOCLST */
		(_("SGENOGWS - no sggws available")).str(SGLOCALE),						/* SGENOGWS */
		(_("SGEAPPINIT - svrinit() failure")).str(SGLOCALE),					/* SGEAPPINIT */
		(_("SGENEEDPCLEAN - sgmngr failed to boot.. pclean may help")).str(SGLOCALE),	/* SGENEEDPCLEAN */
		(_("SGENOFUNC - unknown function address")).str(SGLOCALE)				/* SGENOFUNC */
	};

	if (error_code <= SGMINVAL || error_code >= SGMAXVAL)
		return "";

	return emsgs[error_code].c_str();
}

}
}

