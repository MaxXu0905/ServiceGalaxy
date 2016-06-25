#include <sys/utsname.h>
#include "sg_internal.h"

namespace ai
{
namespace sg
{

class entry_initializer
{
public:
	template<typename T>
	entry_initializer& operator()(T *base, int rows)
	{
		for (int i = 0; i < rows; i++, base++) {
			(void)new (base) T(i);
			base->hashlink.fhlink = i + 1;
			base->hashlink.bhlink = i - 1;
		}
		base--;
		base->hashlink.fhlink = -1;

		return *this;
	}
};

class hash_initializer
{
public:
	hash_initializer& operator()(int *base, int rows)
	{
		for (int i = 0; i < rows; i++)
			*base++ = -1;

		return *this;
	}
};

sgc_ctx::sgc_ctx()
{
	_SGC_ctx_id = -1;

	_SGC_bbp = NULL;
	_SGC_bbmap = NULL;
	_SGC_ptes = NULL;
	_SGC_ntes = NULL;
	_SGC_rtes = NULL;
	_SGC_qtes = NULL;
	_SGC_sgtes = NULL;
	_SGC_stes = NULL;
	_SGC_sctes = NULL;
	_SGC_qte_hash = NULL;
	_SGC_sgte_hash = NULL;
	_SGC_ste_hash = NULL;
	_SGC_scte_hash = NULL;

	_SGC_slot = -1;
	_SGC_crte = NULL;
	_SGC_rplyq_serial = 0;

	memset(_SGC_handleflags, '\0', sizeof(_SGC_handleflags));
	memset(_SGC_handles, '\0', sizeof(_SGC_handleflags));
	_SGC_hdllastfreed = -1;

	_SGC_proc.clear();
	_SGC_amclient = false;
	_SGC_amadm = false;
	_SGC_svrstate = 0;
	_SGC_here_gotten = false;
	_SGC_attach_envread = false;
	_SGC_didregister = false;
	_SGC_blktime = 0;

	_SGC_gotdflts = false;
	_SGC_gotparms = 0;
	_SGC_sginit_pid = 0;
	_SGC_sginit_savepid = 0;

	_SGC_max_num_msg = -1;
	_SGC_max_msg_size = -1;
	_SGC_cmprs_limit = 0;
	_SGC_uid = 0;
	_SGC_gid = 0;
	_SGC_perm = 0;

	_SGC_sndmsg_wkidset = false;
	_SGC_sndmsg_wkid = 0;
	_SGC_sndmsg_wkproc.clear();
	_SGC_wka = -1;
	_SGC_findgw_version = -1;
	_SGC_findgw_nextcache = -1;

	_SGC_do_remedy = 0;
	_SGC_svc_rounds = 0;
	_SGC_mbrtbl_curraddr.clear();
	_SGC_mbrtbl_raddr = NULL;

	_SGC_getmid_mid = BADMID;

	_SGC_asvcindx = -1;
	_SGC_svcindx = -1;
	_SGC_sssq = true;
	_SGC_svstats_doingwork = false;
	_SGC_sgidx = -1;
	_SGC_asvcsgid = BADSGID;

	_SGC_enter_inatmi = 0;
	memset(_SGC_handlesvcindex, 0, sizeof(_SGC_handlesvcindex));
	_SGC_getrply_count = 0;
	_SGC_receive_getanycnt = 0;
	_SGC_rcvrq_countdown = STARVPROTECT;
	_SGC_rcvrq_cpending = 1;

	_SGC_bbasw = NULL;
	_SGC_ldbsw = NULL;
	_SGC_authsw = NULL;

	_SGC_mkmsg_ritercount = 0;
	_SGC_enable_alrm = false;

	_SGC_special_mid = BADMID;

	_SGC_ctx_level = 0;
	memset(&_SGC_thrid, '\0', sizeof(_SGC_thrid));
}

sgc_ctx::~sgc_ctx()
{
}

bool sgc_ctx::enter(sgt_ctx *SGT, int flags)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	if (SGT->_SGT_curctx == 0 && SGT->_SGT_context_invalid && !(flags & ATMIENTER_INVALIDCONTEXT_OK)) {
		SGT->_SGT_error_code = SGEPROTO;
		return retval;
	}

	// NOTE: may not be attached to the BB at this point; see below
	bool lck;
	if (!GPP->_GPP_do_threads)
		lck = true;
	else
		lck = (flags & NML_TSF);

	if (!lck) {
		_SGC_ctx_mutex.lock();
		if (_SGC_ctx_level++ == 0)
			_SGC_thrid = SGT->_SGT_thrid;
	}

	_SGC_enter_inatmi++;

	/*
	 * See if we've been administratively marked to be killed AND we're
	 * a regular client.
	 */
	if (_SGC_amclient && _SGC_crte != NULL && (_SGC_crte->rflags & R_DEAD)) {
		GPP->write_log((_("WARN: Client aborting processing")).str(SGLOCALE));
		abort();
	}

	if (_SGC_bbp != NULL && _SGC_crte != NULL)
		_SGC_crte->rtimestamp = _SGC_bbp->bbmeters.timestamp;

	if (_SGC_enter_inatmi == 1) {
		SGT->_SGT_error_code = 0;
		SGT->_SGT_error_detail = 0;
	}

	retval = true;
	return retval;
}

bool sgc_ctx::leave(sgt_ctx *SGT, int flags)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	bool lck;
	if (!GPP->_GPP_do_threads)
		lck = true;
	else
		lck = (flags & NML_TSF);

	_SGC_enter_inatmi--;

	int error_code = SGT->_SGT_error_code;
	int error_detail  = SGT->_SGT_error_detail;
	BOOST_SCOPE_EXIT((&SGT)(error_code)(error_detail)) {
		SGT->_SGT_error_code = error_code;
		SGT->_SGT_error_detail = error_detail;
	} BOOST_SCOPE_EXIT_END

	/*
	 * See if we've been administratively marked to be killed AND we're
	 * a regular client.  Workstation clients must be terminated through
	 * the WSMIB(5) and that is handled by the WSH process.
	 */
	if (_SGC_amclient && _SGC_crte != NULL && (_SGC_crte->rflags & R_DEAD)) {
		GPP->write_log((_("WARN: Client aborting processing")).str(SGLOCALE));
		abort();
	}

	// NOTE: may not be attached to the BB at this point
	if (!lck) {
		if (--_SGC_ctx_level == 0)
			memset(&_SGC_thrid, '\0', sizeof(_SGC_thrid));
		_SGC_ctx_mutex.unlock();
	}

	retval = true;
	return retval;
}

int sgc_ctx::lock(sgt_ctx *SGT, int ctx_level)
{
	for (int i = 0; i < ctx_level; i++) {
		_SGC_ctx_mutex.lock();
		_SGC_thrid = SGT->_SGT_thrid;
		_SGC_ctx_level++;
	}

	return _SGC_ctx_level;
}

int sgc_ctx::unlock(sgt_ctx *SGT)
{
	if (_SGC_thrid != SGT->_SGT_thrid)
		return 0;

	int ctx_level = _SGC_ctx_level;
	for (int i = 0; i < ctx_level; i++) {
		_SGC_ctx_level--;
		memset(&_SGC_thrid, '\0', sizeof(_SGC_thrid));
		_SGC_ctx_mutex.unlock();
	}

	return ctx_level;
}

void sgc_ctx::get_defaults()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!_SGC_gotdflts) {
		if (get_bbparms()) {
			_SGC_max_num_msg = _SGC_getbbparms_curbbp.max_num_msg;
			_SGC_max_msg_size = _SGC_getbbparms_curbbp.max_msg_size;
			_SGC_cmprs_limit = _SGC_getbbparms_curbbp.cmprs_limit;
			_SGC_uid = _SGC_getbbparms_curbbp.uid;
			_SGC_gid = _SGC_getbbparms_curbbp.gid;
			_SGC_perm = _SGC_getbbparms_curbbp.perm;
			_SGC_do_remedy = _SGC_getbbparms_curbbp.remedy;
		} else {
			_SGC_max_num_msg = DFLT_MSGMNB;
			_SGC_max_msg_size = DFLT_MSGMAX;
			_SGC_cmprs_limit = DFLT_CMPRSLIMIT;
			_SGC_uid = getuid();
			_SGC_gid = getgid();
			_SGC_perm = DFLT_PERM;
			_SGC_do_remedy = DFLT_COSTFIX;
		}

		_SGC_gotdflts = true;
	}
}

bool sgc_ctx::get_here(sg_svrent_t& svrent)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (_SGC_here_gotten) {
		retval = true;
		return retval;
	}

	gpp_ctx *GPP = gpp_ctx::instance();
	sgp_ctx *SGP = sgp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
	sg_proc& proc_mgr = sg_proc::instance(SGT);

	_SGC_proc.grpid = svrent.sparms.svrproc.grpid;
	_SGC_proc.svrid = svrent.sparms.svrproc.svrid;
	_SGC_proc.pid = getpid();
	if ((_SGC_proc.mid = getmid()) == BADMID)
		return retval;

	if (svrent.sparms.rqparms.saddr[0] == '\0')
		sprintf(svrent.sparms.rqparms.saddr, "%d.%d", svrent.sparms.svrproc.grpid, svrent.sparms.svrproc.svrid);

	size_t max_num_msg;
	size_t max_msg_size;
	int rqperm;
	int rpperm;
	if (svrent.sparms.max_num_msg > 0)
		max_num_msg = svrent.sparms.max_num_msg;
	else
		max_num_msg = SGC->_SGC_max_num_msg;

	if (svrent.sparms.max_msg_size > 0)
		max_msg_size = svrent.sparms.max_msg_size;
	else
		max_msg_size = SGC->_SGC_max_msg_size;

	if (svrent.sparms.cmprs_limit > 0)
		SGC->_SGC_cmprs_limit = svrent.sparms.cmprs_limit;

	if (svrent.sparms.rqperm > 0)
		rqperm = svrent.sparms.rqperm;
	else
		rqperm = SGC->_SGC_perm;

	if (svrent.sparms.rpperm > 0)
		rpperm = svrent.sparms.rpperm;
	else
		rpperm = SGC->_SGC_perm;

	if (svrent.sparms.remedy > 0)
		SGC->_SGC_do_remedy = svrent.sparms.remedy;

	// 创建request queue
	if (!(SGP->_SGP_boot_flags & BF_HAVEQ)) {
		if ((_SGC_proc.rqaddr = proc_mgr.getq(_SGC_proc.mid, 0, max_num_msg, max_msg_size, rqperm)) < 0) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = errno;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to create request queue")).str(SGLOCALE));
			return retval;
		}
	}

	// 创建reply queue
	if (SGP->_SGP_boot_flags & BF_REPLYQ) {
		if ((_SGC_proc.rpaddr = proc_mgr.getq(_SGC_proc.mid, 0, max_num_msg, max_msg_size, rpperm)) < 0) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = errno;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to create reply queue")).str(SGLOCALE));
			return retval;
		}
	} else {
		_SGC_proc.rpaddr = _SGC_proc.rqaddr;
	}

	// 设置消息队列地址
	if (strcmp(_SGC_authsw->name, "SGCLST") == 0) {
		if (!proc_mgr.create_wkid(true)) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = errno;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create wkid for sgclst")).str(SGLOCALE));
			return retval;
		}
	}

	_SGC_here_gotten = true;
	retval = true;
	return retval;
}

bool sgc_ctx::remote(int mid) const
{
	return (midnidx(mid) != midnidx(_SGC_proc.mid));
}

bool sgc_ctx::set_bbtype(const string& name)
{
	int key;
	sgt_ctx *SGT = sgt_ctx::instance();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("name={1}") % name).str(SGLOCALE), &retval);
#endif

	if (!_SGC_gotparms && !get_bbparms())
		return retval;

	if (!set_bba(name))
		return retval;

	int oldbbtype = _SGC_bbasw->bbtype;
	_SGC_bbasw->bbtype |= oldbbtype & ~BBT_MASK;

	_SGC_getbbparms_curbbp.magic = (_SGC_bbasw->bbtype & BBT_MASK) | (VERSION >> VSHIFT);
	if ((_SGC_bbasw->bbtype & BBT_MASK) == BBT_MP)
		_SGC_getbbparms_curbbp.ipckey = getkey();

	/* For message type processes, set the WKA's for the process based on
	 * whether the sgclst is local or not. Use the DBBMs WK queue and sem
	 * to determine if it's around or not.
	 */
	if ((_SGC_bbasw->bbtype & BBT_MASK) == BBT_MSG && (_SGC_bbasw->bbtype & BBF_LAN)) {
		bool set = false;
		sg_proc& proc_mgr = sg_proc::instance(SGT);
		if (proc_mgr.getq(_SGC_proc.mid, _SGC_getbbparms_curbbp.ipckey) >= 0)
			set = true;

		// It wasn't local so set WKA to a bridge process
		if (!set && (key = netkey()) >= 0)
			_SGC_getbbparms_curbbp.ipckey = key;
	}

	if (_SGC_getbbparms_curbbp.ldbal == LDBAL_RR)
		retval = set_ldb("RR");
	else
		retval = set_ldb("RT");

	_SGC_gotparms = 2;
	return retval;
}

bool sgc_ctx::set_atype(const string& name)
{
	return set_auth(name);
}

bool sgc_ctx::attach()
{
	// 同时只有一个线程操作该函数
	int i;
	scoped_usignal sig;
	gpp_ctx *GPP = gpp_ctx::instance();
	sgp_ctx *SGP = sgp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_sgte& sgte_mgr = sg_sgte::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (!get_bbparms())
		return retval;

	sg_bbparms_t& bbparms = _SGC_getbbparms_curbbp;

	if (_SGC_didregister) {
		retval = true;
		return retval;
	}

	if (!_SGC_proc.pid)
		_SGC_proc.pid = getpid();

	// 设置共享内存字节数
	bbparms.bbsize = sg_bboard_t::bbsize(bbparms);

	bool is_bbm = (strcmp(_SGC_authsw->name, "SGMNGR") == 0);
	bool is_fake_dbbm = (_SGC_bbasw->bbtype & BBF_DBBM);
	bool is_restarting = (SGP->_SGP_oldpid > 0);
	bool require_create = is_fake_dbbm;

	if (_SGC_bbp == NULL) {
		string shm_name;
		if (_SGC_bbasw->bbtype & BBF_DBBM)
			shm_name = (boost::format("shm.%1%") % _SGC_wka).str();
		else
			shm_name = (boost::format("shm.%1%") % midkey(_SGC_getmid_mid)).str();

		if (is_bbm || is_fake_dbbm) {
			try {
				// 打开共享内存
				mapping_mgr.reset(new bi::shared_memory_object(bi::open_only, shm_name.c_str(), bi::read_write));

				// 检查BB大小
				bi::offset_t bbsize = 0;
				mapping_mgr->get_size(bbsize);
				if (bbsize < sizeof(sg_bboard_t)) {
					SGT->_SGT_error_code = SGEOS;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't attach shared memory, BB changed")).str(SGLOCALE));
					return retval;
				}

				region_mgr.reset(new bi::mapped_region(*mapping_mgr, bi::read_write, 0, bbsize, NULL));

				// 检查BB onwer是否存在，存在则退出
				_SGC_bbp = reinterpret_cast<sg_bboard_t *>(region_mgr->get_address());
				if (_SGC_bbp->bbparms.pid > 0
					&& _SGC_bbp->bbparms.pid != _SGC_proc.pid
					&& kill(_SGC_bbp->bbparms.pid, 0) != -1) {
					SGT->_SGT_error_code = SGEOS;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create shared memory, BB owner {1} alives") % _SGC_bbp->bbparms.pid).str(SGLOCALE));
					return retval;
				}

				// 如果BB有变化，或者正常启动，则需要重新创建
				if (bbsize != bbparms.bbsize || !is_restarting) {
					if (is_restarting) {
						SGT->_SGT_error_code = SGEOS;
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open shared memory, BB changed")).str(SGLOCALE));
						return retval;
					}

					require_create = true;
				} else {
					_SGC_bbp->bbparms.pid = _SGC_proc.pid;
				}
			} catch (bi::interprocess_exception& ex) {
				if (is_restarting) {
					SGT->_SGT_error_code = SGEOS;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open shared memory, BB removed")).str(SGLOCALE));
					return retval;
				}

				require_create = true;
			}
		} else {
			try {
				// 打开共享内存
				mapping_mgr.reset(new bi::shared_memory_object(bi::open_only, shm_name.c_str(), bi::read_write));

				// 如果BB有变化，则退出
				bi::offset_t bbsize = 0;
				mapping_mgr->get_size(bbsize);
				if (bbsize != bbparms.bbsize) {
					SGT->_SGT_error_code = SGEOS;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't attach shared memory, BB changed")).str(SGLOCALE));
					return retval;
				}

				// 检查BB onwer是否存在，不存在则退出，允许ADMIN客户端登录
				region_mgr.reset(new bi::mapped_region(*mapping_mgr, bi::read_write, 0, bbparms.bbsize, NULL));
				_SGC_bbp = reinterpret_cast<sg_bboard_t *>(region_mgr->get_address());
				if (_SGC_slot < bbparms.maxaccsrs
					&& (_SGC_bbp->bbparms.pid <= 0 || kill(_SGC_bbp->bbparms.pid, 0) == -1)) {
					SGT->_SGT_error_code = SGEOS;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create shared memory, BB owner {1} died") % _SGC_bbp->bbparms.pid).str(SGLOCALE));
					return retval;
				}
			} catch (bi::interprocess_exception& ex) {
				SGT->_SGT_error_code = SGEOS;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open shared memory, error_code = {1}, native_error = {2}")
					 % ex.get_error_code()% ex.get_native_error()).str(SGLOCALE));
				return retval;
			}
		}

		if (require_create) {
			try {
				// 创建共享内存
				bi::shared_memory_object::remove(shm_name.c_str());
				mapping_mgr.reset(new bi::shared_memory_object(bi::create_only,
					shm_name.c_str(), bi::read_write, _SGC_perm));
				proc_mgr.chown(shm_name, _SGC_uid, _SGC_gid);

				// 设置共享内存大小
				mapping_mgr->truncate(bbparms.bbsize);
				region_mgr.reset(new bi::mapped_region(*mapping_mgr, bi::read_write, 0, bbparms.bbsize, NULL));
				_SGC_bbp = reinterpret_cast<sg_bboard_t *>(region_mgr->get_address());
			} catch (bi::interprocess_exception& ex) {
				SGT->_SGT_error_code = SGEOS;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create shared memory, error_code = {1}, native_error = {2}")
					 % ex.get_error_code()% ex.get_native_error()).str(SGLOCALE));
				return retval;
			}

			if (is_fake_dbbm)
				bi::shared_memory_object::remove(shm_name.c_str());

			(void)new (_SGC_bbp) sg_bboard_t(bbparms);
		} else {
			if (_SGC_bbp->bbparms.bbsize != bbparms.bbsize
				|| _SGC_bbp->bbparms.maxpes != bbparms.maxpes
				|| _SGC_bbp->bbparms.maxnodes != bbparms.maxnodes
				|| _SGC_bbp->bbparms.maxaccsrs != bbparms.maxaccsrs
				|| _SGC_bbp->bbparms.maxques != bbparms.maxques
				|| _SGC_bbp->bbparms.maxsgt != bbparms.maxsgt
				|| _SGC_bbp->bbparms.maxsvrs != bbparms.maxsvrs
				|| _SGC_bbp->bbparms.maxsvcs != bbparms.maxsvcs) {
				SGT->_SGT_error_code = SGESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Configuration file parameters do not match existing bulletin board.")).str(SGLOCALE));
				detach();
				return retval;
			}
		}
	}

	_SGC_bbmap = &_SGC_bbp->bbmap;

	_SGC_ptes = reinterpret_cast<sg_pte_t *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->pte_root);
	_SGC_ntes = reinterpret_cast<sg_nte_t *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->nte_root);
	_SGC_qtes = reinterpret_cast<sg_qte_t *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->qte_root);
	_SGC_sgtes = reinterpret_cast<sg_sgte_t *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->sgte_root);
	_SGC_stes = reinterpret_cast<sg_ste_t *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->ste_root);
	_SGC_sctes = reinterpret_cast<sg_scte_t *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->scte_root);
	_SGC_rtes = reinterpret_cast<sg_rte_t *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->rte_root);

	_SGC_qte_hash = reinterpret_cast<int *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->qte_hash);
	_SGC_sgte_hash = reinterpret_cast<int *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->sgte_hash);
	_SGC_ste_hash = reinterpret_cast<int *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->ste_hash);
	_SGC_scte_hash = reinterpret_cast<int *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->scte_hash);

	_SGC_misc = reinterpret_cast<sg_misc_t *>(reinterpret_cast<char *>(_SGC_bbp) + _SGC_bbmap->misc_root);

	if (require_create) {
		entry_initializer()
			(_SGC_qtes, bbparms.maxques)
			(_SGC_sgtes, bbparms.maxsgt)
			(_SGC_stes, bbparms.maxsvrs)
			(_SGC_sctes, bbparms.maxsvcs);

		hash_initializer()
			(_SGC_qte_hash, bbparms.quebkts)
			(_SGC_sgte_hash, bbparms.sgtbkts)
			(_SGC_ste_hash, bbparms.svrbkts)
			(_SGC_scte_hash, bbparms.svcbkts);

		// 初始化RTE
		for (i = 0; i < bbparms.maxaccsrs; i++) {
			(void)new (_SGC_rtes + i) sg_rte_t(i);
			_SGC_rtes[i].nextreg = i + 1;
		}
		_SGC_rtes[i - 1].nextreg = -1;

		if (!buildpe()) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Error building node tables")).str(SGLOCALE));
			detach();
			return retval;
		}

		_SGC_misc->maxques = bbparms.maxsvrs * 2 + bbparms.maxaccsrs + MAXADMIN - 1;
		_SGC_misc->que_free = 0;
		_SGC_misc->que_tail = _SGC_misc->maxques - 1;
		_SGC_misc->que_used = -1;
		for (i = 0; i < _SGC_misc->maxques; i++)
			_SGC_misc->items[i].init(i);
		_SGC_misc->items[i - 1].next = -1;
	}

	_SGC_proc.mid = getmid();

	sg_mchent_t mchent;
	mchent.mid = _SGC_proc.mid;

	if (!config_mgr.find(mchent)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find node entry for hostid {1}") % _SGC_proc.mid).str(SGLOCALE));
		return retval;
	}
	if (mchent.envfile[0] != '\0'
		&& !_SGC_attach_envread
		&& !SGP->putenv(mchent.envfile, mchent)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Error putting PROFILE {1} into environment") % mchent.envfile).str(SGLOCALE));
		SGT->_SGT_error_code = SGESYSTEM;
		detach();
		return retval;
	}

	_SGC_attach_envread = true;

	if (mchent.appdir[0] != '\0' && strcmp(_SGC_authsw->name, "CLIENT") != 0) {
		char *s = strchr(mchent.appdir, ':');
		if (s != NULL)
			*s = '\0';
		if (chdir(mchent.appdir) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("WARN: Error changing directory to approot {1} for hostid {2}")
				% mchent.appdir % mchent.mid).str());
		}
		if (s != NULL)
			*s = ':';
	}

	// 保护注册节点的创建
	scoped_syslock syslock(SGT);

	if (!(_SGC_bbp->bbmeters.status & SHUTDOWN)) {
		sg_rte& rte_mgr = sg_rte::instance(SGT);
		if (!rte_mgr.enter()) {
			SGT->_SGT_error_code = SGENOENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failure to create register")).str(SGLOCALE));
			syslock.unlock();
			bbfail();
			return retval;
		}

		_SGC_didregister = true;
	} else {
		SGT->_SGT_error_code = SGENOMNGR;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No sgmngr on this node")).str(SGLOCALE));
		syslock.unlock();
		bbfail();
		return retval;
	}

	// 只有DBBM或Fake DBBM可以调用该函数，BBM直接从DBBM中获取
	if (is_fake_dbbm) {
		if (!sgte_mgr.getsgrps()) {
			syslock.unlock();
			bbfail();
			return retval;
		}
	}

	if (_SGC_bbasw->bbtype & BBF_ALRM) {
		// Start alarm
		_SGC_bbp->bbmeters.timestamp = time(0);
		_SGC_enable_alrm = true;
		usignal::siginit();
		signal(SIGALRM, sigalrm);
		alarm(1);
	} else if (!require_create) {
		_SGC_enable_alrm = true;
	}

	retval = true;
	return retval;
}

void sgc_ctx::detach()
{
	string shm_name;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (_SGC_bbp == NULL)
		return;

	sgp_ctx *SGP = sgp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();

	// Stop alarm
	if (_SGC_bbasw->bbtype & BBF_ALRM) {
		alarm(0);
		usignal::signal(SIGALRM, SIG_IGN);
		_SGC_enable_alrm = false;
	}

	{
		scoped_syslock syslock(SGT);

		if (_SGC_didregister) {
			sg_rte& rte_mgr = sg_rte::instance(SGT);
			rte_mgr.leave(NULL);
			_SGC_didregister = false;
		}

		// 重置对注册节点的引用
		_SGC_slot = -1;
		_SGC_crte = NULL;
	}

	// 对于SERVER，shutrply消息需要该节点存在
	if (!SGP->_SGP_amserver) {
		region_mgr.reset();
		mapping_mgr.reset();
		_SGC_bbp = NULL;
	}

	// 删除共享内存
	if (SGP->_SGP_amdbbm) {
		shm_name = (boost::format("shm.wka.%1%") % _SGC_wka).str();
		bi::shared_memory_object::remove(shm_name.c_str());
	} else if (SGP->_SGP_ambsgws || SGP->_SGP_amgws) {
		shm_name = (boost::format("shm.wka.%1%") % midkey(_SGC_proc.mid)).str();
		bi::shared_memory_object::remove(shm_name.c_str());
	} else if (SGP->_SGP_ambbm) {
		shm_name = (boost::format("shm.%1%") % midkey(_SGC_proc.mid)).str();
		bi::shared_memory_object::remove(shm_name.c_str());
	}
}

bool sgc_ctx::hookup(const string& name)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("name={1}") % name).str(SGLOCALE), &retval);
#endif

	if(!get_bbparms())
		return retval;

	if(!set_atype(name))
		return retval;

	// attempt to attach Bulletin Board
	if (!attach())
		return retval;

	retval = true;
	return retval;
}

bool sgc_ctx::buildpe()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	// Check pointers and initialize first entry.
	if (_SGC_ptes == NULL || _SGC_ntes == NULL)
		return retval;

	_SGC_ptes[0].flags = NP_UNUSED;
	_SGC_ntes[0].flags = NT_UNUSED;

	sgt_ctx *SGT = sgt_ctx::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_pte& pte_mgr = sg_pte::instance(SGT);
	sg_nte& nte_mgr = sg_nte::instance(SGT);

	for (sg_config::mch_iterator iter = config_mgr.mch_begin(); iter != config_mgr.mch_end(); ++iter) {
		if (pte_mgr.create(*iter, U_LOCAL) < 0)
			return retval;
	}

	for (sg_config::net_iterator iter = config_mgr.net_begin(); iter != config_mgr.net_end(); ++iter) {
		if (nte_mgr.create(*iter, U_LOCAL) < 0)
			return retval;
	}

	SGT->_SGT_error_code = 0;
	retval = true;
	return retval;
}

void sgc_ctx::setmid(int mid)
{
	_SGC_getmid_mid = mid;
}

int sgc_ctx::getmid()
{
	int retval = BADMID;
#if defined(DEBUG)
	scoped_debug<int> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (_SGC_getmid_mid != BADMID) {
		retval = _SGC_getmid_mid;
		return retval;
	}

	gpp_ctx *GPP = gpp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();
	char *ptr = gpenv::instance().getenv("PMID");
	if (ptr != NULL) {
		_SGC_getmid_pmid = ptr;

		/*
		 * For processes who get their PMID from the environment, we
		 * need to reset the virtual uname.
		 */
		string pnode = _SGC_getmid_pmid;
		string::size_type pos = pnode.find_last_of('.');
		if (pos != string::npos && pos + 1 < pnode.size()
			&& pnode[pos + 1] >= '0' && pnode[pos + 1] <= '9')
			pnode.resize(pos);

		GPP->_GPP_uname = pnode;
	} else if (!GPP->_GPP_uname.empty()) {
		_SGC_getmid_pmid = GPP->_GPP_uname;
	} else {
		struct utsname thishost;
		if (uname(&thishost) < 0) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = UUNAME;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to determine uname")).str(SGLOCALE));
			return retval;
		}

		_SGC_getmid_pmid = thishost.nodename;
	}

	if ((_SGC_getmid_mid = pmid2mid(_SGC_getmid_pmid.c_str())) == BADMID)
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find hostid for hostname {1}") % _SGC_getmid_pmid).str(SGLOCALE));

	retval = _SGC_getmid_mid;
	return retval;
}

int sgc_ctx::getkey()
{
	int mid;
	sgt_ctx *SGT = sgt_ctx::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	if ((mid = getmid()) != BADMID) {
		retval = midkey(mid);
		return retval;
	}

	/*
	 * This loop uses the fact that the PE index upon which the ipckey is
	 * based is computed from the offset of the *MACHINES section entry,
	 * i.e., the first entry read is PE index 0, the 2nd is pe index 1
	 * and so on.
	 */
	for (sg_config::mch_iterator iter = config_mgr.mch_begin(); iter != config_mgr.mch_end(); ++iter) {
		/*
		 * If this is an invalid entry, don't consider it for matching,
		 * we just needed to see it to count mids correctly.
		 */
		if (iter->flags & MA_INVALID)
			continue;

		/*
		 * First check for an exact match.
		 */
		if (_SGC_getmid_pmid == iter->pmid) {
			retval = midkey(iter->mid);
			return retval;
		}

		/*
		 * Now see if we can legitimately match any pe for this node.
		 * If so, match the first one we find with the right node name.
		 */
		string pnode = _SGC_getmid_pmid;
		string::size_type pos = pnode.find_last_of('.');
		if (pos != string::npos && pos + 1 < pnode.size()
			&& pnode[pos + 1] >= '0' && pnode[pos + 1] <= '9')
			pnode.resize(pos);

		if (pnode == iter->pmid) {
			retval = midkey(iter->mid);
			return retval;
		}
	}

	return retval;
}

int sgc_ctx::netkey()
{
	sg_netent_t netent;
	sgt_ctx *SGT = sgt_ctx::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	int mid = getmid();	// make sure _SGC_getmid_pmid is set

	string pnode = _SGC_getmid_pmid;
	string::size_type pos = pnode.find_last_of('.');
	if (pos != string::npos && pos + 1 < pnode.size()
		&& pnode[pos + 1] >= '0' && pnode[pos + 1] <= '9')
		pnode.resize(pos);

	for (sg_config::mch_iterator iter = config_mgr.mch_begin(); iter != config_mgr.mch_end(); ++iter) {
		/*
		 * If this is an invalid entry, don't consider it for matching,
		 * we just needed to see it to count mids correctly.
		 */
		if (iter->flags & MA_INVALID)
			continue;
		/*
		 * Only check if this machine is on our node, doesn't need to
		 * be a full match.
		 */
		if (pnode == iter->pmid) {
			strcpy(netent.lmid, iter->lmid);
			strcpy(netent.netgroup, NM_DEFAULTNET);
			if (!config_mgr.find(netent))
				continue;

			if (proc_mgr.getq(mid, _SGC_wka) < 0)
				continue;

			retval = midkey(mid);
			return retval;
		}
	}

	return retval;
}

bool sgc_ctx::get_bbparms()
{
	gpp_ctx *GPP = gpp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_bbparms_t& bbparms = config_mgr.get_bbparms();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (bbparms.magic != VERSION) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to read the SGPROFILE file, version type mismatch")).str(SGLOCALE));
		return retval;
	}
	if (bbparms.bbrelease != SGRELEASE) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to read the SGPROFILE file, release mismatch")).str(SGLOCALE));
		return retval;
	}
	if (_SGC_gotparms == 2) {
		if (bbparms.sgversion != _SGC_getbbparms_curbbp.sgversion) {
			_SGC_getbbparms_curbbp = bbparms;
			if (!setpeval(_SGC_getbbparms_curbbp))
				return retval;
		}
		retval = true;
		return retval;
	}
	_SGC_getbbparms_curbbp = bbparms;

	if (!set_bba("MP"))
		return retval;

	_SGC_getbbparms_curbbp.magic = (_SGC_bbasw->bbtype & BBT_MASK) | (VERSION >> VSHIFT);
	_SGC_getbbparms_curbbp.pid = getpid();

	bool rtn;
	if (_SGC_getbbparms_curbbp.ldbal == LDBAL_RR)
		rtn = set_ldb("RR");
	else
		rtn = set_ldb("RT");

	if (!rtn)
		return retval;

	_SGC_max_num_msg = _SGC_getbbparms_curbbp.max_num_msg;
	_SGC_max_msg_size = _SGC_getbbparms_curbbp.max_msg_size;
	_SGC_cmprs_limit = _SGC_getbbparms_curbbp.cmprs_limit;
	_SGC_uid = _SGC_getbbparms_curbbp.uid;
	_SGC_gid = _SGC_getbbparms_curbbp.gid;
	_SGC_perm = _SGC_getbbparms_curbbp.perm;
	_SGC_wka = _SGC_getbbparms_curbbp.ipckey;
	_SGC_do_remedy = _SGC_getbbparms_curbbp.remedy;

	if(_SGC_getbbparms_curbbp.options & SGLAN) {
		// turn on LAN bit (to distinguish from MP model
		_SGC_bbasw->bbtype |= BBF_LAN;
	}

	/*
	 * Reset _SGC_gotparms to 1 if we're not attached to indicate that we've
	 * gotten the parms but they can and should be reset on a subsequent
	 * call to get_bbparms.
	 * Reset _SGC_gotparms to 2 if we're attached to indicate that only pe
	 * specific stuff should be reset on subsequent calls if necessary.
	 */
	_SGC_gotparms = (_SGC_crte != NULL) ? 2 : 1;

	// update the local max info from the PE info
	if(!setpeval(_SGC_getbbparms_curbbp))
		return retval;

	retval = true;
	return retval;
}

void sgc_ctx::qcleanup(bool rmflag)
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sg_proc& proc_mgr = sg_proc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!_SGC_here_gotten)
		return;

	if (rmflag)
		proc_mgr.removeq(_SGC_proc.rqaddr);

	if (_SGC_proc.rpaddr >= 0 && _SGC_proc.rpaddr != _SGC_proc.rqaddr)
		proc_mgr.removeq(_SGC_proc.rpaddr);

	_SGC_here_gotten = false;
}

int sgc_ctx::lmid2mid(const char *lmid) const
{
	if (lmid == NULL || lmid[0] == '\0')
		return BADMID;

	if (_SGC_bbp == NULL)
		return computemid(lmid, LMID2MID);

	for (int i = 0; i < _SGC_bbp->bbparms.maxpes; i++) {
		if (_SGC_ptes[i].flags & MA_INVALID)
			continue;

		if (strcasecmp(lmid, _SGC_ptes[i].lname) == 0)
			return _SGC_ptes[i].mid;
	}

	return BADMID;
}

int sgc_ctx::pmid2mid(const char *pmid) const
{
	if (pmid == NULL || pmid[0] == '\0')
		return BADMID;

	if (_SGC_bbp == NULL)
		return computemid(pmid, PMID2MID);

	for (int i = 0; i < _SGC_bbp->bbparms.maxpes; i++) {
		if (_SGC_ptes[i].flags & MA_INVALID)
			continue;

		if (strcasecmp(pmid, _SGC_ptes[i].pname) == 0)
			return _SGC_ptes[i].mid;
	}

	return BADMID;
}

const char * sgc_ctx::mid2lmid(int mid) const
{
	if (mid < 0 || _SGC_ptes == NULL)
		return NULL;
	else
		return _SGC_ptes[midpidx(mid)].lname;
}

const char * sgc_ctx::mid2pmid(int mid) const
{
	if (mid < 0 || _SGC_ptes == NULL)
		return NULL;
	else
		return _SGC_ptes[midpidx(mid)].pname;
}

sg_pte_t * sgc_ctx::mid2pte(int mid)
{
	if (mid < 0 || _SGC_ptes == NULL)
		return NULL;
	else
		return &_SGC_ptes[midpidx(mid)];
}

const sg_pte_t * sgc_ctx::mid2pte(int mid) const
{
	if (mid < 0 || _SGC_ptes == NULL)
		return NULL;
	else
		return &_SGC_ptes[midpidx(mid)];
}

const char * sgc_ctx::pmtolm(int mid)
{
	const char *s = mid2lmid(mid);
	return (s ? s : "?");
}

long sgc_ctx::netload(int mid) const
{
	sgp_ctx *SGP = sgp_ctx::instance();

	if (SGP->_SGP_netload == -1) {
		char *ptr = gpenv::instance().getenv("SGNETLOAD");
		if (ptr != NULL)
			SGP->_SGP_netload = atol(ptr);
		else
			SGP->_SGP_netload = -2;
	}

	if (remote(mid)) {
		if (SGP->_SGP_netload >= 0)
			return SGP->_SGP_netload;

		const sg_pte_t *pte = mid2pte(_SGC_proc.mid);
		if (pte != NULL)
			return pte->netload;

		return 0;
	}

	return 0;
}

const char * sgc_ctx::mid2node(int mid) const
{
	if (mid < 0 || _SGC_ntes == NULL)
		return NULL;
	else
		return _SGC_ntes[midnidx(mid)].pnode;
}


/*
 * Finds the acting master node for a configuration. The search is on
 * either the specified mid or on the master/backup specified in the
 * config file if ALLMID is passed as the mid.
 */
sg_proc_t * sgc_ctx::fmaster(int mid, sg_proc_t *c_proc)
{
	static sg_proc_t l_proc;
	sg_proc_t *proc;
	int mids[MAX_MASTERS];	/* To save the master and backup mids */
	gpp_ctx *GPP = gpp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_wkinfo_t wkinfo;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("mid={1}") % mid).str(SGLOCALE), NULL);
#endif

	proc = (c_proc != NULL) ? c_proc : &l_proc;

	if (mid == ALLMID) {
		if (_SGC_bbp == NULL) {
			if (SGT->_SGT_error_code == 0) {
				SGT->_SGT_error_code = SGESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot find master without BB")).str(SGLOCALE));
			}
			return NULL;
		}

		int local_mid = -1;
		int i;

		for (i = 0; i < MAX_MASTERS; i++) {
			mids[i] = lmid2mid(_SGC_bbp->bbparms.master[i]);
			if (midnidx(mids[i]) == midnidx(_SGC_proc.mid))
				local_mid = i;
		}


		// If the config backup is local, then check it first.
		if (local_mid != -1) {
			if (fmaster(mids[local_mid], proc) != NULL)
				return proc;
		}

		for (i = 0; i < MAX_MASTERS; i++) {
			if (i == local_mid)
				continue;
			if (fmaster(mids[local_mid], proc) != NULL)
				return proc;
		}

		return NULL;
	}

	// If the mid to be checked is invalid return an error immediately.
	if (mid < 0)
		return NULL;

	/*
	 * If it is local, then check right here in this function, otherwise
	 * format and send an RPC remotely.
	 */
	if (midnidx(mid) != midnidx(_SGC_proc.mid)) {
		sg_rpc_rply_t *rply;
		tafmast_t tafmast;
		sg_agent& agent_mgr = sg_agent::instance(SGT);

		tafmast.mid = mid;
		tafmast.ipckey = _SGC_wka;
		if((rply = agent_mgr.send(mid, OT_FMASTER, &tafmast, sizeof(tafmast))) == NULL)
			return NULL;

		if (rply->rtn < 0)
			return NULL;

		*proc = *reinterpret_cast<sg_proc_t *>(rply->data());
		return proc;
	}

	/*
	 * If we can get the wkpid from the semaphore we'll return a
	 * proc. We may not be able to get a valid queue if we're in
	 * SHM mode. We can't really check our mode here though since
	 * this may be executed by the tagent which does not have _tmbbp set.
	 */
	if (proc_mgr.get_wkinfo(_SGC_wka, wkinfo)) {
		proc->mid = mid;
		proc->grpid = CLST_PGID;
		proc->svrid = MNGR_PRCID;
		proc->pid = wkinfo.pid;
		proc->rqaddr = 0;
		proc->rpaddr = 0;
		if (wkinfo.pid <= 0 || wkinfo.pid == _SGC_proc.pid || !proc_mgr.alive(*proc))
			return NULL;

		if (proc_mgr.getq(mid, wkinfo.qaddr) > 0) {
			proc->rqaddr = wkinfo.qaddr;
			proc->rpaddr = wkinfo.qaddr;
		}

		return proc;
	}

	return NULL;
}

const char * sgc_ctx::admcnm(int slot)
{
	const char *rv;

	switch (slot) {
	case AT_BBM:
		rv = "sgmngr";
		break;
	case AT_ADMIN:
		rv = "sgadmin";
		break;
	case AT_RESTART:
		rv = "sgrecover";
		break;
	case AT_CLEANUP:
		rv = "sgclear";
		break;
	case AT_SHUTDOWN:
		rv = "sgshut";
		break;
	default:
		rv = "";
		break;
	}

	return rv;
}

const char * sgc_ctx::admuname()
{
	struct passwd * passwd = getpwuid(_SGC_bbp->bbparms.uid);
	if (passwd == NULL)
		return "";
	else
		return passwd->pw_name;
}

int sgc_ctx::computemid(const char *name, int scope) const
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	int pidx = 0;
	int nidx;
	vector<string> nodes;
	int retval = BADMID;
#if defined(DEBUG)
	scoped_debug<int> debug(500, __PRETTY_FUNCTION__, (_("name={1}, scope={2}") % name % scope).str(SGLOCALE), NULL);
#endif

	for (sg_config::mch_iterator iter = config_mgr.mch_begin(); iter != config_mgr.mch_end(); ++iter, pidx++) {
		sg_mchent_t& mchent = *iter;

		vector<string>::const_iterator node_iter = std::find(nodes.begin(), nodes.end(), mchent.pmid);
		if (node_iter == nodes.end()) {
			nidx = nodes.size();
			nodes.push_back(mchent.pmid);
		} else {
			nidx = node_iter - nodes.begin();
		}

		if (mchent.flags & MA_INVALID)
			continue;

		switch (scope) {
		case PMID2MID:
			if (strcasecmp(mchent.pmid, name) != 0)
				continue;
			break;
		case LMID2MID:
			if (strcasecmp(mchent.lmid, name) != 0)
				continue;
			break;
		default:
			return retval;
		}

		retval = idx2mid(nidx, pidx);
		return retval;
	}

	return retval;
}

/*
 * set pe-specific values in the bbparms structure
 *   -  if the pe table exists, use those values
 *   -  if the pe table doesn't exist, use the machine entry from the config
 */
bool sgc_ctx::setpeval(sg_bbparms_t& bbparms)
{
	sg_mchent_t mchent;
	gpp_ctx *GPP = gpp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (_SGC_crte == NULL) {
		mchent.mid = getmid();
		if (!config_mgr.find(mchent)) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to locate node entry in the Bulletin Board")).str(SGLOCALE));
			return retval;
		}
		if (mchent.maxaccsrs > 0)
			bbparms.maxaccsrs = mchent.maxaccsrs;
		if (mchent.uid > 0)
			bbparms.uid = mchent.uid;
		if (mchent.gid > 0)
			bbparms.gid = mchent.gid;
		if (mchent.perm > 0)
			bbparms.perm = mchent.perm;
	} else {
		int idx = midpidx(_SGC_proc.mid);
		if (_SGC_ptes[idx].maxaccsrs > 0)
			bbparms.maxaccsrs = _SGC_ptes[idx].maxaccsrs;
		if (_SGC_ptes[idx].uid > 0)
			bbparms.uid = _SGC_ptes[idx].uid;
		if (_SGC_ptes[idx].gid > 0)
			bbparms.gid = _SGC_ptes[idx].gid;
		if (_SGC_ptes[idx].perm > 0)
			bbparms.perm = _SGC_ptes[idx].perm;
	}

	if ((_SGC_bbasw->bbtype & BBT_MASK) == BBT_MP) {
  		if ((bbparms.ipckey = getkey()) < 0) {
			if (SGT->_SGT_error_code == 0)
				SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to map ID of a node to ipckey")).str(SGLOCALE));
			return retval;
		}
	}

	retval = true;
	return retval;
}

bool sgc_ctx::set_bba(const string& name)
{
	try {
		sgp_ctx *SGP = sgp_ctx::instance();
		SGP->_SGP_bbasw_mgr->set_switch(name);
		return true;
	} catch(exception& ex) {
		return false;
	}
}

bool sgc_ctx::set_ldb(const string& name)
{
	try {
		sgp_ctx *SGP = sgp_ctx::instance();
		SGP->_SGP_ldbsw_mgr->set_switch(name);
		return true;
	} catch(exception& ex) {
		return false;
	}
}

bool sgc_ctx::set_auth(const string& name)
{
	try {
		sgp_ctx *SGP = sgp_ctx::instance();
		SGP->_SGP_authsw_mgr->set_switch(name);
		return true;
	} catch(exception& ex) {
		return false;
	}
}

void sgc_ctx::inithdr(sg_message& msg) const
{
	sg_metahdr_t& metahdr = msg;
	metahdr.protocol = SGPROTOCOL;
	metahdr.flags = 0;
	metahdr.mid = _SGC_proc.mid;
}

void sgc_ctx::clear()
{
	memset(_SGC_handleflags, '\0', sizeof(_SGC_handleflags));
	memset(_SGC_handles, '\0', sizeof(_SGC_handleflags));
	_SGC_hdllastfreed = -1;

	_SGC_proc.pid = 0;
	_SGC_here_gotten = false;

	_SGC_sndmsg_wkidset = false;
	_SGC_sndmsg_wkid = 0;
	_SGC_sndmsg_wkproc.clear();
	_SGC_findgw_version = -1;
	_SGC_findgw_nextcache = -1;

	_SGC_svc_rounds = 0;
	_SGC_mbrtbl_curraddr.clear();
	_SGC_mbrtbl_raddr = NULL;

	_SGC_getmid_mid = BADMID;
	_SGC_getmid_pmid = "";

	memset(_SGC_handlesvcindex, 0, sizeof(_SGC_handlesvcindex));
	_SGC_getrply_count = 0;
	_SGC_receive_getanycnt = 0;
	_SGC_rcved_msgs.clear();
	_SGC_rcvrq_countdown = STARVPROTECT;
	_SGC_rcvrq_cpending = 1;

	_SGC_gotdflts = false;
	_SGC_gotparms = 0;
}

void sgc_ctx::bbfail()
{
	sgp_ctx *SGP = sgp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();

	if (strcmp(_SGC_authsw->name, "SGMNGR") != 0) {
		detach();
		return;
	} else {
		if (_SGC_didregister) {
			sg_rte& rte_mgr = sg_rte::instance(SGT);
			rte_mgr.leave(NULL);
			_SGC_didregister = false;
		}
	}

	// 清理消息队列时需要共享内存存在
	qcleanup(true);

	if (_SGC_bbp != NULL) {
		region_mgr.reset();
		mapping_mgr.reset();
		_SGC_bbp = NULL;
		_SGC_ptes = NULL;
		_SGC_ntes = NULL;
	}

	SGP->_SGP_amserver = false;
}

void sgc_ctx::sigalrm(int signo)
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	if (bbp == NULL) {
		alarm(1);
		return;
	}

	bbp->bbmeters.timestamp = time(0);
	alarm(1);
}

}
}

