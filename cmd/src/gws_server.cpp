#include "gws_server.h"
#include "gws_rpc.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

gws_server& gws_server::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<gws_server *>(SGT->_SGT_mgr_array[SVR_MANAGER]);
}

gws_server::gws_server()
	: GWP(gwp_ctx::instance()),
	  timer(GWP->_GWP_io_svc)
{
	GWP = gwp_ctx::instance();
}

gws_server::~gws_server()
{
}

int gws_server::svrinit(int argc, char **argv)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("net-threads,n", po::value<int>(&GWP->_GWP_net_threads)->default_value(1), (_("set threads handling net requests")).str(SGLOCALE).c_str())
		("queue-threads,q", po::value<int>(&GWP->_GWP_queue_threads)->default_value(1), (_("set threads handling message queue requests")).str(SGLOCALE).c_str())
		("cached-msgs,c", po::value<int>(&SGP->_SGP_max_cached_msgs)->default_value(1000), (_("set cached messages not free immediately")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			GPP->write_log((_("INFO: {1} exit normally in help mode.") % GPP->_GPP_procname).str(SGLOCALE));
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

	if (SGP->_SGP_ambsgws) {
		GWP->_GWP_net_threads = 0;
		GWP->_GWP_queue_threads = 1;
	} else {
		if (GWP->_GWP_net_threads <= 0)
			GWP->_GWP_net_threads = 1;
		if (GWP->_GWP_queue_threads <= 0)
			GWP->_GWP_queue_threads = 1;
	}

	// 关闭框架提供的线程机制，使用自定义线程机制
	SGP->_SGP_threads = 0;
	GPP->_GPP_do_threads = true;

	// 创建监听端口
	sgc_ctx *SGC = SGT->SGC();
	int local_mid = SGC->getmid();
	if (local_mid == BADMID) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to get local host id")).str(SGLOCALE));
		return retval;
	}

	sg_proc& proc_mgr = sg_proc::instance(SGT);
	if (!SGP->_SGP_ambsgws) {
		// 获取BSGWS节点信息
		sg_wkinfo_t wkinfo;
		if (proc_mgr.get_wkinfo(SGC->midkey(local_mid), wkinfo)) {
			// 设置gws_rpc对象
			if (SGT->_SGT_mgr_array[GRPC_MANAGER] != NULL) {
				delete SGT->_SGT_mgr_array[GRPC_MANAGER];
				SGT->_SGT_mgr_array[GRPC_MANAGER] = NULL;
			}
			SGT->_SGT_mgr_array[GRPC_MANAGER] = new gws_rpc();

			gws_rpc& grpc_mgr = gws_rpc::instance(SGT);
			message_pointer msg = grpc_mgr.create_msg(OB_EXIT, 0);

			// Initialize header fields specific to our purposes.
			sg_msghdr_t& msghdr = *msg;
			msghdr.rcvr = SGC->_SGC_proc;
			msghdr.rplymtype = NULLMTYPE;

			sg_metahdr_t& metahdr = *msg;
			metahdr.mid = msghdr.rcvr.mid;
			metahdr.qaddr = wkinfo.qaddr;

			// Send the message to the sghgws
			sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
			if (!ipc_mgr.send(msg, SGSIGRSTRT | SGBYPASS)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't send shutdown message to the sghgws")).str(SGLOCALE));
				return retval;
			}

			// Give sghgws chance to die
			int i;
			for (i = 0; i < 100; i++) {
				if (kill(wkinfo.pid, 0) == -1)
					break;

				boost::this_thread::sleep(bp::milliseconds(10));
			}

			if (i == 100) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to stop the sghgws")).str(SGLOCALE));
				return retval;
			}
		}
	}

	// 修改GWS队列地址
	if (!proc_mgr.create_wkid(false)) {
		SGT->_SGT_error_code = SGEOS;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create wkid for sghgws/sggws")).str(SGLOCALE));
		return retval;
	}

	// 调整版本
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	rpc_mgr.smlaninc();

	// 当前进程为GWS
	SGP->_SGP_amgws = true;
	GPP->_GPP_do_threads = true;

	// 调整circuits列表，预分配内存，在状态变更时都不会重新分配
	sg_config& config_mgr = sg_config::instance(SGT);
	int local_maxconn;

	if (SGP->_SGP_ambsgws) {
		local_maxconn = 1;
	} else {
		sg_mchent_t mchent;
		mchent.mid = local_mid;
		if (!config_mgr.find(mchent)) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find node entry in SGPROFILE")).str(SGLOCALE));
			return retval;
		}
		local_maxconn = mchent.maxconn;
	}

	// 获取本地网络
	set<gws_netent_t> local_nets;
	if (!get_address(local_mid, local_nets))
		return retval;

	int pidx = SGC->midpidx(local_mid);
	for (sg_config::mch_iterator iter = config_mgr.mch_begin(); iter != config_mgr.mch_end(); ++iter) {
		if (iter->flags & MA_INVALID)
			continue;

		int maxconn;
		if (SGC->midpidx(iter->mid) == pidx)
			maxconn = 0;
		else
			maxconn = std::min(local_maxconn, iter->maxconn);

		// 获取远程网络
		set<gws_netent_t> remote_nets;
		if (!get_address(iter->mid, remote_nets))
			return retval;

		vector<gws_netent_t> nets;
		BOOST_FOREACH(const gws_netent_t& item, remote_nets) {
			if (local_nets.find(item) != local_nets.end())
				nets.push_back(item);
		}

		GWP->_GWP_circuits.push_back(gws_circuit_t(maxconn, nets));
	}

	BOOST_FOREACH(const gws_netent_t& item, local_nets) {
		boost::shared_ptr<basio::ip::tcp::acceptor> acceptor(new basio::ip::tcp::acceptor(GWP->_GWP_io_svc));
		acceptors.push_back(acceptor);

		basio::ip::tcp::resolver resolver(GWP->_GWP_io_svc);
		basio::ip::tcp::resolver::query query(item.ipaddr, item.port);
		boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
		acceptor->open(endpoint.protocol());
		acceptor->set_option(basio::socket_base::reuse_address(true));
		acceptor->bind(endpoint);
		acceptor->listen();
		start_accept(*acceptor, item.tcpkeepalive, item.nodelay);
	}

	start_connect(local_mid);

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

	GWP->_GWP_thread_group.resize(GWP->_GWP_net_threads + GWP->_GWP_queue_threads);
	// 启动网络处理流程
	for (int i = 0; i < GWP->_GWP_net_threads; i++)
		pthread_create(&GWP->_GWP_thread_group[i], &attr, &gws_server::net_run, NULL);

	// 启动消息队列处理流程
	GWP->_GWP_queue_svcs.resize(GWP->_GWP_queue_threads);
	for (int i = 0; i < GWP->_GWP_queue_threads; i++)
		pthread_create(&GWP->_GWP_thread_group[GWP->_GWP_net_threads + i], &attr, &gws_queue::static_run, &GWP->_GWP_queue_svcs[i]);

	GPP->write_log((_("INFO: {1} started successfully.") % GPP->_GPP_procname).str(SGLOCALE));
	retval = 0;
	return retval;
}

int gws_server::svrfini()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	GPP->write_log((_("INFO: {1} stopped successfully.") % GPP->_GPP_procname).str(SGLOCALE));
	retval = 0;
	return retval;
}

int gws_server::run(int flags)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	// 重置线程相关对象
	SGT = sgt_ctx::instance();
	// 设置sg_svr对象指针
	if (SGT->_SGT_mgr_array[SVR_MANAGER] != NULL && SGT->_SGT_mgr_array[SVR_MANAGER] != this) {
		GPP->write_log(__FILE__, __LINE__, (_("FATAL: Internal error, sg_svr can only be called once per thread")).str(SGLOCALE));
		return retval;
	}
	SGT->_SGT_mgr_array[SVR_MANAGER] = this;

	// 设置gws_rpc对象
	if (SGT->_SGT_mgr_array[GRPC_MANAGER] != NULL) {
		delete SGT->_SGT_mgr_array[GRPC_MANAGER];
		SGT->_SGT_mgr_array[GRPC_MANAGER] = NULL;
	}
	SGT->_SGT_mgr_array[GRPC_MANAGER] = new gws_rpc();

	sgc_ctx *SGC = SGT->SGC();
	sg_rpc& rpc_mgr = sg_rpc::instance(SGT);
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	int local_mid = SGC->getmid();
	if (local_mid == BADMID) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to get local host id")).str(SGLOCALE));
		return retval;
	}

	if (SGP->_SGP_boot_flags & BF_SUSPAVAIL) {
		if (!rpc_mgr.set_stat(SGP->_SGP_ssgid, ~SUSPENDED, 0))
			GPP->write_log(__FILE__, __LINE__, (_("WARN: Unable to resume process {1}") % SGP->_SGP_ssgid).str(SGLOCALE));
	}

	if (!SGP->_SGP_ambsgws) {
		// 网络监听多线程方式
		int clean_time = 0;
		while (1) {
			{
				bi::scoped_lock<boost::mutex> lock(GWP->_GWP_exit_mutex);

				// 收到了shutdown消息
				if (SGP->_SGP_shutdown)
					break;

				bp::ptime expire = bp::from_time_t(bbp->bbmeters.timestamp + 1 + bbp->bbparms.scan_unit);
				GWP->_GWP_exit_cond.timed_wait(GWP->_GWP_exit_mutex, expire);

				// 收到了shutdown消息
				if (SGP->_SGP_shutdown)
					break;
			}

			// 检查网络连接
			start_connect(local_mid);

			// 检查BBM是否正常，必要时重启BBM
			if (++clean_time == bbp->bbparms.sanity_scan) {
				clean_time = 0;

				sg_ste& ste_mgr = sg_ste::instance(SGT);
				sg_proc& proc_mgr = sg_proc::instance(SGT);

				sg_ste_t bbmste;
				bbmste.svrid() = MNGR_PRCID;
				bbmste.grpid() = SGC->_SGC_proc.grpid;
				if (ste_mgr.retrieve(S_GRPID, &bbmste, &bbmste, NULL) != 1) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: sggws recovery: cannot find the sgmngr entry in the Bulletin Board")).str(SGLOCALE));
					continue;
				}

				sg_qte& qte_mgr = sg_qte::instance(SGT);
				sg_qte_t bbmqte;
				bbmqte.hashlink.rlink = bbmste.queue.qlink;
				if(qte_mgr.retrieve(S_BYRLINK, &bbmqte, &bbmqte, NULL) < 0){
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: sggws recovery cannot find queue entry for sgmngr in the Bulletin Board")).str(SGLOCALE));
					continue;
				}

				if (!proc_mgr.alive(bbmste.svrproc())) {
					if (bbmste.global.status & RESTARTING)
						continue;

					if (rpc_mgr.ustat(&bbmste.sgid(), 1, PSUSPEND, STATUS_OP_OR) < 0) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: sggws recovery cannot set sgmngr PSUSPEND")).str(SGLOCALE));
						continue;
					}

					/*
					 * Reset the sgmngr status so that if we're not the first
					 * sgrecover, the other one will pick the sgmngr for restarting.
					 */
					bbmste.global.status |= RESTARTING;
					bbmste.global.status &= ~IDLE;
					ste_mgr.update(&bbmste, 1, U_LOCAL);

					int spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
					sg_viable& viable_mgr = sg_viable::instance(SGT);
					if (viable_mgr.rstsvr(&bbmqte, &bbmste, spawn_flags) < 0) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: sggws recovery attempting to restart the sgmngr failed")).str(SGLOCALE));
						continue;
					}
				}
			}
		}
	} else {
		// 网络监听单线程方式
		timer.expires_from_now(bp::seconds(1));
		timer.async_wait(boost::bind(&gws_server::handle_timeout, this, basio::placeholders::error));

		while (!SGP->_SGP_shutdown) {
			GWP->_GWP_io_svc.run_one();
			if (GWP->_GWP_io_svc.stopped())
				break;

			if (timeout) {
				start_connect(local_mid);
				timeout = false;
			}
		}
	}

	cleanup();		// this will not detach for sggws
	GWP->_GWP_shutdown = true;

	BOOST_FOREACH(pthread_t& thread, GWP->_GWP_thread_group) {
		pthread_join(thread, NULL);
	}

	// must wait for all queue threads exit since it may use network staff
	GWP->_GWP_io_svc.stop();
	SGC->detach();
	SGP->_SGP_amserver = false;
	if (SGC->_SGC_crte != NULL)
		SGC->_SGC_crte->rflags = CLIENT;

	retval = 0;
	return retval;	// no more processing
}

void gws_server::start_accept(basio::ip::tcp::acceptor& acceptor, bool tcpkeepalive, bool nodelay)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("tcpkeepalive={1}, nodelay={2}") % tcpkeepalive % nodelay).str(SGLOCALE), NULL);
#endif

	gws_connection::pointer new_connection = gws_connection::create(GWP->_GWP_io_svc, -1);

	acceptor.async_accept(new_connection->socket(),
		boost::bind(boost::mem_fn(&gws_server::handle_accept), this, boost::ref(acceptor),
		new_connection, tcpkeepalive, nodelay,
		basio::placeholders::error));
}

bool gws_server::start_connect(int local_mid)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("local_mid={1}") % local_mid).str(SGLOCALE), &retval);
#endif
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	int local_pidx = SGC->midpidx(local_mid);
	for (int pidx = 0; pidx < local_pidx && !(SGC->_SGC_ptes[pidx].flags & NP_UNUSED); pidx++) {
		if ((SGC->_SGC_ptes[pidx].flags & NP_INVALID)
			|| !(SGC->_SGC_ptes[pidx].flags & NP_NETWORK))
			continue;

		gws_circuit_t& circuit = GWP->_GWP_circuits[pidx];
		bi::scoped_lock<boost::mutex> lock(circuit.mutex);

		if (circuit.all_ready())
			continue;

		for (int slot = 0; slot < circuit.lines.size(); slot++) {
			gws_line_t& line = circuit.lines[slot];

			switch (line.node_state) {
			case NODE_STATE_INIT:		// 初始状态
				break;
			case NODE_STATE_CONNECTING:	// 正在连接
			case NODE_STATE_CONNECTED:	// 已经连接
			case NODE_STATE_SUSPEND:	// 永久挂起
				continue;
			case NODE_STATE_TIMED:		// 超时连接
				if (line.recon_time > bbp->bbmeters.timestamp) {
					// 未到等待时间
					continue;
				}
				break;
			default:
				continue;
			}

			int mid = SGC->_SGC_ptes[pidx].mid;
			gws_netent_t& netent = line.netent;

			// 设置状态
			line.node_state = NODE_STATE_CONNECTING;

			gws_connection::pointer new_connection = gws_connection::create(GWP->_GWP_io_svc, slot);
			new_connection->connect(mid, netent.ipaddr, netent.port, netent.tcpkeepalive, netent.nodelay);
		}
	}

	retval = true;
	return retval;
}

void gws_server::handle_accept(basio::ip::tcp::acceptor& acceptor, gws_server::pointer new_connection, bool tcpkeepalive, bool nodelay, const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(10, __PRETTY_FUNCTION__, (_("tcpkeepalive={1}, nodelay={2}, error={3}") % tcpkeepalive % nodelay % error).str(SGLOCALE), NULL);
#endif

	if (!error)
		new_connection->start(tcpkeepalive, nodelay);

	start_accept(acceptor, tcpkeepalive, nodelay);
}

void gws_server::handle_timeout(const boost::system::error_code& error)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("error={1}") % error).str(SGLOCALE), NULL);
#endif

	if (!error)
		timeout = true;

	timer.expires_from_now(bp::seconds(bbp->bbparms.scan_unit));
	timer.async_wait(boost::bind(&gws_server::handle_timeout, this, basio::placeholders::error));
}

bool gws_server::get_address(int mid, set<gws_netent_t>& netents)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_config& config_mgr = sg_config::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("mid={1}") % mid).str(SGLOCALE), &retval);
#endif

	string lmid = SGC->_SGC_ptes[SGC->midpidx(mid)].lname;
	for (sg_config::net_iterator iter = config_mgr.net_begin(); iter != config_mgr.net_end(); ++iter) {
		if (lmid != iter->lmid)
			continue;

		gws_netent_t item;
		string naddr = iter->naddr;
		if (naddr.size() < 5) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid gwsaddr parameter given, at least 5 characters long")).str(SGLOCALE));
			return retval;
		}

		if (memcmp(naddr.c_str(), "//", 2) != 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid gwsaddr parameter given, should start with \"//\"")).str(SGLOCALE));
			return retval;
		}

		string::size_type pos = naddr.find(":");
		if (pos == string::npos) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid gwsaddr parameter given, missing ':'")).str(SGLOCALE));
			return retval;
		}

		string hostname = naddr.substr(2, pos - 2);
		item.port = naddr.substr(pos + 1, naddr.length() - pos - 1);

		if (!isdigit(hostname[0])) {
			hostent *ent = gethostbyname(hostname.c_str());
			if (ent == NULL) {
				SGT->_SGT_error_code = SGESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid hostname of gwsaddr parameter given")).str(SGLOCALE));
				return retval;
			}
			item.ipaddr = boost::lexical_cast<string>(ent->h_addr_list[0][0] & 0xff);
			for (int i = 1; i < ent->h_length; i++) {
				item.ipaddr += '.';
				item.ipaddr += boost::lexical_cast<string>(ent->h_addr_list[0][i] & 0xff);
			}
		} else {
			item.ipaddr = hostname;
		}

		item.netgroup = iter->netgroup;
		item.tcpkeepalive = iter->tcpkeepalive;
		item.nodelay = iter->nodelay;

		netents.insert(item);
	}

	if (netents.empty()) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No network available for remote node {1}")
			% SGC->mid2lmid(mid)).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

void * gws_server::net_run(void *arg)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("arg={1}") % arg).str(SGLOCALE), NULL);
#endif
	sgt_ctx *SGT = sgt_ctx::instance();
	gwp_ctx *GWP = gwp_ctx::instance();

	// 设置gws_rpc对象
	if (SGT->_SGT_mgr_array[GRPC_MANAGER] != NULL) {
		delete SGT->_SGT_mgr_array[GRPC_MANAGER];
		SGT->_SGT_mgr_array[GRPC_MANAGER] = NULL;
	}
	SGT->_SGT_mgr_array[GRPC_MANAGER] = new gws_rpc();

	GWP->_GWP_io_svc.run();
	return NULL;
}

}
}

