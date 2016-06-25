#include "metarep_svr.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

metarep_svr::metarep_svr()
{
	MRP = mrp_ctx::instance();
}

metarep_svr::~metarep_svr()
{
}

int metarep_svr::svrinit(int argc, char **argv)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("conf,c", po::value<string>(&MRP->_MRP_config_file)->required(), (_("metadata repository configuration file")).str(SGLOCALE).c_str())
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

	if (!load()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: {1} start failure, see ULOG for more detail.") % GPP->_GPP_procname).str(SGLOCALE));
		return retval;
	}

	sgc_ctx *SGC = SGT->SGC();
	GPP->write_log((_("INFO: {1} -g {2} -p {3} started successfully.") % GPP->_GPP_procname % SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid).str(SGLOCALE));
	retval = 0;
	return retval;
}

int metarep_svr::svrfini()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	sgc_ctx *SGC = SGT->SGC();
	GPP->write_log((_("INFO: {1} -g {2} -p {3} stopped successfully.") % GPP->_GPP_procname % SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid).str(SGLOCALE));
	retval = 0;
	return retval;
}

bool metarep_svr::load()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif
	gpenv& env_mgr = gpenv::instance();

	try {
		bpt::iptree pt;
		bpt::read_xml(MRP->_MRP_config_file, pt);

		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("repositories")) {
			if (v.first != "repository")
				continue;

			const bpt::iptree& v_pt = v.second;

			string key = v_pt.get<string>("key");
			string config;
			env_mgr.expand_var(config, v_pt.get<string>("config"));

			if (MRP->_MRP_key_map.find(key) != MRP->_MRP_key_map.end()) {
				SGT->_SGT_error_code = SGEDUPENT;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Duplicate key {1} found") % key).str(SGLOCALE));
				return retval;
			}

			MRP->_MRP_key_map[key] = config;
		}

		if (MRP->_MRP_key_map.empty()) {
			SGT->_SGT_error_code = SGENOENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: No entry loaded.").str(SGLOCALE)));
			return retval;
		}

		if (MRP->_MRP_key_map.size() > 1)
			GPP->write_log((_("INFO: {1} entries loaded successfully.") % MRP->_MRP_key_map.size()).str(SGLOCALE));
		else
			GPP->write_log((_("INFO: {1} entry loaded successfully.") % MRP->_MRP_key_map.size()).str(SGLOCALE));

		retval = true;
		return retval;
	} catch (bpt::ptree_error& ex) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Parse section repositories failure, {1}") % ex.what()).str(SGLOCALE));
		return retval;
	}
}

}
}

