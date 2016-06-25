#include "build_meta.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;
using namespace ai::sg;
using namespace std;

int main(int argc, char **argv)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	dbcp_ctx *DBCP = dbcp_ctx::instance();

	GPP->set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("input,i", po::value<string>(&DBCP->_DBCP_dbcconfig)->required(), (_("specify XML configuration file, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;

	po::positional_options_description pdesc;
	pdesc.add("input", 1);

	try {
		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).positional(pdesc).run(), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		exit(1);
	}

	// 打开配置文件
	dbc_config& config_mgr = dbc_config::instance();
	if (!config_mgr.open())
		return -1;

	try {
		build_meta meta_mgr;

		meta_mgr.gen_switch();
		meta_mgr.gen_structs();
		meta_mgr.gen_searchs();
		meta_mgr.gen_search_headers();
		meta_mgr.gen_search_defs();
		meta_mgr.gen_exp();
		return 0;
	} catch (exception& ex) {
		cout << ex.what();
		return -1;
	}
}

