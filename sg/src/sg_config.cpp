#include "sg_internal.h"
#include <regex.h>

namespace ai
{
namespace sg
{

boost::once_flag sg_config::once_flag = BOOST_ONCE_INIT;
sg_config * sg_config::_instance = NULL;

sg_config& sg_config::instance(sgt_ctx *SGT, bool auto_open)
{
	if (!SGT->_SGT_sgconfig_opened) {
		if (_instance == NULL)
			boost::call_once(once_flag, sg_config::init_once);

		if (auto_open && !_instance->open())
			exit(1);
	}

	return *_instance;
}

bool sg_config::open(const string& sgconfig_)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif
	sg_cfg_entry_t map_info;
	sgt_ctx *SGT = sgt_ctx::instance();
	bi::scoped_lock<boost::mutex> lock(cfg_mutex);

	if (SGT->_SGT_sgconfig_opened) {
		retval = true;
		return retval;
	}

	if (ref_count > 0)
		goto OK_LABEL;

	if (sgconfig_.empty()) {
		char *ptr = gpenv::instance().getenv("SGPROFILE");
		if (ptr == NULL) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: SGPROFILE is not set")).str(SGLOCALE));
			return retval;
		}

		sgconfig = ptr;
	} else {
		sgconfig = sgconfig_;
		string env = "SGPROFILE=";
		env += sgconfig;
		gpenv::instance().putenv(env.c_str());
	}

	try {
		mapping_mgr.reset(new bi::file_mapping(sgconfig.c_str(), bi::read_write));
	} catch (bi::interprocess_exception& ex) {
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = ex.get_native_error();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open SGPROFILE file {1}, error_code = {2}, native_error = {3}")
			 % sgconfig % ex.get_error_code()% ex.get_native_error()).str(SGLOCALE));
		return retval;
	}

	region_mgr.reset(new bi::mapped_region(*mapping_mgr, bi::read_write));

	base_addr = reinterpret_cast<char *>(region_mgr->get_address());
	cfg_map = reinterpret_cast<sg_cfg_map_t *>(base_addr);

	get_page_info(sizeof(sg_cfg_map_t), 1, map_info);
	bbp = reinterpret_cast<sg_bbparms_t *>(base_addr + map_info.pages * CFG_PAGE_SIZE);

OK_LABEL:
	ref_count++;
	SGT->_SGT_sgconfig_opened = true;

	retval = true;
	return retval;
}

void sg_config::close()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif
	sgt_ctx *SGT = sgt_ctx::instance();
	bi::scoped_lock<boost::mutex> lock(cfg_mutex);

	if (!SGT->_SGT_sgconfig_opened)
		return;

	SGT->_SGT_sgconfig_opened = false;

	if (--ref_count > 0)
		return;

	region_mgr.reset();
	mapping_mgr.reset();
	base_addr = NULL;
	cfg_map = NULL;
	bbp = NULL;
	sgconfig.clear();
}

bool sg_config::reopen(const string& sgconfig_)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	close();
	retval = open(sgconfig_);
	return retval;
}

bool sg_config::init(int pes, int nodes, int sgt, int svrs, int svcs, int perm)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("pes={1}, nodes={2}, sgt={3}, svrs={4}, svcs={5}, perm={6}") % pes % nodes % sgt % svrs % svcs % perm).str(SGLOCALE), &retval);
#endif
	sgt_ctx *SGT = sgt_ctx::instance();

	sg_cfg_entry_t map_info;
	sg_cfg_entry_t bbp_info;
	sg_cfg_entry_t pe_info;
	sg_cfg_entry_t node_info;
	sg_cfg_entry_t sgt_info;
	sg_cfg_entry_t svr_info;
	sg_cfg_entry_t svc_info;

	get_page_info(sizeof(sg_cfg_map_t), 1, map_info);
	get_page_info(sizeof(sg_bbparms_t), 1, bbp_info);
	get_page_info(sizeof(sg_mchent_t), pes, pe_info);
	get_page_info(sizeof(sg_netent_t), nodes, node_info);
	get_page_info(sizeof(sg_sgent_t), sgt, sgt_info);
	get_page_info(sizeof(sg_svrent_t), svrs, svr_info);
	get_page_info(sizeof(sg_svcent_t), svcs, svc_info);

	int pages = map_info.pages + bbp_info.pages + pe_info.pages
		+ node_info.pages + sgt_info.pages + svr_info.pages + svc_info.pages;
	size_t size = pages * CFG_PAGE_SIZE;

	if (sgconfig.empty()) {
		char *ptr = gpenv::instance().getenv("SGPROFILE");
		if (ptr == NULL) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: SGPROFILE is not set")).str(SGLOCALE));
			return retval;
		}

		sgconfig = ptr;
	}

	try {
		bi::file_mapping::remove(sgconfig.c_str());
		if (creat(sgconfig.c_str(), perm) == -1) {
			SGT->_SGT_error_code = SGEOS;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create SGPROFILE file {1}") % sgconfig).str(SGLOCALE));
			return retval;
		}

		if (truncate(sgconfig.c_str(), size) == -1) {
			SGT->_SGT_error_code = SGEOS;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't truncate SGPROFILE file {1}") % sgconfig).str(SGLOCALE));
			return retval;
		}

		mapping_mgr.reset(new bi::file_mapping(sgconfig.c_str(), bi::read_write));
	} catch (bi::interprocess_exception& ex) {
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = ex.get_native_error();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create SGPROFILE file {1}, error_code = {2}, native_error = {3}")
			 % sgconfig % ex.get_error_code()% ex.get_native_error()).str(SGLOCALE));
		return retval;
	}

	region_mgr.reset(new bi::mapped_region(*mapping_mgr, bi::read_write));

	base_addr = reinterpret_cast<char *>(region_mgr->get_address());

	int offset = 0;
	cfg_map = reinterpret_cast<sg_cfg_map_t *>(base_addr);

	cfg_map->pages = pages;
	offset += map_info.pages * CFG_PAGE_SIZE;

	bbp = reinterpret_cast<sg_bbparms_t *>(base_addr + offset);

	cfg_map->bbp_use = offset;
	offset += bbp_info.pages * CFG_PAGE_SIZE;

	cfg_map->pe_info = pe_info;
	cfg_map->pe_info.entry_free = offset;
	init_pages(pe_info, offset);
	offset += pe_info.pages * CFG_PAGE_SIZE;

	cfg_map->node_info = node_info;
	cfg_map->node_info.entry_free = offset;
	init_pages(node_info, offset);
	offset += node_info.pages * CFG_PAGE_SIZE;

	cfg_map->sgt_info = sgt_info;
	cfg_map->sgt_info.entry_free = offset;
	init_pages(sgt_info, offset);
	offset += sgt_info.pages * CFG_PAGE_SIZE;

	cfg_map->svr_info = svr_info;
	cfg_map->svr_info.entry_free = offset;
	init_pages(svr_info, offset);
	offset += svr_info.pages * CFG_PAGE_SIZE;

	cfg_map->svc_info = svc_info;
	cfg_map->svc_info.entry_free = offset;
	init_pages(svc_info, offset);

	retval = true;
	return retval;
}

bool sg_config::flush()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	retval = region_mgr->flush();
	return retval;
}

sg_bbparms_t& sg_config::get_bbparms()
{
	return *bbp;
}

sg_config::mch_iterator sg_config::mch_begin()
{
	return mch_iterator(cfg_map, cfg_map->pe_info.entry_use);
}

sg_config::mch_iterator sg_config::mch_end()
{
	return mch_iterator();
}

sg_config::net_iterator sg_config::net_begin()
{
	return net_iterator(cfg_map, cfg_map->node_info.entry_use);
}

sg_config::net_iterator sg_config::net_end()
{
	return net_iterator();
}

sg_config::sgt_iterator sg_config::sgt_begin()
{
	return sgt_iterator(cfg_map, cfg_map->sgt_info.entry_use);
}

sg_config::sgt_iterator sg_config::sgt_end()
{
	return sgt_iterator();
}

sg_config::svr_iterator sg_config::svr_begin()
{
	return svr_iterator(cfg_map, cfg_map->svr_info.entry_use);
}

sg_config::svr_iterator sg_config::svr_end()
{
	return svr_iterator();
}

sg_config::svc_iterator sg_config::svc_begin()
{
	return svc_iterator(cfg_map, cfg_map->svc_info.entry_use);
}

sg_config::svc_iterator sg_config::svc_end()
{
	return svc_iterator();
}

sg_mchent_t * sg_config::find_by_lmid(const string& lmid)
{
	mch_iterator iter;
	for (iter = mch_begin(); iter != mch_end(); ++iter) {
		if (lmid == iter->lmid)
			return &(*iter);
	}

	return NULL;
}

bool sg_config::find(sg_mchent_t& mchent)
{
	mch_iterator iter = find_internal(mchent);
	if (iter == mch_end())
		return false;

	int mid = mchent.mid;
	mchent = *iter;
	mchent.mid = mid;
	return true;
}

bool sg_config::find(sg_netent_t& netent)
{
	net_iterator iter = find_internal(netent);
	if (iter == net_end())
		return false;

	netent = *iter;
	return true;
}

bool sg_config::find(sg_sgent_t& sgent)
{
	sgt_iterator iter = find_internal(sgent);
	if (iter == sgt_end())
		return false;

	sgent = *iter;
	return true;
}

// svrent中设置了grpid+svrid
bool sg_config::find(sg_svrent_t& svrent)
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
	sg_proc_t saved_proc = svrent.sparms.svrproc;

	if (saved_proc.svrid == MNGR_PRCID || saved_proc.svrid == GWS_PRCID) {
		memset(&svrent, 0, sizeof(svrent));
		svrent.sparms.svrproc = saved_proc;
		svrent.sparms.rqperm = SGC->_SGC_perm;
		svrent.sparms.rpperm = SGC->_SGC_perm;
		svrent.sparms.svridmax = saved_proc.svrid;
		svrent.sparms.max_num_msg = SGC->_SGC_max_num_msg;
		svrent.sparms.max_msg_size = SGC->_SGC_max_msg_size;
		svrent.sparms.cmprs_limit = SGC->_SGC_cmprs_limit;
		svrent.sparms.remedy = SGC->_SGC_do_remedy;
		svrent.sparms.rqparms.options = RESTARTABLE;
		svrent.sparms.rqparms.grace = DFLT_RSTINT;
		svrent.sparms.rqparms.maxgen = std::numeric_limits<int>::max();

		svrent.sparms.rqparms.rcmd[0] = '\0';
		svrent.bparms.flags = 0;
		svrent.bparms.bootmin = 1;
		svrent.bparms.clopt[0] = '\0';
		svrent.bparms.envfile[0] = '\0';

		if (saved_proc.is_bbm()) {
			strcpy(svrent.sparms.rqparms.filename, "sgmngr");
		} else if (saved_proc.is_gws()) {
			strcpy(svrent.sparms.rqparms.filename, "sggws");
			SGP->_SGP_boot_flags |= BF_REPLYQ;

			// 调整GWS的clopt选项
			sg_mchent_t mchent;
			mchent.mid = SGC->grp2mid(svrent.sparms.svrproc.grpid);
			mch_iterator mch_iter = find_internal(mchent);
			if (mch_iter != mch_end())
				strcpy(svrent.bparms.clopt, mch_iter->clopt);
		} else { // sgclst
			sprintf(svrent.sparms.rqparms.saddr, "%d", SGC->_SGC_wka);
			strcpy(svrent.sparms.rqparms.filename, "sgclst");
		}

		SGP->_SGP_svridmin = saved_proc.svrid;
		return true;
	}

	svr_iterator iter = find_internal(svrent);
	if (iter == svr_end())
		return false;

	svrent = *iter;
	svrent.sparms.svrproc = saved_proc;
	if (svrent.sparms.rqperm == 0)
		svrent.sparms.rqperm = SGC->_SGC_perm;
	if (svrent.sparms.rpperm == 0)
		svrent.sparms.rpperm = SGC->_SGC_perm;
	if (svrent.sparms.max_num_msg == 0)
		svrent.sparms.max_num_msg = SGC->_SGC_max_num_msg;
	if (svrent.sparms.max_msg_size == 0)
		svrent.sparms.max_msg_size = SGC->_SGC_max_msg_size;

	SGP->_SGP_svridmin = iter->sparms.svrproc.svrid;
	return true;
}

bool sg_config::find(sg_svcparms_t& svcparms, int grpid)
{
	if (strcmp(svcparms.svc_name, DBBM_SVC_NAME) == 0
		|| strcmp(svcparms.svc_name, BBM_SVC_NAME) == 0
		|| strcmp(svcparms.svc_name, GW_SVC_NAME) == 0)
		goto DEFAULT_LABEL;

	{
		svc_iterator iter = find_internal(svcparms, grpid);
		if (iter == svc_end())
			goto DEFAULT_LABEL;

		svcparms.priority = iter->priority;
		svcparms.svc_policy = iter->svc_policy;
		svcparms.svc_load = iter->svc_load;
		svcparms.load_limit = iter->load_limit;
		svcparms.svctimeout = iter->svctimeout;
		svcparms.svcblktime = iter->svcblktime;
		return true;
	}

DEFAULT_LABEL:
	svcparms.priority = DFLT_PRIORITY;
	svcparms.svc_policy = DFLT_POLICY;
	svcparms.svc_load = DFLT_COST;
	svcparms.load_limit = DFLT_COSTLIMIT;
	svcparms.svctimeout = DFLT_TIMEOUT;
	svcparms.svcblktime = DFLT_BLKTIME;
	return true;
}

bool sg_config::insert(const sg_mchent_t& mchent)
{
	if (cfg_map->pe_info.entries >= bbp->maxpes)
		return false;

	return insert_internal(mchent, cfg_map->pe_info);
}

bool sg_config::insert(const sg_netent_t& netent)
{
	if (cfg_map->node_info.entries >= bbp->maxnodes)
		return false;

	return insert_internal(netent, cfg_map->node_info);
}

bool sg_config::insert(const sg_sgent_t& sgent)
{
	if (cfg_map->sgt_info.entries >= bbp->maxsgt)
		return false;

	return insert_internal(sgent, cfg_map->sgt_info);
}

bool sg_config::insert(const sg_svrent_t& svrent)
{
	if (cfg_map->svr_info.entries >= bbp->maxsvrs)
		return false;

	return insert_internal(svrent, cfg_map->svr_info);
}

bool sg_config::insert(const sg_svcent_t& svcent)
{
	if (cfg_map->svc_info.entries >= bbp->maxsvcs)
		return false;

	return insert_internal(svcent, cfg_map->svc_info);
}

bool sg_config::erase(const sg_mchent_t& mchent)
{
	return erase_internal(mchent, cfg_map->pe_info);
}

bool sg_config::erase(const sg_netent_t& netent)
{
	return erase_internal(netent, cfg_map->node_info);
}

bool sg_config::erase(const sg_sgent_t& sgent)
{
	return erase_internal(sgent, cfg_map->sgt_info);
}

bool sg_config::erase(const sg_svrent_t& svrent)
{
	return erase_internal(svrent, cfg_map->svr_info);
}

bool sg_config::erase(const sg_svcent_t& svcent)
{
	return erase_internal(svcent, cfg_map->svc_info);
}

bool sg_config::propagate(const sg_mchent_t& mchent)
{
	static taprop_t *taprop = NULL;	/* server.xml from the prev time */
	static int sgversion = 0;			/* sg version from the prev time */

	sg_rpc_rply_t *rply;			/* reply message */
	char xmlname[L_tmpnam];		/* temporary file for XML config */
	int fd;						/* file descriptor for prop files*/
	size_t propsz;					/* propagated file size */
	struct stat sb;					/* stat buffer */
	sg_mchent_t lmchent;
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
	sg_agent& agent_mgr = sg_agent::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (mchent.mid == SGC->_SGC_proc.mid || (SGC->_SGC_ptes[SGC->midpidx(mchent.mid)].flags & NP_SYS1)) {
		/*
		 * If target is this machine, or sgboot has already propagated
		 * to the target machine, don't send it again.  Make sure it is
		 * marked if sgboot.
		 */
		SGC->_SGC_ptes[SGC->midpidx(SGC->_SGC_proc.mid)].flags |= NP_SYS1;
		retval = true;
		return retval;
	}

	lmchent.mid = SGC->_SGC_proc.mid;
	if (!find(lmchent))
		return retval;

	// See if the remote machine is already up-to-date.
	if((rply = agent_mgr.send(mchent.mid, OT_SGVERSION, &mchent, sizeof(mchent))) == NULL) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot send message to sgagent on {1}") % mchent.mid).str(SGLOCALE));
		return retval;
	}

	if (rply->rtn >= 0 && *reinterpret_cast<int *>(rply->data()) == SGC->_SGC_bbp->bbparms.sgversion) {
		SGC->_SGC_ptes[SGC->midpidx(mchent.mid)].flags |= NP_SYS1;
		retval = true;
		return retval;
	}

	/*
	 * If all sites are the current release, then dump the config file
	 * once.  If alternating between old and current release, need to
	 * dump the config file for each old machine and each time the
	 * current release occurs after an old release machine (i.e.,
	 * still optimize if there is a string of current release machines).
	 */
	// Check if SGPROFILE has changed.
	if (sgversion != bbp->sgversion) {
		// Invalidate cache
		if (taprop != NULL) {
			delete []reinterpret_cast<char *>(taprop);
			taprop = NULL;
			sgversion = 0;
		}
		if (bbp)	/* save current version */
			sgversion = bbp->sgversion;
	}

	if(taprop == NULL) {		/* No cache */
		// Get a temporary file name for sgunpack output
		if (tmpnam(xmlname) == NULL)
			return retval;

		/*
		 * Save the current umask and protect the temporary file about to be created with 0077
		 * umask permissions to prevent unwanted read-access by other users running on that machine
		 */
		mode_t savmask = umask(0077);
		BOOST_SCOPE_EXIT((&savmask)) {
			umask(savmask);
		} BOOST_SCOPE_EXIT_END

		/* Create the sgunpack command line. */
		string cmd = (boost::format("%1%/bin/sgunpack -s -H %2%") % lmchent.sgdir % xmlname).str();

		sig_action sig(SIGCHLD, SIG_DFL);

		if (sys_func::instance().gp_status(system(cmd.c_str())) != 0 || stat(xmlname, &sb) < 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not sgunpack config file for propagation to remote site {1}") % mchent.lmid).str(SGLOCALE));
			return retval;
		}

		// Get buffer for message propagation.
		propsz = sizeof(taprop_t) + sb.st_size - sizeof(taprop->data);
		taprop = reinterpret_cast<taprop_t *>(new char[propsz]);

		// Now read the unloaded config file into memory for sending.
		if ((fd = ::open(xmlname, O_RDONLY, 0400)) == -1
			|| (taprop->size = read(fd, taprop->data, sb.st_size)) <= 0) {
			delete []reinterpret_cast<char *>(taprop);
			taprop = NULL;
			if (fd >= 0)
				::close(fd);
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not access unloaded configuration file for propagation")).str(SGLOCALE));
			return retval;
		}

		/*
		 * now reset propsz to the actual data read since
		 * stat() and read() do not return the same file
		 * size on NT and Netware for text files
		 */
		propsz = sizeof(taprop_t) + taprop->size - sizeof(taprop->data);
		::close(fd);
	} else {	/* use cache */
		propsz = sizeof(taprop_t) + taprop->size - sizeof(taprop->data);
	}

	// Copy in target machine entry.
	taprop->mchent = mchent;
	memcpy(taprop->passwd, bbp->passwd, MAX_PASSWD);
	taprop->passwd[MAX_PASSWD] = '\0';

	// Now do the SGPROFILE propagation to the site.
	if((rply = agent_mgr.send(mchent.mid, OT_SGPROP, taprop, propsz)) == NULL) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot send message to sgagent on {1}") % mchent.mid).str(SGLOCALE));
		return retval;
	}

	if (rply->rtn < 0) {
		// Populate status message back to user.
		switch (rply->num) {
		case TGESND:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Send failure on SGPROFILE propagation to {1}") % mchent.lmid).str(SGLOCALE));
			break;
		case TGERCV:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Receive failure on SGPROFILE propagation to {1}") % mchent.lmid).str(SGLOCALE));
			break;
		case TGEFILE:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Could not create remote tmp file on {1} for {2} creation") % mchent.lmid % mchent.sgconfig).str(SGLOCALE));
			break;
		case TGELOADCF:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgpack error on {1} for SGPROFILE file {2} creation") % mchent.lmid % mchent.sgconfig).str(SGLOCALE));
			if (rply->datalen) {
				/* Format sgpack output */
				cerr << "\n" << (_("ERROR: sgpack error on {1} sgpack output:\n") % mchent.lmid).str(SGLOCALE)
					<< std::setw(rply->datalen) << rply->data();
			}
			break;
		default:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown error propagating SGPROFILE file to {1}") % mchent.lmid).str(SGLOCALE));
			break;
		}

		return retval;
	}

	retval = true;
	return retval;
}

sg_config::sg_config()
{
	GPP = gpp_ctx::instance();
	SGP = sgp_ctx::instance();

	ref_count = 0;
}

sg_config::~sg_config()
{
}

char * sg_config::get_entry(sg_cfg_entry_t& cfg_entry)
{
	if (cfg_entry.entry_free == -1)
		alloc_pages(cfg_entry);

	int curr_offset = cfg_entry.entry_free;
	sg_syslink_t *curr = reinterpret_cast<sg_syslink_t *>(base_addr + curr_offset);
	cfg_entry.entry_free = curr->flink;
	sg_syslink_t *next = reinterpret_cast<sg_syslink_t *>(base_addr + cfg_entry.entry_free);
	next->blink = -1;

	if (cfg_entry.entry_use == -1) {
		cfg_entry.entry_use = curr_offset;
		curr->blink = -1;
		curr->flink = -1;
	} else {
		// 找到最后一个使用节点
		int entry_use = cfg_entry.entry_use;
		sg_syslink_t *use_entry;
		while (1) {
			use_entry = reinterpret_cast<sg_syslink_t *>(base_addr + entry_use);
			if (use_entry->flink == -1)
				break;

			entry_use = use_entry->flink;
		}


		use_entry->flink = curr_offset;
		curr->blink = entry_use;
		curr->flink = -1;
	}

	cfg_entry.entries++;
	return reinterpret_cast<char *>(curr + 1);
}

void sg_config::put_entry(sg_cfg_entry_t& cfg_entry, void *data)
{
	sg_syslink_t *curr = reinterpret_cast<sg_syslink_t *>(reinterpret_cast<char *>(data) - sizeof(sg_syslink_t));
	int curr_offset = reinterpret_cast<char *>(curr) - base_addr;

	if (curr->blink != -1) {
		sg_syslink_t *prev = reinterpret_cast<sg_syslink_t *>(base_addr + curr->blink);
		prev->flink = curr->flink;
	} else {
		cfg_entry.entry_use = curr->flink;
	}

	if (curr->flink != -1) {
		sg_syslink_t *next = reinterpret_cast<sg_syslink_t *>(base_addr + curr->flink);
		next->blink = curr->blink;
	}

	curr->blink = -1;
	curr->flink = cfg_entry.entry_free;
	if (cfg_entry.entry_free != -1) {
		sg_syslink_t *free_entry = reinterpret_cast<sg_syslink_t *>(base_addr + cfg_entry.entry_free);
		free_entry->blink = curr_offset;
	}

	cfg_entry.entry_free = curr_offset;
	cfg_entry.entries--;
}

void sg_config::get_page_info(size_t data_size, int entries, sg_cfg_entry_t& cfg_entry)
{
	int entry_size = common::round_up(static_cast<int>(data_size), LONG_ALIGN) + sizeof(sg_syslink_t);
	int page_size = CFG_PAGE_SIZE;
	int per_pages = 1;
	while (entry_size > page_size) {
		page_size <<= 1;
		per_pages <<= 1;
	}

	if (entries <= 0)
		entries = 1;

	int per_entries = page_size / entry_size;

	cfg_entry.per_pages = per_pages;
	cfg_entry.per_entries = per_entries;
	cfg_entry.data_size = data_size;
	cfg_entry.entry_size = entry_size;
	cfg_entry.pages = (entries + per_entries - 1) / per_entries * per_pages;
	cfg_entry.entries = 0;
	cfg_entry.entry_use = -1;
	cfg_entry.entry_free = -1;
}

void sg_config::init_pages(sg_cfg_entry_t& cfg_entry, int page_offset)
{
	int curr_offset = page_offset;
	sg_syslink_t *curr = reinterpret_cast<sg_syslink_t *>(base_addr + curr_offset);

	for (int i = 0; i < cfg_entry.pages; i += cfg_entry.per_pages) {
		for (int j = 0; j < cfg_entry.per_entries; j++) {
			curr->blink = curr_offset - cfg_entry.entry_size;
			curr->flink = curr_offset + cfg_entry.entry_size;
			curr = reinterpret_cast<sg_syslink_t *>(reinterpret_cast<char *>(curr) + cfg_entry.entry_size);
			curr_offset += cfg_entry.entry_size;
		}
	}

	curr = reinterpret_cast<sg_syslink_t *>(base_addr + page_offset);
	curr->blink = -1;
}

void sg_config::alloc_pages(sg_cfg_entry_t& cfg_entry)
{
	int size = (cfg_map->pages + cfg_entry.per_pages) * CFG_PAGE_SIZE;
	if (truncate(sgconfig.c_str(), size) == -1)
		throw sg_exception(__FILE__, __LINE__, SGEOS, 0, (_("ERROR: Can't truncate SGPROFILE file {1}") % sgconfig).str(SGLOCALE));
	region_mgr.reset(new bi::mapped_region(*mapping_mgr, bi::read_write));

	base_addr = reinterpret_cast<char *>(region_mgr->get_address());
	cfg_map = reinterpret_cast<sg_cfg_map_t *>(base_addr);

	sg_cfg_entry_t map_info;
	get_page_info(sizeof(sg_cfg_map_t), 1, map_info);
	bbp = reinterpret_cast<sg_bbparms_t *>(base_addr + map_info.pages * CFG_PAGE_SIZE);

	int page_offset = cfg_map->pages * CFG_PAGE_SIZE;
	cfg_map->pages += cfg_entry.per_pages;
	cfg_entry.entry_free = page_offset;
	init_pages(cfg_entry, page_offset);
}

sg_config::mch_iterator sg_config::find_internal(const sg_mchent_t& mchent)
{
	mch_iterator iter;
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
	int target = SGC->midpidx(mchent.mid);
	int cur;
	for (iter = mch_begin(), cur = 0; iter != mch_end() && cur < target; ++iter, cur++)
		continue;

	if (iter != mch_end() && (iter->flags & MA_INVALID))
		return mch_end();

	return iter;
}

sg_config::net_iterator sg_config::find_internal(const sg_netent_t& netent)
{
	net_iterator iter;
	for (iter = net_begin(); iter != net_end(); ++iter) {
		if (strcmp(iter->lmid, netent.lmid) == 0
			&& strcmp(iter->netgroup, netent.netgroup) == 0)
			break;
	}

	return iter;
}

sg_config::sgt_iterator sg_config::find_internal(const sg_sgent_t& sgent)
{
	sgt_iterator iter;
	for (iter = sgt_begin(); iter != sgt_end(); ++iter) {
		if (sgent.sgname[0] != '\0') {
			if (strcmp(iter->sgname, sgent.sgname) == 0)
				break;
		} else if (iter->grpid == sgent.grpid) {
			break;
		}
	}

	return iter;
}

sg_config::svr_iterator sg_config::find_internal(const sg_svrent_t& svrent)
{
	svr_iterator iter;
	for (iter = svr_begin(); iter != svr_end(); ++iter) {
		if (iter->sparms.svrproc.grpid == svrent.sparms.svrproc.grpid
			&& iter->sparms.svrproc.svrid <= svrent.sparms.svrproc.svrid
			&& iter->sparms.svridmax >= svrent.sparms.svrproc.svrid)
			break;
	}

	return iter;
}

sg_config::svc_iterator sg_config::find_internal(const sg_svcent_t& svcent)
{
	svc_iterator iter;
	for (iter = svc_begin(); iter != svc_end(); ++iter) {
		if (iter->grpid == svcent.grpid
			&& strcmp(iter->svc_pattern, svcent.svc_pattern) == 0)
			break;
	}

	return iter;
}

sg_config::svc_iterator sg_config::find_internal(const sg_svcparms_t& svcparms, int grpid)
{
	svc_iterator iter;
	for (iter = svc_begin(); iter != svc_end(); ++iter) {
		if (iter->grpid != DFLT_PGID && iter->grpid != grpid)
			continue;

		regex_t reg;
		if (regcomp(&reg, iter->svc_pattern, REG_NOSUB | REG_EXTENDED))
			continue;

		BOOST_SCOPE_EXIT((&reg)) {
			regfree(&reg);
		} BOOST_SCOPE_EXIT_END;

		if (regexec(&reg, svcparms.svc_name, (size_t)0, 0, 0) != 0)
			continue;

		break;
	}

	return iter;
}

void sg_config::init_once()
{
	_instance = new sg_config();
}

}
}

