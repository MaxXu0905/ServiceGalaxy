#include "dbc_msvr.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

namespace ai
{
namespace sg
{

dbc_msvr::dbc_msvr()
{
	DBCP = dbcp_ctx::instance();
	DBCT = dbct_ctx::instance();
}

dbc_msvr::~dbc_msvr()
{
}

int dbc_msvr::svrinit(int argc, char **argv)
{
	string dbc_key;
	int dbc_version;
	string sdc_key;
	int sdc_version;
	vector<string> hosts;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	// 关闭框架提供的线程机制
	SGP->_SGP_threads = 0;

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("version,v", (_("print current DBC version")).str(SGLOCALE).c_str())
		("dbckey,d", po::value<string>(&dbc_key)->required(), (_("specify DBC's configuration key")).str(SGLOCALE).c_str())
		("dbcversion,y", po::value<int>(&dbc_version)->default_value(-1), (_("specify DBC's configuration version")).str(SGLOCALE).c_str())
		("sdckey,s", po::value<string>(&sdc_key)->required(), (_("specify SDC's configuration key")).str(SGLOCALE).c_str())
		("sdcversion,z", po::value<int>(&sdc_version)->default_value(-1), (_("specify SDC's configuration version")).str(SGLOCALE).c_str())
		("reuse,r", (_("reuse shared memory if exists")).str(SGLOCALE).c_str())
		("master,m", (_("run in master mode")).str(SGLOCALE).c_str())
		("immediate,i", (_("finish svrinit() without login on DBC")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			GPP->write_log((_("INFO: {1} exit normally in help mode.") % GPP->_GPP_procname).str(SGLOCALE));
			retval = 0;
			return retval;
		} else if (vm.count("version")) {
			std::cout << (_("DBC version ")).str(SGLOCALE) << DBC_RELEASE << std::endl;
			std::cout << (_("Compiled on ")).str(SGLOCALE) << __DATE__ << " " << __TIME__ << std::endl;
			GPP->write_log((_("INFO: {1} exit normally in version mode.") % GPP->_GPP_procname).str(SGLOCALE));
			retval = 0;
			return retval;
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		SGT->_SGT_error_code = SGEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: {1} start failure, {2}.") % GPP->_GPP_procname % ex.what()).str(SGLOCALE));
		return retval;
	}

	if (vm.count("reuse"))
		DBCP->_DBCP_reuse = true;

	if (vm.count("master"))
		DBCP->_DBCP_proc_type = DBC_MASTER;
	else
		DBCP->_DBCP_proc_type = DBC_SLAVE;

	if (vm.count("immediate"))
		immediate = true;
	else
		immediate = false;

	if (!DBCP->get_config(dbc_key, dbc_version, sdc_key, sdc_version))
		return retval;

	DBCP->_DBCP_is_server = true;
	DBCP->_DBCP_current_pid = getpid();

	// 获取Master节点列表
	if (!DBCP->get_mlist(SGT))
		return retval;

	if (!svr_mgr.login()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: login to domain failed, {1}") % DBCT->strerror()).str(SGLOCALE));
		return retval;
	}

	// 首先删除ready文件，这样Slave节点会等待Master启动完成
	if (DBCP->_DBCP_proc_type & DBC_MASTER) {
		sgc_ctx *SGC = SGT->SGC();
		const char *lmid = SGC->mid2lmid(SGC->_SGC_proc.mid);
		string ready_file = DBCT->get_ready_file(lmid);

		// 如果ready文件不存在，则映像文件可能不一致，需要删除
		if (access(ready_file.c_str(), R_OK) == -1) {
			dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
			dbc_bbparms_t& bbparms = bbp->bbparms;

			string cmd = "rm -f ";
			cmd += bbparms.data_dir;
			cmd += "*/*-*.b.";
			cmd += lmid;
			cmd += " >/dev/null 2>&1";

			sys_func& func_mgr = sys_func::instance();
			if (func_mgr.new_task(cmd.c_str(), NULL, 0) != 0) {
				DBCT->_DBCT_error_code = DBCESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't remove corrupted image - {1}") % strerror(errno)).str(SGLOCALE));
				return retval;
			}
		} else {
			::unlink(ready_file.c_str());
		}
	}

	if (!immediate && !svr_mgr.load()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: load data failed, {1}") % DBCT->strerror()).str(SGLOCALE));
		return retval;
	}

	sgc_ctx *SGC = SGT->SGC();
	GPP->write_log((_("INFO: {1} -g {2} -p {3} started successfully.") % GPP->_GPP_procname % SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid).str(SGLOCALE));
	retval = 0;
	return retval;
}

int dbc_msvr::svrfini()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	svr_mgr.logout();

	// 移除临时文件
	if (!DBCP->_DBCP_dbcconfig.empty())
		unlink(DBCP->_DBCP_dbcconfig.c_str());

	if (!DBCP->_DBCP_sdcconfig.empty())
		unlink(DBCP->_DBCP_sdcconfig.c_str());

	sgc_ctx *SGC = SGT->SGC();
	GPP->write_log((_("INFO: {1} -g {2} -p {3} stopped successfully.") % GPP->_GPP_procname % SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid).str(SGLOCALE));
	retval = 0;
	return retval;
}

int dbc_msvr::run(int flags)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	// 如果还没有登录，则现在登录
	if (immediate && !svr_mgr.load()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: load data failed, {1}") % DBCT->strerror()).str(SGLOCALE));
		return retval;
	}

	// 重置线程相关对象
	SGT = sgt_ctx::instance();
	// 设置sg_svr对象指针
	if (SGT->_SGT_mgr_array[SVR_MANAGER] != NULL && SGT->_SGT_mgr_array[SVR_MANAGER] != this) {
		GPP->write_log(__FILE__, __LINE__, (_("FATAL: Internal error, sg_svr can only be called once per thread")).str(SGLOCALE));
		return retval;
	}
	SGT->_SGT_mgr_array[SVR_MANAGER] = this;

	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_svcdsp& svcdsp_mgr = sg_svcdsp::instance(SGT);
	message_pointer rmsg = sg_message::create(SGT);
	bool drain;
	bool onetime;

	if (flags & SGONETIME) {
		flags &= ~SGDRAIN;
		onetime = true;
		drain = false;
	} else {
		if (flags & SGDRAIN) {
			flags |= SGNOBLOCKING;
			drain = true;
		} else {
			flags &= ~SGNOBLOCKING;
			drain = false;
		}
		onetime = false;
	}

	if (SGP->_SGP_boot_flags & BF_SUSPAVAIL) {
		if (!rpc_mgr.set_stat(SGP->_SGP_ssgid, ~SUSPENDED, 0))
			GPP->write_log(__FILE__, __LINE__, (_("WARN: UNABLE TO RESUME process {1}") % SGP->_SGP_ssgid).str(SGLOCALE));
	}

	// continuously receive messages
	try {
		sgc_ctx *SGC = SGT->SGC();
		sg_bboard_t *sgbbp = SGC->_SGC_bbp;
		sg_bbmeters_t& sgbbmeters = sgbbp->bbmeters;
		dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
		dbc_bbparms_t& bbparms = bbp->bbparms;
		dbc_bbmeters_t& bbmeters = bbp->bbmeters;
		int remain = bbparms.scan_unit;
		cleantime = 0;
		start_time = sgbbmeters.timestamp;

		while (1) {
			SGT->_SGT_fini_cmd = 0;

			if (SGP->_SGP_shutdown_due_to_sigterm) {
				drain = false;
				onetime = false;
				break;
			}

			if (!rcvrq(rmsg, flags, remain)) {
				if (SGP->_SGP_shutdown_due_to_sigterm || SGP->_SGP_shutdown) {
					drain = false;
					onetime = false;
					break;
				}

				if (onetime || drain)
					break;

				if (SGT->_SGT_error_code == SGETIME) {
					scoped_usignal sig;
					bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

					if (++cleantime == bbparms.sanity_scan) { // 检查是否有死掉的注册节点
						svr_mgr.rte_clean();
						svr_mgr.te_clean();
						cleantime = 0;
					}

					for (dbc_te_t *te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
						if (!te->in_persist())
							continue;

						svr_mgr.refresh_table(te);
					}

					// 检查是否有超时的事务
					svr_mgr.chk_tran_timeout();

					remain = bbparms.scan_unit;
					start_time = sgbbmeters.timestamp;
				}

				continue;
			}

			svcdsp_mgr.svcdsp(rmsg, 0);

			if (!finrq())
				break;

			if (SGT->_SGT_fini_cmd & SGBREAK)
				break;

			if (onetime)
				break;

			time_t currtime = sgbbmeters.timestamp;
			if (currtime - start_time >= bbp->bbparms.scan_unit) {
				scoped_usignal sig;
				bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

				cleantime += (currtime - start_time) / bbp->bbparms.scan_unit;
				if (cleantime == bbparms.sanity_scan) { // 检查是否有死掉的注册节点
					svr_mgr.rte_clean();
					svr_mgr.te_clean();
					cleantime = 0;
				}

				for (dbc_te_t *te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
					if (!te->in_persist())
						continue;

					svr_mgr.refresh_table(te);
				}

				// 检查是否有超时的事务
				svr_mgr.chk_tran_timeout();

				remain = bbparms.scan_unit;
				start_time = sgbbmeters.timestamp;
				remain = bbp->bbparms.scan_unit;
			} else {
				remain = bbp->bbparms.scan_unit - (currtime - start_time);
			}
		}
	} catch (exception& ex) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Fatal error receiving requests, shutting process down")).str(SGLOCALE));
		drain = false;
		onetime = false;
	}

	cleanup();		// this will detach

	if (onetime || drain || (SGT->_SGT_fini_cmd & SGBREAK)) {
		retval = 0;
		return retval;
	}

	return retval;	// no more processing
}

}
}

