#include "file_svr.h"
#include "file_svc.h"
#include "file_struct.h"

namespace po = boost::program_options;

namespace ai
{
namespace sg
{

int const FILE_GET = 0x1;
int const FILE_PUT = 0x2;
int const FILE_ADMIN = 0x4;

file_svr::file_svr()
{
	SSHP = sshp_ctx::instance();
}

file_svr::~file_svr()
{
}

int file_svr::svrinit(int argc, char **argv)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	vector<string> types;

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("type,t",  po::value<vector<string> >(&types), (_("specify which type is allowed: GET, PUT, or ADMIN").str(SGLOCALE)).c_str())
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

	int flags = 0;
	BOOST_FOREACH(const string& type, types) {
		if (strcasecmp(type.c_str(), "GET") == 0) {
			flags |= FILE_GET;
		} else if (strcasecmp(type.c_str(), "PUT") == 0) {
			flags |= FILE_PUT;
		} else if (strcasecmp(type.c_str(), "ADMIN") == 0) {
			flags |= FILE_ADMIN;
		} else {
			std::cout << desc << std::endl;
			SGT->_SGT_error_code = SGEINVAL;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid type {1} given.") % type).str(SGLOCALE));
			return retval;
		}
	}

	if (flags == 0)
		flags = FILE_GET | FILE_PUT | FILE_ADMIN;

	// 发布服务
	if (!advertise(flags))
		return retval;

	sgc_ctx *SGC = SGT->SGC();
	GPP->write_log((_("INFO: {1} -g {2} -p {3} started successfully.") % GPP->_GPP_procname % SGC->_SGC_proc.grpid % SGC->_SGC_proc.svrid).str(SGLOCALE));
	retval = 0;
	return retval;
}

int file_svr::svrfini()
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

bool file_svr::advertise(int flags)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	sg_svcdsp_t *svcdsp = SGP->_SGP_svcdsp;
	if (svcdsp == NULL || svcdsp->index == -1) {
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: No operation provided on sgcompile time")).str(SGLOCALE) );
		return retval;
	}

	string class_name = svcdsp->class_name;
	sg_api& api_mgr = sg_api::instance(SGT);

	for (int i = 0; i < 3; i++) {
		string svc_name;
		switch (i) {
		case 0:
			if (!(flags & FILE_GET))
				continue;
			svc_name = FMNGR_GSVC;
			break;
		case 1:
			if (!(flags & FILE_PUT))
				continue;
			svc_name = FMNGR_PSVC;
			break;
		case 2:
			if (!(flags & FILE_ADMIN))
				continue;
			svc_name = FMNGR_OSVC;
			break;
		default:
			continue;
		}

		if (!api_mgr.advertise(svc_name, class_name)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to advertise operation {1}") % svc_name).str(SGLOCALE));
			return retval;
		}
	}

	retval = true;
	return retval;
}

}
}

