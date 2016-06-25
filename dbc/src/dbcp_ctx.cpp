#include "dbc_internal.h"

namespace bi = boost::interprocess;
namespace bf = boost::filesystem;
using namespace std;

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

namespace ai
{
namespace sg
{

switch_lib::switch_lib()
{
	handle = NULL;
	bbversion = -1;
}

switch_lib::~switch_lib()
{
	if (handle != NULL)
		::dlclose(handle);
}

dbc_switch_t * switch_lib::get_switch(int table_id)
{
	dbct_ctx *DBCT = dbct_ctx::instance();
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;

	if (bbp == NULL || bbversion != bbp->bbparms.bbversion) {
		if (!load_lib())
			return NULL;
	}

	return dbc_switch[table_id];
}

void switch_lib::set_switch(int table_id, dbc_switch_t *value)
{
	dbc_switch[table_id] = value;
}

const std::string& switch_lib::get_md5sum()
{
	dbct_ctx *DBCT = dbct_ctx::instance();
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;

	if (bbp == NULL || bbversion != bbp->bbparms.bbversion) {
		if (!load_lib())
			md5sum = "";
	}

	return md5sum;
}

bool switch_lib::load_lib()
{
	gpp_ctx *GPP = gpp_ctx::instance();
	dbct_ctx *DBCT = dbct_ctx::instance();
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;

	if (handle != NULL) {
		::dlclose(handle);
		handle = NULL;
		bbversion = -1;
	}

	const char *lib_name = ::getenv("DBC_CLIENT_LIB");
	if (lib_name == NULL)
		lib_name = "libdbcclient.so";

	handle = ::dlopen(lib_name, RTLD_LAZY);
	if (!handle) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = UOPEN;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open library {1}") % lib_name).str(SGLOCALE));
		return false;
	}

	get_switch_func fn = get_switch_func(::dlsym(handle, "get_switch"));
	if (!fn) {
		DBCT->_DBCT_error_code = DBCENOSWITCH;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function get_switch() doesn't exist in library {1}") % lib_name).str(SGLOCALE));
		return false;
	}

	dbc_switch = (*fn)();
	if (dbc_switch == NULL) {
		DBCT->_DBCT_error_code = DBCENOSWITCH;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: get_switch() returned NULL.")).str(SGLOCALE));
		return false;
	}

	get_md5sum_func md5sum_fn = get_md5sum_func(::dlsym(handle, "get_client_md5sum"));
	if (!md5sum_fn) {
		DBCT->_DBCT_error_code = DBCENOSWITCH;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function get_client_md5sum() doesn't exist in library {1}") % lib_name).str(SGLOCALE));
		return false;
	}

	md5sum = (*md5sum_fn)();

	if (bbp != NULL)
		bbversion = bbp->bbparms.bbversion;

	return true;
}

boost::once_flag dbcp_ctx::once_flag = BOOST_ONCE_INIT;
dbcp_ctx * dbcp_ctx::_instance = NULL;

bool dbcp_ctx::get_config(const string& dbc_key, int dbc_version, const string& sdc_key, int sdc_version)
{
	string config;
	sg_api& api_mgr = sg_api::instance();
	sys_func& func_mgr = sys_func::instance();
	gpp_ctx *GPP = gpp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("dbc_key={1}, dbc_version={2}, sdc_key={3}, sdc_version={4}") % dbc_key % dbc_version % sdc_key % sdc_version).str(SGLOCALE), &retval);
#endif

	if (!dbc_key.empty()) {
		// 获取DBC配置文件
		if (!api_mgr.get_config(config, dbc_key, dbc_version)) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failure to load DBC configuration")).str(SGLOCALE));
			return retval;
		}

		// 把配置文件写入临时文件
		func_mgr.tempnam(_DBCP_dbcconfig);

		ofstream ofs(_DBCP_dbcconfig.c_str());
		if (!ofs) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = UOPEN;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file to write DBC configuration")).str(SGLOCALE));
			return retval;
		}

		ofs << config;
		if (!ofs) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = UWRITE;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't write data to DBC configuration file")).str(SGLOCALE));
			return retval;
		}
	}

	if (!sdc_key.empty()) {
		// 获取SDC配置文件
		if (!api_mgr.get_config(config, sdc_key, sdc_version)) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failure to load SDC configuration")).str(SGLOCALE));
			return retval;
		}

		// 把配置文件写入临时文件
		func_mgr.tempnam(_DBCP_sdcconfig);

		ofstream ofs(_DBCP_sdcconfig.c_str());
		if (!ofs) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = UOPEN;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file to write SDC configuration")).str(SGLOCALE));
			return retval;
		}

		ofs << config;
		if (!ofs) {
			SGT->_SGT_error_code = SGEOS;
			SGT->_SGT_native_code = UWRITE;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't write data to SDC configuration file")).str(SGLOCALE));
			return retval;
		}
	}

	// 打开配置文件
	dbc_config& config_mgr = dbc_config::instance();
	if (!config_mgr.open())
		return retval;

	// 检查版本是否一致
	string result;
	if (!config_mgr.md5sum(result, true)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: md5sum failed")).str(SGLOCALE));
		return retval;
	}

	if (result != _DBCP_once_switch.get_md5sum()) {
		SGT->_SGT_error_code = SGESYSTEM;
		SGT->_SGT_native_code = UWRITE;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: dbcclient library and configuration file don't match")).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

dbcp_ctx * dbcp_ctx::instance()
{
	if (_instance == NULL)
		boost::call_once(once_flag, dbcp_ctx::init_once);
	return _instance;
}

void dbcp_ctx::clear()
{
	for (int i = 0; i < MAX_SQLITES; i++) {
		dbc_sqlite_t& sqlite = _DBCP_sqlites[i];

		if (sqlite.select_stmt != NULL) {
			sqlite3_finalize(sqlite.select_stmt);
			sqlite.select_stmt = NULL;
		}

		if (sqlite.insert_stmt != NULL) {
			sqlite3_finalize(sqlite.insert_stmt);
			sqlite.insert_stmt = NULL;
		}

		if (sqlite.delete_stmt != NULL) {
			sqlite3_finalize(sqlite.delete_stmt);
			sqlite.delete_stmt = NULL;
		}

		if (sqlite.db != NULL) {
			sqlite3_close(sqlite.db);
			sqlite.db = NULL;
		}
	}
}

bool dbcp_ctx::get_mlist(sgt_ctx *SGT)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif
	gpp_ctx *GPP = gpp_ctx::instance();
	sg_config& config_mgr = sg_config::instance(SGT);
	sgc_ctx *_SGC = SGT->SGC();
	sg_bboard_t *bbp = _SGC->_SGC_bbp;
	sg_bbparms_t& bbparms = bbp->bbparms;

	for (sg_config::svr_iterator siter = config_mgr.svr_begin(); siter != config_mgr.svr_end(); ++siter) {
		string clopt = siter->bparms.clopt;

		boost::char_separator<char> sep(" \t\b");
		tokenizer tokens(clopt, sep);
		tokenizer::iterator iter;

		// 跳过--前的选项
		for (iter = tokens.begin(); iter != tokens.end(); ++iter) {
			if (*iter == "--") {
				++iter;
				break;
			}
		}

		for (; iter != tokens.end(); ++iter) {
			if (*iter == "-m" || *iter == "--master") {
				std::pair<int, string> item;
				sg_sgent_t sgent;

				sgent.grpid = siter->sparms.svrproc.grpid;
				sgent.sgname[0] = '\0';
				if (!config_mgr.find(sgent)) {
					SGT->_SGT_error_code = SGESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find group for pgid {1}") % sgent.grpid).str(SGLOCALE));
					return retval;
				}

				item.first = _SGC->lmid2mid(sgent.lmid[bbparms.current]);
				item.second = _SGC->mid2pmid(item.first);
				_DBCP_mlist.push_back(item);
				break;
			}
		}
	}

	retval = true;
	return retval;
}

dbcp_ctx::dbcp_ctx()
{
	_DBCP_proc_type = DBC_STANDALONE;

	_DBCP_attach_addr = NULL;
	_DBCP_attached_count = 0;
	_DBCP_attached_clients = 0;

	for (int i = 0; i < MAX_SQLITES; i++) {
		dbc_sqlite_t& sqlite = _DBCP_sqlites[i];

		sqlite.select_stmt = NULL;
		sqlite.insert_stmt = NULL;
		sqlite.delete_stmt = NULL;
		sqlite.db = NULL;
	}

	_DBCP_reuse = false;
	_DBCP_enable_sync = false;
	_DBCP_is_server = false;
	_DBCP_is_sync = false;
	_DBCP_is_admin = false;
	_DBCP_current_pid = getpid();
	_DBCP_search_type = SEARCH_TYPE_NONE;
}

dbcp_ctx::~dbcp_ctx()
{
}

void dbcp_ctx::init_once()
{
	_instance = new dbcp_ctx();
}

}
}

