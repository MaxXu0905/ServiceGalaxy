#include "dbc_server.h"
#include "sg_internal.h"

namespace po = boost::program_options;
using namespace ai::sg;

int main(int argc, char **argv)
{
	bool background = false;
	dbcp_ctx *DBCP = dbcp_ctx::instance();

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("version,v", (_("print current dbc_server version")).str(SGLOCALE).c_str())
		("enable,e", (_("enable starting dbc_sync")).str(SGLOCALE).c_str())
		("reuse,r", (_("reuse original shared memory")).str(SGLOCALE).c_str())
		("background,b", (_("run dbc_server in background mode")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			::exit(0);
		} else if (vm.count("version")) {
			std::cout << (_("dbc_server version ")).str(SGLOCALE) << DBC_RELEASE << std::endl;
			std::cout << (_("Compiled on ")).str(SGLOCALE) << __DATE__ << " " << __TIME__ << std::endl;
			::exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		::exit(1);
	}

	if (vm.count("enable"))
		DBCP->_DBCP_enable_sync = true;
	else
		DBCP->_DBCP_enable_sync = false;

	if (vm.count("reuse"))
		DBCP->_DBCP_reuse = true;

	if (vm.count("background"))
		background = true;

	if (background) {
		sys_func& sys_mgr = sys_func::instance();
		sys_mgr.background();
	}

	// 设置可执行文件名称
	gpp_ctx *GPP = gpp_ctx::instance();
	GPP->set_procname(argv[0]);

	// 打开配置文件
	dbc_config& config_mgr = dbc_config::instance();
	if (!config_mgr.open()) {
		cerr << (_("ERROR: Can't open configuration file.\n")).str(SGLOCALE);
		return -1;
	}

	DBCP->_DBCP_is_server = true;
	DBCP->_DBCP_current_pid = getpid();

	try {
		dbc_server svr_mgr;
		svr_mgr.run();
		return 0;
	} catch (exception& ex) {
		cout << ex.what();
		return -1;
	}
}

