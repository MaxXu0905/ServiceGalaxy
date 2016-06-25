#include "gpp_ctx.h"
#include "gpenv.h"
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

namespace bf = boost::filesystem;

namespace ai
{
namespace sg
{

boost::once_flag gpp_ctx::once_flag = BOOST_ONCE_INIT;
gpp_ctx * gpp_ctx::_instance = NULL;

const char SGVERSION[] = "1.0";

gpp_ctx * gpp_ctx::instance()
{
	if (_instance == NULL)
		boost::call_once(once_flag, gpp_ctx::init_once);
	return _instance;
}

gpp_ctx::gpp_ctx()
{
	_GPP_ver_flag = false;
	_GPP_do_threads = false;
	_GPP_thrid = pthread_self();
	_GPP_sig_pending = 0;
	_GPP_sig_defer = 0;
	_GPP_skip_log = false;

	setenv();
}

gpp_ctx::~gpp_ctx()
{
}

void gpp_ctx::setenv()
{
	char *ptr;
	string ulogpfx;
	if ((ptr = ::getenv("SGLOGNAME")) != NULL) {
		ulogpfx = ptr;
	} else if ((ptr = ::getenv("APPROOT")) != NULL) {
		ulogpfx = ptr;
		ulogpfx += "/LOG";
	} else {
		ulogpfx = "LOG";
	}
	if (ulogpfx != _GPP_ulogpfx) {
		_GPP_ulogpfx = ulogpfx;
		lfile.reset(new log_file(log_file::mode_day, _GPP_ulogpfx));
		_GPP_ver_flag = false;
	} else {
		lfile->setenv();
	}
}

void gpp_ctx::set_ulogpfx(const string& ulogpfx)
{
	lfile.reset(new log_file(log_file::mode_day, ulogpfx, _GPP_procname));
}

void gpp_ctx::set_procname(const string& procname)
{
	bf::path path(procname);
	_GPP_procname = path.filename().native();
	if (lfile != NULL)
		lfile->set_procname(_GPP_procname);
}

const string& gpp_ctx::uname()
{
	if (_GPP_uname.empty()) {
		gpenv& env_mgr = gpenv::instance();
		char *ptr = env_mgr.getenv("PMID");
		if (ptr == NULL) {
			char name[128];

			if(gethostname(name, sizeof(name)) >= 0) {
				name[sizeof(name) - 1] = '\0';
				_GPP_uname = name;
			}
		} else {
			_GPP_uname = ptr;
		}
	}

	return _GPP_uname;
}

void gpp_ctx::write_log(const char* filename, int linenum, const char* msg, int len)
{
	if (_GPP_skip_log)
		return;

	if (!_GPP_ver_flag)
		write_version();

	if (len > 0 || msg[0] != '\0')
		lfile->write_log(filename, linenum, msg, len);
}

void gpp_ctx::write_log(const char* filename, int linenum, const string& msg)
{
	if (_GPP_skip_log)
		return;

	if (!_GPP_ver_flag)
		write_version();

	if (!msg.empty())
		lfile->write_log(filename, linenum, msg);
}

void gpp_ctx::write_log(const string& msg)
{
	if (_GPP_skip_log)
		return;

	if (!_GPP_ver_flag)
		write_version();

	if (!msg.empty())
		lfile->write_log(msg);
}

void gpp_ctx::write_log(const char *msg)
{
	if (_GPP_skip_log)
		return;

	if (!_GPP_ver_flag)
		write_version();

	if (msg[0] != '\0')
		lfile->write_log(msg);
}

void gpp_ctx::save_env(char *env)
{
	string key;
	char *ptr = strchr(env, '=');

	if (ptr == NULL)
		key = env;
	else
		key = string(env, ptr);
	_GPP_gpenv_save[key] = boost::shared_array<char>(env);
}

void gpp_ctx::write_version()
{
	_GPP_ver_flag = true;

	lfile->write_log((_("INFO: ServiceGalaxy version {1}, {2}-bit")
		% SGVERSION % (sizeof(long) * 8)).str(SGLOCALE));
}

void gpp_ctx::init_once()
{
	_instance = new gpp_ctx();
}

}
}

