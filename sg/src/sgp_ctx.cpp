#include "sg_internal.h"

namespace bi = boost::interprocess;
namespace bf = boost::filesystem;
namespace bp = boost::posix_time;
using namespace std;

namespace ai
{
namespace sg
{

int const MAX_CACHED_MSGS = 10;
int const MAX_KEEP_SIZE = 1024 * 1024;

boost::once_flag sgp_ctx::once_flag = BOOST_ONCE_INIT;
sgp_ctx * sgp_ctx::_instance = NULL;

sgp_ctx * sgp_ctx::instance()
{
	if (_instance == NULL)
		boost::call_once(once_flag, sgp_ctx::init_once);
	return _instance;
}

sgp_ctx::sgp_ctx()
{
	_SGP_boot_flags = 0;
	_SGP_shutdown = 0;
	_SGP_shutdown_due_to_sigterm = false;
	_SGP_shutdown_due_to_sigterm_called = false;

	_SGP_ssgid = -1;
	_SGP_qsgid = -1;

 	_SGP_svrinit_called = false;
	_SGP_svrproc.clear();

	_SGP_grpid = DFLT_PGID;
	_SGP_svrid = CLT_PRCID;
	_SGP_svridmin = -1;
	_SGP_stdmain_booted = false;

	_SGP_adv_all = false;
	_SGP_svcidx = -1;

	_SGP_oldpid = 0;
	_SGP_threads = 0;
	_SGP_let_svc = false;

	_SGP_amserver = false;
	_SGP_ambbm = false;
	_SGP_amdbbm = false;
	_SGP_amgws = false;
	_SGP_ambsgws = false;
	_SGP_amsgboot = false;

	_SGP_ctx_state = CTX_STATE_UNINIT;
	_SGP_ctx_count = 0;
	_SGP_netload = 0;
	_SGP_dbbm_resp_timeout = 0;

	_SGP_max_cached_msgs = MAX_CACHED_MSGS;
	_SGP_max_keep_size = MAX_KEEP_SIZE;

	_SGP_bbasw_mgr = NULL;
	_SGP_ldbsw_mgr = NULL;
	_SGP_authsw_mgr = NULL;
	_SGP_switch_mgr = NULL;

	_SGP_queue_warned = false;
	_SGP_queue_expire = 0;

	setenv();
}

sgp_ctx::~sgp_ctx()
{
	delete _SGP_switch_mgr;
	delete _SGP_authsw_mgr;
	delete _SGP_ldbsw_mgr;
	delete _SGP_bbasw_mgr;
}

void sgp_ctx::setenv()
{
	gpp_ctx *GPP = gpp_ctx::instance();
	char *ptr;

	try {
		string debug_level;
		if ((ptr = ::getenv("DEBUG_LEVEL")) != NULL) {
			debug_level = ptr;

			string::size_type pos = debug_level.find('-');
			if (pos == string::npos) {
				_SGP_debug_level_lower = 0;
				_SGP_debug_level_upper = boost::lexical_cast<int>(debug_level);
			} else {
				_SGP_debug_level_lower = boost::lexical_cast<int>(debug_level.substr(0, pos));
				_SGP_debug_level_upper = boost::lexical_cast<int>(debug_level.substr(pos + 1, debug_level.length() - pos - 1));
			}
		} else {
			_SGP_debug_level_lower = 0;
			_SGP_debug_level_upper = std::numeric_limits<int>::max();
		}
	} catch (exception& ex) {
		GPP->write_log((_("WARN: Bad format for environment DEBUG_LEVEL, ignored")).str(SGLOCALE));
		_SGP_debug_level_lower = 0;
		_SGP_debug_level_upper = std::numeric_limits<int>::max();
	}

	try {
		if ((ptr = ::getenv("SGMAXCACHEDMSGS")) != NULL) {
			_SGP_max_cached_msgs = boost::lexical_cast<int>(ptr);
			if (_SGP_max_cached_msgs <= 0) {
				GPP->write_log((_("WARN: Bad format for environment SGMAXCACHEDMSGS, ignored")).str(SGLOCALE));
				_SGP_max_cached_msgs = MAX_CACHED_MSGS;
			}
		}
	} catch (exception& ex) {
		GPP->write_log((_("WARN: Bad format for environment SGMAXCACHEDMSGS, ignored")).str(SGLOCALE));
		_SGP_max_cached_msgs = MAX_CACHED_MSGS;
	}

	try {
		if ((ptr = ::getenv("SGMAXKEEPSIZE")) != NULL) {
			_SGP_max_keep_size = boost::lexical_cast<int>(ptr);
			if (_SGP_max_keep_size <= 0) {
				GPP->write_log((_("WARN: Bad format for environment SGMAXKEEPSIZE, ignored")).str(SGLOCALE));
				_SGP_max_keep_size = MAX_KEEP_SIZE;
			}
		}
	} catch (exception& ex) {
		GPP->write_log((_("WARN: Bad format for environment SGMAXKEEPSIZE, ignored")).str(SGLOCALE));
		_SGP_max_keep_size = MAX_KEEP_SIZE;
	}
}

int sgp_ctx::create_ctx()
{
	sgc_pointer SGC(new sgc_ctx());
	bi::scoped_lock<boost::recursive_mutex> lock(_SGP_ctx_mutex);

	// 在CTX_STATE_MULTI状态下，不使用SGSINGLECONTEXT
	int start;
	if (_SGP_ctx_state == CTX_STATE_MULTI) {
		if (_SGP_ctx_array.empty())
			_SGP_ctx_array.resize(1);
		start = 1;
	} else {
		start = 0;
	}

	for (int i = start; i < _SGP_ctx_array.size(); i++) {
		if (_SGP_ctx_array[i] == NULL) {
			SGC->_SGC_ctx_id = i;
			_SGP_ctx_array[i] = SGC;
			if (SGC->_SGC_ctx_id > 0)
				_SGP_ctx_count++;
			return SGC->_SGC_ctx_id;
		}
	}

	SGC->_SGC_ctx_id = _SGP_ctx_array.size();
	_SGP_ctx_array.push_back(SGC);
	if (SGC->_SGC_ctx_id > 0)
		_SGP_ctx_count++;
	return SGC->_SGC_ctx_id;
}

int sgp_ctx::destroy_ctx(int context)
{
	bi::scoped_lock<boost::recursive_mutex> lock(_SGP_ctx_mutex);

	if (context < 0 || context >= _SGP_ctx_array.size())
		return -1;

	_SGP_ctx_array[context].reset();
	if (context > 0)
		--_SGP_ctx_count;
	return _SGP_ctx_count;
}

bool sgp_ctx::valid_ctx(int context)
{
	bi::scoped_lock<boost::recursive_mutex> lock(_SGP_ctx_mutex);

	if (context == SGNULLCONTEXT || context == SGINVALIDCONTEXT)
		context = 0;

	if (context >= _SGP_ctx_array.size() || context < 0)
		return false;

	if (_SGP_ctx_array[context] == NULL)
		return false;

	return true;
}

bool sgp_ctx::putenv(const string& filename, const sg_mchent_t& mchent)
{
	const char *name;
	const char *value;
	bf::path path1;
	bf::path path2;

	if (!gpenv::instance().read_env(filename))
		return false;

	// 检查特定的环境变量
	for (int i = 0; i < 3; i++) {
		switch (i) {
		case 0:
			name = "SGROOT";
			value = mchent.sgdir;
			break;
		case 1:
			name = "APPROOT";
			value = mchent.appdir;
			break;
		case 2:
			name = "SGPROFILE";
			value = mchent.sgconfig;
			break;
		}

		path1 = value;

		char *ptr = gpenv::instance().getenv(name);
		if (ptr != NULL)
			path2 = ptr;

		if (path1 != path2) {
			if (ptr != NULL) {
				string msg = (_("WARN: {1} value \"{2}\" in environment does not match configuration \"{3}\"")
					% name % ptr % value).str();
				gpp_ctx::instance()->write_log(__FILE__, __LINE__, msg);
			}

			string env = name;
			env += '=';
			env += value;
			gpenv::instance().putenv(env.c_str());
		}
	}

	// 检查PATH
	value = gpenv::instance().getenv(_TMPATH);
	if (value == NULL)
		value = "";
	string path = (boost::format("%1%=%2%/bin:%3%/bin:/bin:/usr/bin")
		% _TMPATH % mchent.sgdir % mchent.appdir).str();
	int prefix_len = strlen(_TMPATH) + 1;
	if (strncmp(path.c_str() + prefix_len, value, path.length() - prefix_len) != 0) {
		path += ':';
		path += value;
		gpenv::instance().putenv(path.c_str());
	}

	// 检查LIBPATH
	value = gpenv::instance().getenv(_TMLDPATH);
	if (value == NULL)
		value = "";
	string libpath = (boost::format("%1%=%2%/lib:%3%/lib:/lib:/usr/lib")
		% _TMLDPATH % mchent.sgdir % mchent.appdir).str();
	prefix_len = strlen(_TMLDPATH) + 1;
	if (strncmp(libpath.c_str() + prefix_len, value, libpath.length() - prefix_len) != 0) {
		libpath += ':';
		libpath += value;
		gpenv::instance().putenv(libpath.c_str());
	}

	return true;
}

void sgp_ctx::setids(sg_proc_t& proc)
{
	proc.grpid = _SGP_grpid;
	proc.svrid = _SGP_svrid;
}

bool sgp_ctx::do_auth(int perm, int ipckey, string& passwd, bool verify)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();
	gpenv& env_mgr = gpenv::instance();

	char *ptr = env_mgr.getenv("HOME");
	if (ptr == NULL) {
		if (isatty(0))
			std::cout << (_("ERROR: HOME environment is not set\n")).str(SGLOCALE);
		else
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: HOME environment is not set")).str(SGLOCALE));
		SGT->_SGT_error_code = SGESYSTEM;
		return false;
	}

	int const IPCKEY_LEN = 10;
	int const PASSWD_LEN = 32;
	int const TOTAL_LEN = IPCKEY_LEN + PASSWD_LEN + 1;

	string ipckey_str = boost::lexical_cast<string>(ipckey & ((1 << MCHIDSHIFT) - 1));
	ipckey_str.resize(IPCKEY_LEN);

	char buf[TOTAL_LEN];
	off_t offset = 0;
	string read_passwd;

	// 创建文件
	string filename = (boost::format("%1%/.sgpasswd") % ptr).str();
	int passwd_fd = open(filename.c_str(), O_CREAT | O_RDWR, perm);
	if (passwd_fd == -1) {
		string errmsg = (_("ERROR: Can't open file {1}, {2}") % filename % strerror(errno)).str(SGLOCALE);
		if (isatty(0))
			std::cout << errmsg << std::endl;
		else
			GPP->write_log(__FILE__, __LINE__, errmsg);
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = UOPEN;
		return false;
	}

	BOOST_SCOPE_EXIT((&passwd_fd)) {
		close(passwd_fd);
	} BOOST_SCOPE_EXIT_END

	// 读取密码
	while (read(passwd_fd, buf, sizeof(buf)) == sizeof(buf)) {
		if (memcmp(ipckey_str.c_str(), buf, IPCKEY_LEN) == 0) {
			read_passwd.assign(buf + IPCKEY_LEN, buf + IPCKEY_LEN + PASSWD_LEN);
			break;
		}
		offset += TOTAL_LEN;
	}

	// 需要验证密码
	if (verify) {
		// 密码相等
		if (read_passwd == passwd)
			return true;
	} else {
		// 调用者已经提供密码
		if (!passwd.empty()) {
			// 保存密码
			if (lseek(passwd_fd, offset, SEEK_SET) == -1) {
				std::cerr << (_("ERROR: Can't seek file position.\n")).str(SGLOCALE);
				SGT->_SGT_error_code = SGEOS;
				SGT->_SGT_native_code = ULSEEK;
				return false;
			}

			if (write(passwd_fd, ipckey_str.c_str(), IPCKEY_LEN) != IPCKEY_LEN
				|| write(passwd_fd, passwd.c_str(), PASSWD_LEN) != PASSWD_LEN
				|| write(passwd_fd, "\n", 1) != 1) {
				std::cerr << (_("ERROR: Can't write data to passwd file.\n")).str(SGLOCALE);
				SGT->_SGT_error_code = SGEOS;
				SGT->_SGT_native_code = UWRITE;
				return false;
			}

			return true;
		}
	}

	// 需要输入密码，如果不是终端，则错误
	if (!isatty(0)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: file {1} does not exist") % filename).str(SGLOCALE));
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = UOPEN;
		return false;
	}

	while (1) {
		string input_passwd;
		std::cerr << "\n" << (_("Please input password: ")).str(SGLOCALE) << std::flush;
		if (!(cin >> input_passwd)) {
			// 用户取消操作
			SGT->_SGT_error_code = SGESYSTEM;
			return false;
		}

		string encrypted_password;
		if (!encrypt(input_passwd, encrypted_password)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't encrypt password")).str(SGLOCALE));
			SGT->_SGT_error_code = SGESYSTEM;
			return false;
		}

		if (verify) {
			// 验证密码
			if (encrypted_password != passwd) {
				std::cerr << (_("invalid passwd, try again\n")).str(SGLOCALE) << std::flush;
				boost::this_thread::sleep(bp::seconds(1));
				continue;
			}
		} else {
			if (read_passwd != encrypted_password) {
				// 保存密码
				if (lseek(passwd_fd, offset, SEEK_SET) == -1) {
					std::cerr << (_("ERROR: Can't seek file position.\n")).str(SGLOCALE);
					SGT->_SGT_error_code = SGEOS;
					SGT->_SGT_native_code = ULSEEK;
					return false;
				}

				if (write(passwd_fd, ipckey_str.c_str(), IPCKEY_LEN) != IPCKEY_LEN
					|| write(passwd_fd, encrypted_password.c_str(), PASSWD_LEN) != PASSWD_LEN
					|| write(passwd_fd, "\n", 1) != 1) {
					std::cerr << (_("ERROR: Can't write data to passwd file.\n")).str(SGLOCALE);
					SGT->_SGT_error_code = SGEOS;
					SGT->_SGT_native_code = UWRITE;
					return false;
				}
			}

			passwd = encrypted_password;
		}

		return true;
	}
}

bool sgp_ctx::encrypt(const string& password, string& encrypted_password)
{
	sgt_ctx *SGT = sgt_ctx::instance();

	// 生成md5sum
	string command = (boost::format("echo \"%1%\"|") % password).str();
	if (access("/usr/bin/md5sum", X_OK) != -1)
		command += "/usr/bin/md5sum";
	else if (access("/bin/md5sum", X_OK) != -1)
		command += "/bin/md5sum";
	else
		command += "md5sum";

	FILE *fp = popen(command.c_str(), "r");
	if (!fp) {
		std::cerr << (_("ERROR: Can't open pipe to execute command.\n")).str(SGLOCALE);
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = UOPEN;
		return false;
	}

	BOOST_SCOPE_EXIT((&fp)) {
		fclose(fp);
	} BOOST_SCOPE_EXIT_END

	// 获取md5sum
	char buf[32];
	int cnt = fread(buf, sizeof(buf), 1, fp);
	if (cnt != 1) {
		std::cerr << (_("ERROR: Can't read data from pipe.\n")).str(SGLOCALE);
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = UREAD;
		return false;
	}

	encrypted_password = string(buf, buf + 32);
	return true;
}

void sgp_ctx::clear()
{
	bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(_SGP_queue_mutex);

	_SGP_queue_map.clear();
	_SGP_queue_warned = false;
	_SGP_queue_expire = 0;
}

void sgp_ctx::init_once()
{
	_instance = new sgp_ctx();
}

}
}

