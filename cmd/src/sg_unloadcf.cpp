#include "sg_unloadcf.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

sg_unloadcf::sg_unloadcf()
{
	GPP = gpp_ctx::instance();
	SGP = sgp_ctx::instance();
	prompt = true;
	suppress = false;
}

sg_unloadcf::~sg_unloadcf()
{
}

void sg_unloadcf::init(int argc, char **argv)
{
	GPP->set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("override,y", (_("override existing XML configuration file?")).str(SGLOCALE).c_str())
		("suppress,s", (_("suppress prompting information")).str(SGLOCALE).c_str())
		("output,o", po::value<string>(&xml_file)->required(), (_("specify XML configuration file, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
		("hidden,H", (_("enable hidden options")).str(SGLOCALE).c_str())
	;

	po::positional_options_description pdesc;
	pdesc.add("output", 1);

	po::variables_map vm;

	try {
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

	if (vm.count("override")) {
		prompt = false;
	} else if (vm.count("suppress")) {
		prompt = false;
		suppress = true;
	}

	if (vm.count("hidden"))
		enable_hidden = true;
	else
		enable_hidden = false;

	char *ptr = gpenv::instance().getenv("SGPROFILE");
	if (ptr == NULL) {
		if (!suppress)
			std::cout << (_("ERROR: SGPROFILE environment is not set\n")).str(SGLOCALE);
		exit(1);
	}
	config_file = ptr;

	if (access(xml_file.c_str(), R_OK | W_OK) == -1)
		prompt = false;

	if (prompt) {
		string cmd;
		std::cout << (_("Override existing XML configuration file? (y/n): ")).str(SGLOCALE) << std::flush;
		while (1) {
			std::cin >> cmd;
			if (cmd == "y" || cmd == "Y") {
				break;
			} else if (cmd == "n" || cmd == "N") {
				exit(1);
			} else {
				std::cout << (_("answer y or n: ")).str(SGLOCALE) << std::flush;
			}
		}
	}
}

void sg_unloadcf::run()
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sg_xmlcfg xmlcfg_mgr(xml_file);
	sg_config& config_mgr = sg_config::instance(SGT);

	xmlcfg_mgr.bbparms = config_mgr.get_bbparms();

	for (sg_config::mch_iterator iter = config_mgr.mch_begin(); iter != config_mgr.mch_end(); ++iter)
		xmlcfg_mgr.mchents.push_back(*iter);

	for (sg_config::net_iterator iter = config_mgr.net_begin(); iter != config_mgr.net_end(); ++iter)
		xmlcfg_mgr.netents.push_back(*iter);

	for (sg_config::sgt_iterator iter = config_mgr.sgt_begin(); iter != config_mgr.sgt_end(); ++iter)
		xmlcfg_mgr.sgents.push_back(*iter);

	for (sg_config::svr_iterator iter = config_mgr.svr_begin(); iter != config_mgr.svr_end(); ++iter)
		xmlcfg_mgr.svrents.push_back(*iter);

	for (sg_config::svc_iterator iter = config_mgr.svc_begin(); iter != config_mgr.svc_end(); ++iter)
		xmlcfg_mgr.svcents.push_back(*iter);

	xmlcfg_mgr.save(enable_hidden);

	if (!suppress)
		std::cout << (_("INFO: XML configuration file {1} has been created\n") % xml_file).str(SGLOCALE);
}

}
}

using namespace ai::sg;

int main(int argc, char **argv)
{
	try {
		sg_unloadcf mgr;

		mgr.init(argc, argv);
		mgr.run();
		return 0;
	} catch (boost::interprocess::interprocess_exception& ex) {
		cout << ex.get_error_code() << std::endl;
		exit(1);
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		exit(1);
	}
}

