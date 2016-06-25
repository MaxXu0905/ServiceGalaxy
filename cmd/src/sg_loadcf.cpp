#include "sg_loadcf.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;

namespace ai
{
namespace sg
{

sg_loadcf::sg_loadcf()
{
	GPP = gpp_ctx::instance();
	SGP = sgp_ctx::instance();
	SGT = sgt_ctx::instance();

	prompt = true;
	suppress = false;
}

sg_loadcf::~sg_loadcf()
{
}

void sg_loadcf::init(int argc, char **argv)
{
	GPP->set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("override,y", (_("override existing SGPROFILE file?")).str(SGLOCALE).c_str())
		("suppress,s", (_("suppress prompting information")).str(SGLOCALE).c_str())
		("password,p", po::value<string>(&passwd)->default_value(""), (_("encrypted password to verify")).str(SGLOCALE).c_str())
		("input,i", po::value<string>(&xml_file)->required(), (_("specify XML configuration file, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
		("hidden,H", (_("enable hidden options")).str(SGLOCALE).c_str())
	;

	po::positional_options_description pdesc;
	pdesc.add("input", 1);

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
	sgconfig = ptr;

	if (access(sgconfig.c_str(), R_OK | W_OK) == -1)
		prompt = false;

	if (prompt) {
		string cmd;
		std::cout << (_("Override existing SGPROFILE file? (y/n): ")).str(SGLOCALE) << std::flush;
		while (1) {
			std::cin >> cmd;
			if (cmd == "y" || cmd == "Y") {
				break;
			} else if (cmd == "n" || cmd == "N") {
				exit(1);
			} else {
				cout << (_("answer y or n: ")).str(SGLOCALE) << std::flush;
			}
		}
	}
}

void sg_loadcf::run()
{
	sg_xmlcfg xmlcfg_mgr(xml_file);
	xmlcfg_mgr.load(enable_hidden);

	sg_bbparms_t& bbparms = xmlcfg_mgr.bbparms;
	if (bbparms.options & SGAA) {
		if (!SGP->do_auth(bbparms.perm, bbparms.ipckey, passwd, false))
			return;
	} else {
		passwd = "";
	}

	if (bbparms.maxsvrs * 2 + bbparms.maxaccsrs + MAXADMIN > QUEUE_MASK) {
		if (!suppress)
			std::cout << (_("ERROR: maxsvrs * 2 + maxaccsrs + {1} must be not greater than {2}")% MAXADMIN %QUEUE_MASK).str(SGLOCALE) << "\n";
		exit(1);
	}

	sg_config& config_mgr = sg_config::instance(SGT, false);
	if (!config_mgr.init(xmlcfg_mgr.bbparms.maxpes, xmlcfg_mgr.bbparms.maxnodes, xmlcfg_mgr.bbparms.maxsgt,
		xmlcfg_mgr.bbparms.maxsvrs, xmlcfg_mgr.bbparms.maxsvcs, xmlcfg_mgr.bbparms.perm)) {
		if (!suppress)
			std::cout << (_("ERROR: Failed to initialize SGPROFILE\n")).str(SGLOCALE);
		exit(1);
	}

	if (!config_mgr.open()) {
		if (!suppress)
			std::cout << (_("ERROR: Failed to open SGPROFILE\n")).str(SGLOCALE);
		exit(1);
	}

	sg_bbparms_t& cfg_bbparms = config_mgr.get_bbparms();
	cfg_bbparms = bbparms;
	memcpy(cfg_bbparms.passwd, passwd.c_str(), MAX_PASSWD);
	cfg_bbparms.passwd[MAX_PASSWD] = '\0';

	int i = 0;
	BOOST_FOREACH(sg_mchent_t& ent, xmlcfg_mgr.mchents) {
		ent.mid = i++;
		if (!config_mgr.insert(ent)) {
			if (!suppress)
				std::cout << (_("ERROR: Can't add node to SGPROFILE file ")).str(SGLOCALE) << sgconfig << "\n";
			exit(1);
		}
	}

	BOOST_FOREACH(const sg_netent_t& ent, xmlcfg_mgr.netents) {
		if (!config_mgr.insert(ent)) {
			if (!suppress)
				std::cout << (_("ERROR: Can't add node entry to SGPROFILE file ")).str(SGLOCALE) << sgconfig << "\n";
			exit(1);
		}
	}

	BOOST_FOREACH(const sg_sgent_t& ent, xmlcfg_mgr.sgents) {
		if (!config_mgr.insert(ent)) {
			if (!suppress)
				std::cout << (_("ERROR: Can't add process group to SGPROFILE file ")).str(SGLOCALE) << sgconfig << "\n";
			exit(1);
		}
	}

	BOOST_FOREACH(const sg_svrent_t& ent, xmlcfg_mgr.svrents) {
		if (!config_mgr.insert(ent)) {
			if (!suppress)
				std::cout << (_("ERROR: Can't add process entry to SGPROFILE file ")).str(SGLOCALE) << sgconfig << "\n";
			exit(1);
		}
	}

	BOOST_FOREACH(const sg_svcent_t& ent, xmlcfg_mgr.svcents) {
		if (!config_mgr.insert(ent)) {
			if (!suppress)
				std::cout << (_("ERROR: Can't add operation entry to SGPROFILE file ")).str(SGLOCALE) << sgconfig << "\n";
			exit(1);
		}
	}

	if (!config_mgr.flush()) {
		if (!suppress)
			std::cout << (_("ERROR: Can't flush SGPROFILE file ")).str(SGLOCALE) << sgconfig << "\n";
		exit(1);
	}

	if (!suppress)
		std::cout << (_("INFO: SGPROFILE file {1} has been created\n") % sgconfig).str(SGLOCALE);
}

}
}

using namespace ai::sg;

int main(int argc, char **argv)
{
	try {
		sg_loadcf mgr;

		mgr.init(argc, argv);
		mgr.run();
		return 0;
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		exit(1);
	}
}

