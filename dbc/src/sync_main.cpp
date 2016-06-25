#include "dbc_sync.h"
#include "sg_internal.h"

namespace po = boost::program_options;
namespace bf = boost::filesystem;
using namespace ai::sg;

int main(int argc, char **argv)
{
	bool background = false;

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("version,v", (_("print current dbc_sync version")).str(SGLOCALE).c_str())
		("background,b", (_("run dbc_sync in background mode")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			::exit(0);
		} else if (vm.count("version")) {
			std::cout << (_("dbc_sync version ")).str(SGLOCALE) << DBC_RELEASE << std::endl;
			std::cout << (_("Compiled on ")).str(SGLOCALE) << __DATE__ << " " << __TIME__ << std::endl;
			::exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		::exit(1);
	}

	if (vm.count("background"))
		background = true;

	if (background) {
		sys_func& sys_mgr = sys_func::instance();
		sys_mgr.background();
	}

	// 设置可执行文件名称
	gpp_ctx *GPP = gpp_ctx::instance();
	GPP->set_procname(argv[0]);

	dbcp_ctx *DBCP = dbcp_ctx::instance();
	DBCP->_DBCP_is_sync = true;
	DBCP->_DBCP_current_pid = getpid();

	try {
		dbc_sync sync_mgr;
		sync_mgr.run();
		return 0;
	} catch (exception& ex) {
		cout << ex.what();
		return -1;
	}
}

