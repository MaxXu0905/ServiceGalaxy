#include "admin_server.h"

namespace po = boost::program_options;
using namespace ai::sg;

int main(int argc, char **argv)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	GPP->set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("version,v", (_("print current bbm version")).str(SGLOCALE).c_str())
		("conf,c", (_("run in conf mode")).str(SGLOCALE).c_str())
		("read,r", (_("run in read mode")).str(SGLOCALE).c_str())
		("wiz,w", (_("run in wizard mode")).str(SGLOCALE).c_str())
		("hidden,H", (_("enable hidden options")).str(SGLOCALE).c_str())
		("sm,s", (_("run in SM mode")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			exit(0);
		} else if (vm.count("version")) {
			std::cout << (_("sgmngr version ")).str(SGLOCALE) << SGRELEASE << std::endl;
			std::cout << (_("Compiled on ")).str(SGLOCALE) << __DATE__ << " " << __TIME__ << std::endl;
			exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		exit(1);
	}

	bool enable_hidden = false;
	if (vm.count("hidden"))
		enable_hidden = true;

	int& global_flags = cmd_parser::get_global_flags();
	if (vm.count("conf"))
		global_flags |= CONF;
	if (vm.count("read"))
		global_flags |= READ;
	if (vm.count("wiz"))
		global_flags |= WIZ;
	if (enable_hidden && vm.count("sm"))
		global_flags |= SM;

	if (!(global_flags & CONF)) {
		gpenv& env_mgr = gpenv::instance();
		char *ptr = env_mgr.getenv("SGPROFILE");

		if (ptr == NULL) {
			cerr << (_("ERROR: SGPROFILE environment variable not set.\n")).str(SGLOCALE);
			exit(1);
		}

		if (ptr[0] != '/') {
			cerr << (_("ERROR: SGPROFILE must be an absolute pathname.\n")).str(SGLOCALE);
			exit(1);
		}
	}

	try {
		admin_server admin_mgr;
		return admin_mgr.run();
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}
}

