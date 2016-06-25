#include "sdc_server.h"
#include "sdc_svc.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bpt = boost::property_tree;

namespace ai
{
namespace sg
{

sdc_server::sdc_server()
{
	DBCP = dbcp_ctx::instance();
	DBCT = dbct_ctx::instance();
}

sdc_server::~sdc_server()
{
}

int sdc_server::svrinit(int argc, char **argv)
{
	string dbc_key;
	int dbc_version;
	string sdc_key;
	int sdc_version;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("dbckey,d", po::value<string>(&dbc_key)->required(), (_("specify DBC's configuration key")).str(SGLOCALE).c_str())
		("dbcversion,y", po::value<int>(&dbc_version)->default_value(-1), (_("specify DBC's configuration version")).str(SGLOCALE).c_str())
		("sdckey,s", po::value<string>(&sdc_key)->required(), (_("specify SDC's configuration key")).str(SGLOCALE).c_str())
		("sdcversion,z", po::value<int>(&sdc_version)->default_value(-1), (_("specify SDC's configuration version")).str(SGLOCALE).c_str())
		("table,t", po::value<vector<string> >(&table_names), (_("specify table names to advertise operation")).str(SGLOCALE).c_str())
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

	std::sort(table_names.begin(), table_names.end());

	if (!DBCP->get_config(dbc_key, dbc_version, sdc_key, sdc_version))
		return retval;

	dbc_api& api_mgr = dbc_api::instance(DBCT);
	if (!api_mgr.login())
		return retval;

	if (!api_mgr.set_autocommit(true))
		return retval;

	// 发布SDC特有的服务
	if (!advertise())
		return retval;

	sgc_ctx *SGC = SGT->SGC();
	GPP->write_log((_("INFO: {1} -g {2} -p {3} started successfully.") % GPP->_GPP_procname % SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid).str(SGLOCALE));
	retval = 0;
	return retval;
}

int sdc_server::svrfini()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	dbc_api& api_mgr = dbc_api::instance(DBCT);
	api_mgr.disconnect();
	api_mgr.logout();

	if (!DBCP->_DBCP_sdcconfig.empty())
		unlink(DBCP->_DBCP_sdcconfig.c_str());

	if (!DBCP->_DBCP_dbcconfig.empty())
		unlink(DBCP->_DBCP_dbcconfig.c_str());

	sgc_ctx *SGC = SGT->SGC();
	GPP->write_log((_("INFO: {1} -g {2} -p {3} stopped successfully.") % GPP->_GPP_procname % SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid).str(SGLOCALE));
	retval = 0;
	return retval;
}

bool sdc_server::advertise()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	string svc_name;
	sg_api& api_mgr = sg_api::instance(SGT);
	sdc_config& config_mgr = sdc_config::instance(DBCT);
	set<sdc_config_t>& config_set = config_mgr.get_config_set();
	sgc_ctx *SGC = SGT->SGC();

	bool has_svc = false;
	bool has_sqlsvc = false;

	for (sg_svcdsp_t *svcdsp = SGP->_SGP_svcdsp; svcdsp->index != -1; ++svcdsp) {
		string class_name = svcdsp->class_name;

		if (strcmp(svcdsp->svc_name, ".SDC_SVC") == 0) {
			has_svc = true;
			BOOST_FOREACH(const sdc_config_t& item, config_set) {
				if (item.mid != SGC->_SGC_proc.mid)
					continue;

				// 不是指定的表，不需发布服务
				if (!table_names.empty() && !std::binary_search(table_names.begin(), table_names.end(), item.table_name))
					continue;

				svc_name = (boost::format("SDC_%1%_%2%") % item.table_id % item.partition_id).str();
				if (!api_mgr.advertise(svc_name, class_name)) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to advertise operation {1}") % svc_name).str(SGLOCALE));
					return retval;
				}
			}
		} else if (strcmp(svcdsp->svc_name, ".SDC_SQLSVC") == 0) {
			has_sqlsvc = true;
			svc_name = (boost::format("SDC_SQLSVC_%1%") % SGC->_SGC_proc.mid).str();
			if (!api_mgr.advertise(svc_name, class_name)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to advertise operation {1}") % svc_name).str(SGLOCALE));
				return retval;
			}
		}
	}

	if (!has_svc) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No .SDC_SVC operation provided on sgcompile time")).str(SGLOCALE));
		return retval;
	}

	if (!has_sqlsvc) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No .SDC_SQLSVC operation provided on sgcompile time")).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

}
}

