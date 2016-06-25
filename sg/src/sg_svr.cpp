#include "sg_internal.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

bool sg_svr::sync_gotsig = false;

sg_svr& sg_svr::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_svr *>(SGT->_SGT_mgr_array[SVR_MANAGER]);
}

sg_svr::sg_svr()
{
}

sg_svr::~sg_svr()
{
}

int sg_svr::_main(int argc, char **argv, INSTANCE_FUNC create_instance)
{
	sgc_ctx *SGC = SGT->SGC();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	GPP->set_procname(argv[0]);

	if (SGC->_SGC_amclient || SGP->_SGP_amserver || SGC->_SGC_svrstate & (SGIN_INIT | SGIN_SVC | SGIN_DONE))
		return retval;

	signal(SIGTERM, sg_svr::stdsigterm);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, sg_svr::stdsigterm);

	sg_svr *_this = this;
	BOOST_SCOPE_EXIT((&_this)) {
		_this->cleanup();
	} BOOST_SCOPE_EXIT_END

	int options;
	if ((options = stdinit(argc, argv)) < 0) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGESYSTEM;

		if (SGT->_SGT_error_code != SGEDUPENT || !(SGP->_SGP_boot_flags & BF_SPAWN))
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: System init function failed - {1}, {2}") % SGT->strerror() % SGT->strnative()).str(SGLOCALE));

		return retval;
	}

	SGT->_SGT_error_code = 0;
	SGC->_SGC_svrstate = SGIN_INIT;

	try {
		int _argc = 0;
		boost::shared_array<char *> _argv(new char *[argc]);
		_argv[_argc++] = argv[0];

		bool found_sep = false;
		for (int i = 0; i < argc; i++) {
			if (found_sep)
				_argv[_argc++] = argv[i];
			else if (strcmp(argv[i], "--") == 0)
				found_sep = true;
		}

		if (svrinit(_argc, _argv.get()) != 0) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: System init function failed - {1}, {2}") % SGT->strerror() % SGT->strnative()).str(SGLOCALE));
			return retval;
		}
	} catch (exception& ex) {
		SGC->_SGC_svrstate = 0;
		SGP->_SGP_svrinit_called = false;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: svrinit() failed, {1}") % ex.what()).str(SGLOCALE));

		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGEAPPINIT;

		return retval;
	}

	SGC->_SGC_svrstate = 0;
	SGP->_SGP_svrinit_called = true;

	if (SGC->_SGC_crte->hndl.numghandle > 0) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: svrinit() failed with outstanding message handles")).str(SGLOCALE));
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGESYSTEM;
		return retval;
	}

	// 通知sgboot已经成功初始化
	synch(0);

	SGP->_SGP_stdmain_booted = true;
	SGP->_SGP_amserver = true;

	try {
		if (SGP->_SGP_threads <= 1) {
			run();
		} else {
			GPP->_GPP_do_threads = true;

			sg_bboard_t *bbp = SGC->_SGC_bbp;
			sg_bbparms_t& bbparms = bbp->bbparms;

			pthread_attr_t attr;
			pthread_attr_init(&attr);
			BOOST_SCOPE_EXIT((&attr)) {
				pthread_attr_destroy(&attr);
			} BOOST_SCOPE_EXIT_END

			if (bbparms.stack_size > 0)
				pthread_attr_setstacksize(&attr, bbparms.stack_size);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

			std::vector<pthread_t> thread_group;
			thread_group.resize(SGP->_SGP_threads);

			vector<boost::shared_ptr<sg_svr> > svrs;
			for (int i = 0; i < SGP->_SGP_threads; i++) {
				boost::shared_ptr<sg_svr> svr_instance(create_instance());
				svrs.push_back(svr_instance);

				pthread_create(&thread_group[i], &attr, &sg_svr::static_run, svr_instance.get());
			}

			BOOST_FOREACH(pthread_t& thread, thread_group) {
				pthread_join(thread, NULL);
			}
		}
	} catch (exception& ex) {
		GPP->write_log(__FILE__, __LINE__, ex.what());
		return retval;
	}

	retval = options;
	return options;
}

void * sg_svr::static_run(void *arg)
{
	sg_svr *_instance = reinterpret_cast<sg_svr *>(arg);

	_instance->run(0);
	return NULL;
}

int sg_svr::stdinit(int argc, char **argv)
{
	signal(SIGHUP, SIG_IGN);

	sgc_ctx *SGC = SGT->SGC();
	string ulogpfx;
	int nice_value;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("version,v", (_("print ServiceGalaxy version")).str(SGLOCALE).c_str())
		("pgid,g", po::value<int>(&SGP->_SGP_grpid)->required(), (_("set process group id")).str(SGLOCALE).c_str())
		("prcid,p", po::value<int>(&SGP->_SGP_svrid)->required(), (_("set process id")).str(SGLOCALE).c_str())
		("hostid,h", po::value<int>(&SGC->_SGC_getmid_mid), (_("set host id")).str(SGLOCALE).c_str())
		("all,A", (_("advertise all operations")).str(SGLOCALE).c_str())
		("spawn,D", (_("spawn process")).str(SGLOCALE).c_str())
		("migrate,M", (_("migrate process")).str(SGLOCALE).c_str())
		("restart,R", po::value<pid_t>(&SGP->_SGP_oldpid), (_("origial process pid to restart")).str(SGLOCALE).c_str())
		("susp-avail,P", (_("boot process in suspended mode, and make it available after svrinit")).str(SGLOCALE).c_str())
		("uname,u", po::value<string>(&GPP->_GPP_uname), (_("set the fake uname")).str(SGLOCALE).c_str())
		("logname,L", po::value<string>(&ulogpfx), (_("user log name")).str(SGLOCALE).c_str())
		("sync,f", po::value<int>(&SGP->_SGP_boot_sync)->default_value(0), (_("fd to synchronize with sgboot/sgshut")).str(SGLOCALE).c_str())
		("stdout,o", po::value<string>(&SGP->_SGP_stdmain_ofile), (_("redirect stdout to given file")).str(SGLOCALE).c_str())
		("stderr,e", po::value<string>(&SGP->_SGP_stdmain_efile), (_("redirect stderr to given file")).str(SGLOCALE).c_str())
		("nice,n", po::value<int>(&nice_value)->default_value(-1), (_("set nice value")).str(SGLOCALE).c_str())
		("operation,s", po::value<vector<string> >(&SGP->_SGP_adv_svcs), (_("advertised operation list")).str(SGLOCALE).c_str())
		("threads,T", po::value<int>(&SGP->_SGP_threads)->default_value(1), (_("threads to handle queue message")).str(SGLOCALE).c_str())
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
			std::cout << (_("ServiceGalaxy version ")).str(SGLOCALE) << SGRELEASE << std::endl;
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

	SGP->_SGP_adv_all = false;

	if (vm.count("all"))
		SGP->_SGP_adv_all = true;
	if (vm.count("spawn"))
		SGP->_SGP_boot_flags |= BF_SPAWN;
	if (vm.count("migrate"))
		SGP->_SGP_boot_flags |= BF_MIGRATE;
	if (vm.count("susp-avail"))
		SGP->_SGP_boot_flags |= BF_SUSPAVAIL;

	// 重启进程时不需要重新发布服务
	if (SGP->_SGP_oldpid > 0) {
		SGP->_SGP_boot_flags |= BF_RESTART;
		SGP->_SGP_adv_all = false;
		SGP->_SGP_adv_svcs.clear();
	}

	// 设置ulog
	if (!ulogpfx.empty())
		GPP->set_ulogpfx(ulogpfx);

	if (nice_value != -1 && nice(nice_value) == -1)
		GPP->write_log(__FILE__, __LINE__, (_("WARN: nice() failed")).str(SGLOCALE));

	// 设置stdout/stderr
	if (!SGP->_SGP_stdmain_ofile.empty()) {
		FILE *fp;
		if ((fp = freopen(SGP->_SGP_stdmain_ofile.c_str(), "a", stdout)) == NULL)
			GPP->write_log((_("WARN: Cannot open {1} as stdout: Uunixerr = {2}") % SGP->_SGP_stdmain_ofile % strerror(errno)).str(SGLOCALE));
		setvbuf(stderr, NULL, _IOLBF, BUFSIZ);
	}

	if (!SGP->_SGP_stdmain_efile.empty()) {
		FILE *fp;
		if ((fp = freopen(SGP->_SGP_stdmain_efile.c_str(), "a", stderr)) == NULL)
			GPP->write_log((_("WARN: Cannot open {1} as stdout: Uunixerr = {2}") % SGP->_SGP_stdmain_ofile % strerror(errno)).str(SGLOCALE));
		setvbuf(stderr, NULL, _IOLBF, BUFSIZ);
	}

	vector<sg_svcparms_t> svcparms;
	for (sg_svcdsp_t *svcdsp = SGP->_SGP_svcdsp; svcdsp->index != -1; svcdsp++) {
		const string& svc_name = svcdsp->svc_name;

		if (!SGP->_SGP_adv_all
			&& std::find(SGP->_SGP_adv_svcs.begin(), SGP->_SGP_adv_svcs.end(), svc_name) == SGP->_SGP_adv_svcs.end())
			continue;

		sg_svcparms_t item;
		strncpy(item.svc_name, svcdsp->svc_name, sizeof(item.svc_name) - 1);
		item.svc_name[sizeof(item.svc_name) - 1] = '\0';
		strncpy(item.svc_cname, svcdsp->class_name, sizeof(item.svc_cname) - 1);
		item.svc_cname[sizeof(item.svc_cname) - 1] = '\0';
		item.svc_index = svcdsp->index;
		svcparms.push_back(item);
	}

	if (enroll(argv[0], svcparms) == BADSGID)
		return retval;

	if (SGC->_SGC_crte != NULL)
		SGC->_SGC_crte->qaddr = SGC->_SGC_proc.rqaddr;

	// 设置主线程的sg_svr
	SGT->_SGT_mgr_array[SVR_MANAGER] = this;

	retval = 0;
	return retval;
}

int sg_svr::run(int flags)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	// 重置线程相关对象
	sgt_ctx *SGT = sgt_ctx::instance();

	// 设置sg_svr对象指针
	if (SGT->_SGT_mgr_array[SVR_MANAGER] != NULL && SGT->_SGT_mgr_array[SVR_MANAGER] != this) {
		GPP->write_log(__FILE__, __LINE__, (_("FATAL: Internal error, sg_svr can only be called once per thread")).str(SGLOCALE));
		return -1;
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
			GPP->write_log(__FILE__, __LINE__, (_("WARN: Unable to resume process {1}") % SGP->_SGP_ssgid).str(SGLOCALE));
	}

	// continuously receive messages
	try {
		while (1) {
			SGT->_SGT_fini_cmd = 0;

			if (SGP->_SGP_shutdown_due_to_sigterm) {
				drain = false;
				onetime = false;
				break;
			}

			if (!rcvrq(rmsg, flags)) {
				if (SGP->_SGP_shutdown_due_to_sigterm || SGP->_SGP_shutdown) {
					drain = false;
					onetime = false;
					break;
				}

				if (onetime || drain)
					break;

				continue;
			}

			svcdsp_mgr.svcdsp(rmsg, 0);

			if (!finrq())
				break;

			if (SGT->_SGT_fini_cmd & SGBREAK)
				break;

			if (onetime)
				break;
		}
	} catch (exception& ex) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Fatal error receiving requests, shutting process down")).str(SGLOCALE));
		drain = false;
		onetime = false;
	}

	if (onetime || drain || (SGT->_SGT_fini_cmd & SGBREAK)) {
		retval = 0;
		return retval;
	}

	return retval;	// no more processing
}

int sg_svr::svrinit(int argc, char **argv)
{
	int retval = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	sgc_ctx *SGC = SGT->SGC();
	GPP->write_log((_("INFO: {1} -g {2} -p {3} started successfully.") % GPP->_GPP_procname % SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid).str(SGLOCALE));
	return retval;
}

int sg_svr::svrfini()
{
	int retval = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	sgc_ctx *SGC = SGT->SGC();
	GPP->write_log((_("INFO: {1} -g {2} -p {3} stopped successfully.") % GPP->_GPP_procname % SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid).str(SGLOCALE));
	return retval;
}

sgid_t sg_svr::enroll(const char *aout, vector<sg_svcparms_t>& svcparms)
{
	sgid_t retval = BADSGID;
#if defined(DEBUG)
	scoped_debug<sgid_t> debug(950, __PRETTY_FUNCTION__, (_("aout={1}") % aout).str(SGLOCALE), &retval);
#endif

	// 已经注册，直接返回
	if (SGP->_SGP_amserver) {
		retval = SGP->_SGP_ste.sgid();
		return retval;
	}

	// 检查服务的名称，
	if (!SGP->_SGP_let_svc) {
		BOOST_FOREACH(const sg_svcparms_t& v, svcparms) {
			if (v.admin_name()) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot advertise operation names beginning with '.'")).str(SGLOCALE));
				return retval;
			}
		}
	}

	sgc_ctx *SGC = SGT->SGC();
	sg_config& config_mgr = sg_config::instance(SGT);
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_mchent_t mchent;

	if (SGP->_SGP_svrid == MNGR_PRCID || SGP->_SGP_svrid == GWS_PRCID) {
		if (!SGC->get_bbparms())
			return retval;
	}

	// 设置标识
	string name;
	if (SGP->_SGP_svrid != MNGR_PRCID)
		name = "SERVER";
	else
		name = (SGP->_SGP_grpid == CLST_PGID) ? "SGCLST" : "SGMNGR";

	// attach BB
	if (!SGC->hookup(name))
		return retval;

	sg_ste& ste_mgr = sg_ste::instance(SGT);

	// 获取SERVER参数
	sg_svrent_t svrent;
	SGP->setids(svrent.sparms.svrproc);
	if (!config_mgr.find(svrent)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find process entry for pgid {1}, prcid {2}") % svrent.sparms.svrproc.grpid % svrent.sparms.svrproc.svrid).str(SGLOCALE));
		return retval;
	}

	memcpy(&SGP->_SGP_svrent, &svrent, sizeof(svrent));
	SGP->_SGP_svrent.sparms.svrproc.svrid = SGP->_SGP_svridmin;

	// BBM在attach()之后会设置标识
	if (SGP->_SGP_amserver)
		goto SET_LABEL;

	mchent.mid = SGC->_SGC_proc.mid;
	if (!config_mgr.find(mchent))
		goto SKIP_ENVFILE;

	if (svrent.sparms.svrproc.admin_grp())
		goto SKIP_GRP_ENVFILE;

	// 如果有分组下的环境变量文件
	{
		sg_sgent_t sgent;
		sgent.grpid = svrent.sparms.svrproc.grpid;
		sgent.sgname[0] = '\0';
		if (!config_mgr.find(sgent)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Error getting Group Environment filename")).str(SGLOCALE));
			goto SKIP_GRP_ENVFILE;
		}

		if (sgent.envfile[0] != '\0' && !SGP->putenv(sgent.envfile, mchent))
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Error putting PROFILE {1} into environment") % sgent.envfile).str(SGLOCALE));
	}

SKIP_GRP_ENVFILE:
	if (svrent.bparms.envfile[0] != '\0'
		&& !SGP->putenv(svrent.bparms.envfile, mchent))
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Error putting PROFILE {1} into environment") % svrent.bparms.envfile).str(SGLOCALE));

SKIP_ENVFILE:
	// 根据环境变量，设置对应的值
	GPP->setenv();
	SGP->setenv();

	SGP->_SGP_boot_flags |= svrent.bparms.flags;
	if (!SGC->get_here(svrent))
		return retval;

	// 获取服务参数
	BOOST_FOREACH(sg_svcparms_t& v, svcparms) {
		if (!config_mgr.find(v, svrent.sparms.svrproc.grpid)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot get operation parms for {1}") % v.svc_name).str(SGLOCALE));
			retval = failure();
			return retval;
		}
	}

	strcpy(svrent.sparms.rqparms.filename, GPP->_GPP_procname.c_str());
	svrent.sparms.rqparms.paddr = SGC->_SGC_proc.rqaddr;
	svrent.sparms.svrproc = SGC->_SGC_proc;
	svrent.sparms.ctime = time(0);

	SGP->_SGP_ssgid = ste_mgr.offer(svrent.sparms, &(*svcparms.begin()), svcparms.size());
	if (SGP->_SGP_ssgid == BADSGID) {
		bool dup;

		if (SGT->_SGT_error_code == SGEDUPENT) {
			if (!(SGP->_SGP_boot_flags & BF_SPAWN))
				GPP->write_log(__FILE__, __LINE__, (_("WARN: Duplicate process")).str(SGLOCALE));
			dup = true;
		} else {
			dup = false;
		}

		retval = failure();
		if (dup)
			SGT->_SGT_error_code = SGEDUPENT;
		return retval;
	}

SET_LABEL:
	if (ste_mgr.retrieve(S_BYSGIDS, &SGP->_SGP_ste, &SGP->_SGP_ste, &SGP->_SGP_ssgid) < 0) {
		retval = failure();
		return retval;
	}

	if (SGC->_SGC_crte != NULL) {
		SGC->_SGC_crte->rt_grpid = SGP->_SGP_ste.grpid();
		SGC->_SGC_crte->rt_svrid = SGP->_SGP_ste.svrid();
	}

	/* 检查创建STE时是否同时创建了消息队列，
	 * 如果是，则删除系统默认创建的消息队列
	 * 如果我们是restarting server，或者MSSQ set，则使用创建STE时的消息队列
	 */
	if (SGP->_SGP_ste.rqaddr() != SGC->_SGC_proc.rqaddr) {
		proc_mgr.removeq(SGC->_SGC_proc.rqaddr);
		SGC->_SGC_proc.rqaddr = SGP->_SGP_ste.rqaddr();
	}

	if (SGP->_SGP_ste.rpaddr() != SGC->_SGC_proc.rpaddr) {
		proc_mgr.removeq(SGC->_SGC_proc.rpaddr);
		SGC->_SGC_proc.rpaddr = SGP->_SGP_ste.rpaddr();
	}

	sg_qte& qte_mgr = sg_qte::instance(SGT);
	SGP->_SGP_qte.hashlink.rlink = SGP->_SGP_ste.queue.qlink;
	if (qte_mgr.retrieve(S_BYRLINK, &SGP->_SGP_qte, &SGP->_SGP_qte, NULL) < 0) {
		retval = failure();
		return retval;
	}

	SGP->_SGP_qsgid = SGP->_SGP_qte.hashlink.sgid;
	SGP->_SGP_amserver = true;
	SGC->_SGC_crte->rflags = SERVER;

	// 重新设置SPAWNING位
	sg_qte_t *qte = qte_mgr.link2ptr(SGP->_SGP_qte.hashlink.rlink);
	if (qte->global.status & SPAWNING) {
		scoped_bblock bblock(SGT);
		qte->global.status &= ~SPAWNING;
	}

	if (!SGC->_SGC_proc.admin_grp()) { // 普通分组
		bool found = false;
		sg_sgte& sgte_mgr = sg_sgte::instance(SGT);
		sg_sgte_t sgte;
		{
			sg_sgte_t *sgtep;
			sharable_bblock bblock(SGT);
			for (int bucket = 0; bucket <= SGC->SGTBKTS(); bucket++) {
				for (int idx = SGC->_SGC_sgte_hash[bucket]; idx != -1; idx = sgtep->hashlink.fhlink) {
					sgtep = sgte_mgr.link2ptr(idx);
					if (sgtep->parms.grpid == SGC->_SGC_proc.grpid) {
						sgte = *sgtep;
						SGC->_SGC_sgidx = idx;
						found = true;
						break;
					}
				}
			}
		}

		if (!found) {
			GPP->write_log(__FILE__, __LINE__, (_("WARN: Cannot retrieve own process group table entry")).str(SGLOCALE));
			retval = SGP->_SGP_ssgid;
			return retval;
		}

		if (sgte.parms.midlist[sgte.parms.curmid] != SGC->_SGC_proc.mid) {
			/*
			 * Before updating the server group location, make
			 * absolutely sure that there are no servers in this
			 * group running on the currently indicated mid.  That
			 * would result in servers from the same group running
			 * on separate sites and that's a no-no.
			 */
			scoped_bblock bblock(SGT);

			sg_ste_t ste;
			int cnt;
			int i;
			ste.grpid() = sgte.parms.grpid;
			if ((cnt = ste_mgr.retrieve(S_GROUP, &ste, NULL, NULL)) > 0) {
				boost::shared_array<sgid_t> auto_sgids(new sgid_t[cnt + 1]);
				sgid_t *sgids = auto_sgids.get();

				cnt = ste_mgr.retrieve(S_GROUP, &ste, NULL, sgids);
				for (i = 0; i < cnt; i++) {
					if (ste_mgr.retrieve(S_BYSGIDS, &ste, &ste, &sgids[i]) == 1
						&& ste.mid() != SGC->_SGC_proc.mid) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Process group active on primary location")).str(SGLOCALE));
						retval = failure();
						return retval;
					}
				}
			}

			/*
			 * Update the location of this server's group as
			 * listed in the SGPROFILE file.
			 */
			for (i = 0; i < MAXLMIDSINLIST; i++) {
				if (sgte.parms.midlist[i] == SGC->_SGC_proc.mid) {
					sgte.parms.curmid = i;
					break;
				}
			}

			if (i == MAXLMIDSINLIST) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: current host id not in pgroup's node list")).str(SGLOCALE));
				retval = failure();
				return retval;
			}

			GPP->write_log((_("INFO: Boot process migrating location of group {1} to node {2}")
				% sgte.parms.sgname % SGC->mid2lmid(sgte.parms.midlist[sgte.parms.curmid])).str(SGLOCALE));

			/*
			 * Code deleted here to send O_UPDGRPS to the sgclst.
			 * The responsibility for updating the SGPROFILE has
			 * been moved to the O_USGRPS opcode so that all sites
			 * will have an updated SGPROFILE without additional
			 * messaging overhead.
			 */
			// Now update the group location as listed in the BB.
			if (sgte_mgr.update(&sgte, 1, 0) == -1) {
				GPP->write_log((_("WARN: Cannot update process group location as listed in bulletin board")).str(SGLOCALE));
				goto RET_LABEL;
			}
		}
	}

RET_LABEL:
	retval = SGP->_SGP_ssgid;
	return retval;
}

bool sg_svr::synch(int status)
{
	sig_action action(SIGPIPE, sg_svr::sigpipe);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(950, __PRETTY_FUNCTION__, (_("status={1}") % status).str(SGLOCALE), &retval);
#endif

	// make sure fd SGP->_SGP_boot_sync is a pipe
	if (SGP->_SGP_boot_sync >= 0 && !isatty(SGP->_SGP_boot_sync)
		&& lseek(SGP->_SGP_boot_sync, 0, 1) == -1) {
		write(SGP->_SGP_boot_sync, &status, sizeof(status));
		close(SGP->_SGP_boot_sync);
		if (SGP->_SGP_boot_sync == 0)
			freopen("/dev/null", "r", stdin);
	}
	SGP->_SGP_boot_sync = -1;

	if (!sync_gotsig)
		retval = true;

	return retval;
}

void sg_svr::cleanup()
{
	sgc_ctx *SGC = SGT->SGC();
#if defined(DEBUG)
	scoped_debug<bool> debug(940, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (SGC->_SGC_amclient || (SGC->_SGC_svrstate & (SGIN_INIT | SGIN_SVC | SGIN_DONE)))
		return;

	bool restarting = (!SGP->_SGP_stdmain_booted && (SGP->_SGP_boot_flags & BF_RESTART));
	if (!SGP->_SGP_stdmain_booted)
		SGP->_SGP_shutdown++;	// force sgmngr's to cleanup

	if (!restarting && SGP->_SGP_amserver) {
		if (stop_serv() < 0)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to stop serving")).str(SGLOCALE));
	}

	// 如果还没有启动完成，需要通知sgboot
	if (!SGP->_SGP_stdmain_booted) {
		if (SGT->_SGT_error_code == 0)
			SGT->_SGT_error_code = SGESYSTEM;

		synch(SGT->_SGT_error_code);
	}

	if (!restarting) {
		if (SGC->_SGC_proc.is_bbm()) {
			// 对BBM，无论有没有调用svrinit()，都需要调用svrfini()
			SGP->_SGP_svrinit_called = false;
			SGC->_SGC_svrstate = SGIN_DONE;
			svrfini();
			SGC->_SGC_svrstate = 0;
		}
	}
}

int sg_svr::stop_serv()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(940, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (!SGP->_SGP_amserver) {
		SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	sig_action action(SIGTERM, SIG_IGN);
	sgc_ctx *SGC = SGT->SGC();

	// 调用svrfini()，BBM需要特殊处理
	if (SGP->_SGP_svrinit_called && !SGP->_SGP_ambbm) {
		SGP->_SGP_svrinit_called = false;
		SGC->_SGC_svrstate = SGIN_DONE;
		svrfini();
		SGC->_SGC_svrstate = 0;
	}

	// 临时关闭_SGP_shutdown，以消除 os_receive()对_SGP_shutdown标识的特殊处理
	int saved_shutdown = SGP->_SGP_shutdown;
	SGP->_SGP_shutdown = 0;

	sg_ste& ste_mgr = sg_ste::instance(SGT);
	if ((retval = ste_mgr.remove(SGP->_SGP_ssgid)) < 0) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot remove server entry - {1}") % SGT->strerror()).str(SGLOCALE));
		retval = -1;
	}

	SGP->_SGP_shutdown += saved_shutdown;

	if (retval >= 0) {
		// 删除消息队列，只有在shutdown时，需要删除request queue，
		// 没有这个标识时，当前进程是MSSQ
		if (SGP->_SGP_shutdown)
			SGC->qcleanup(true);
		else
			SGC->qcleanup(false);
	}

	if (!SGP->_SGP_amgws) {
		SGC->detach();
		SGP->_SGP_amserver = false;
		if (SGC->_SGC_crte != NULL)
			SGC->_SGC_crte->rflags = CLIENT;
	}

	return retval;
}

bool sg_svr::rcvrq(message_pointer& rmsg, int flags, int timeout)
{
	int prio;
	sgc_ctx *SGC = SGT->SGC();
	sg_handle& hdl_mgr = sg_handle::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(960, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	if (!(SGP->_SGP_amserver | SGC->_SGC_amadm)) {
		SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	// Prevent msg starvation if single-server/single-queue.
	if ((flags & SGGETANY) || (SGC->_SGC_sssq && (--SGC->_SGC_rcvrq_countdown == 0))) {
		prio = NULLMTYPE;
		SGC->_SGC_rcvrq_countdown = STARVPROTECT;
	} else {
		// get highest prio msg on queue
		prio = -SGP->_SGP_ste.global.maxmtype;
	}

	if (SGC->_SGC_rcvrq_cpending) {
		if (!chkclient(rmsg))
			return retval;
	}

	if (!SGC->_SGC_sssq && SGC->_SGC_amadm) {
		/*
		 * A little assistence for cleanupsrv(1):
		 * don't attempt to drain the queue if there
		 * are other servers servicing it.
		 */
		SGT->_SGT_error_code = SGENOENT;
		return retval;
	}

	while (1) {
		SGT->_SGT_error_code = 0;

		// check reply queue for stale messages on reply queue
		if (SGC->_SGC_proc.rqaddr != SGC->_SGC_proc.rpaddr)
			hdl_mgr.rcvcstales();

		sg_msghdr_t *msghdr = *rmsg;
		msghdr->rcvr = SGC->_SGC_proc;
		msghdr->rplymtype = SGP->_SGP_shutdown ? NULLMTYPE : prio; // 0 = FIFO
		if (msghdr->rplymtype == NULLMTYPE)
			flags |= SGGETANY;

		if (!ipc_mgr.receive(rmsg, flags, timeout))
			return retval;

		sg_metahdr_t *metahdr = *rmsg;
		msghdr = *rmsg;
		SGT->_SGT_msghdr = *msghdr;

		if (!SGP->_SGP_ambbm && !SGP->_SGP_amgws) {
			/*
			 * sgmngr's must be able to receive replies while
			 * getting requests so don't do this check.
			 */
			if (rmsg->is_rply()) {
				// got a stale message on request queue
				hdl_mgr.drphandle(*rmsg);
				continue;
			}

			/*
			 * Ignore stale reply messages from a sgmngr/sgclst
			 */
			if (metahdr->mtype == sg_metahdr_t::bbrplymtype(SGC->_SGC_proc.pid))
				continue;
		}

		// Ignore stale network kill reply messages
		if (metahdr->mtype == (sg_metahdr_t::bbrplymtype(SGC->_SGC_proc.pid) | SG_NWKILLBIT))
			continue;

		// GWS需要对路由的META消息透传
		if (rmsg->is_meta() && (!SGP->_SGP_amgws || (metahdr->flags & METAHDR_FLAGS_GW))) {
			sg_meta& meta_mgr = sg_meta::instance(SGT);
			if (!meta_mgr.metarcv(rmsg))
				return retval;

			if (!(flags & SGGETANY) && !SGC->_SGC_sssq) {
				/*
				 * prio may have changed if we dequeued a
				 * meta-message not intended for us.
				 */
				prio = -SGP->_SGP_ste.global.maxmtype;
			}
			continue;
		}

		SGT->_SGT_lastprio = metahdr->mtype;
		break;
	}

	retval = true;
	return retval;
}

bool sg_svr::finrq()
{
	bool retval = true;
#if defined(DEBUG)
	scoped_debug<bool> debug(960, __PRETTY_FUNCTION__, "", &retval);
#endif
	return retval;
}

sgid_t sg_svr::failure()
{
	sgc_ctx *SGC = SGT->SGC();
	int oerrno = errno;

	if (!SGC->_SGC_amclient && !SGC->_SGC_amadm) {
		/* Only remove the queue for non-BRIDGE processes, BRIDGEs
		 * have well known queues and remove them under their own
		 * control.
		 */
		if (!SGC->_SGC_proc.is_gws())
			SGC->qcleanup(true);
		else
			SGC->qcleanup(false);
		SGC->detach();
	}

	errno = oerrno;
	return BADSGID;
}

// check for incomplete service requests
bool sg_svr::chkclient(message_pointer& rmsg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_ste_t *s;
	sg_ste& ste_mgr = sg_ste::instance(SGT);
	sg_svcdsp& svcdsp_mgr = sg_svcdsp::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(930, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (ste_mgr.retrieve(S_BYRLINK, &SGP->_SGP_ste, &SGP->_SGP_ste, NULL) < 0) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot find own process entry")).str(SGLOCALE));
		return retval;
	}

	// reset service timedout flag for restarted single-threaded server.
	s = ste_mgr.link2ptr(SGP->_SGP_ste.hashlink.rlink);

	if (SGP->_SGP_ste.local.currclient.pid) {
		sg_msghdr_t& msghdr = *rmsg;

		msghdr.flags = SGP->_SGP_ste.local.curroptions;
		msghdr.rplymtype = SGP->_SGP_ste.local.currrtype;
		msghdr.rplyiter = SGP->_SGP_ste.local.currriter;
		msghdr.rplyto = SGP->_SGP_ste.local.currclient;
		msghdr.alt_flds.msgid = SGP->_SGP_ste.local.currmsgid;
		strcpy(msghdr.svc_name, SGP->_SGP_ste.local.currservice);

		if (!rtntosndr(rmsg)) {
			/*
			 * Even if rtntosndr failed, we should reset the
			 * client pending flag to 0. If the message send
			 * failed once, it is very likely to fail again. We
			 * do not want the restarting server to be
			 * continuously trying to send this error message
			 * without ever processing service requests on its
			 * queue.
			 */
			SGC->_SGC_rcvrq_cpending = 0;
			s->global.status &= ~SVCTIMEDOUT;
			return retval;
		}

		svcdsp_mgr.svr_done();
	}

	s->global.status &= ~SVCTIMEDOUT;
	SGC->_SGC_rcvrq_cpending = 0;
	retval = true;
	return retval;
}

// return message to sender
bool sg_svr::rtntosndr(message_pointer& rmsg)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_msghdr_t& msghdr = *rmsg;
	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(930, __PRETTY_FUNCTION__, "", &retval);
#endif

	GPP->write_log((_("WARN: Process {1}/{2}: client process {3}: lost message")
		% SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid % msghdr.rplyto.pid).str(SGLOCALE));

	bool calive = proc_mgr.alive(msghdr.rplyto);

	if (calive) {
		GPP->write_log((_("WARN: OPERATION={1}    MSG_ID={2}    REASON=process died\n")
			% msghdr.svc_name % msghdr.alt_flds.msgid).str(SGLOCALE));
	} else { // client gone
		GPP->write_log((_("WARN: OPERATION={1}    MSG_ID={2}    REASON=process and client died\n")
			% msghdr.svc_name % msghdr.alt_flds.msgid).str(SGLOCALE));
	}

	/*
	 * If no reply was wanted or the client is
	 * now gone, log an error to the central
	 * log file and return.
	 */

	if (!rmsg->rplywanted() || !calive) {
		retval = true;
		return retval;
	}

	msghdr.flags |= SGNACK;
	msghdr.sender = SGC->_SGC_proc;
	msghdr.error_code = SGESVCERR;
	if (SGP->_SGP_ste.global.status & SVCTIMEDOUT)
		msghdr.error_detail = SGED_SVCTIMEOUT;

	msghdr.rcvr = msghdr.rplyto;
	rmsg->set_length(0);

	retval = ipc_mgr.send(rmsg, SGREPLY | SGSIGRSTRT);
	return retval;
}

void sg_svr::stdsigterm(int signo)
{
	sgp_ctx *SGP = sgp_ctx::instance();

	SGP->_SGP_shutdown++;
	SGP->_SGP_shutdown_due_to_sigterm = true;
}

void sg_svr::sigpipe(int signo)
{
	sync_gotsig = true;
}

}
}

