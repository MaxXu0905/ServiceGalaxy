#if !defined(__GPP_CTX_H__)
#define __GPP_CTX_H__

#include <unistd.h>
#include <string>
#include <map>
#include "log_file.h"
#include "uunix.h"
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread/once.hpp>

namespace ai
{
namespace sg
{

class gpp_ctx
{
private:
	boost::shared_ptr<log_file> lfile;
	std::map<std::string, boost::shared_array<char> > _GPP_gpenv_save;

public:
	static gpp_ctx * instance();
	~gpp_ctx();

	void setenv();
	void set_ulogpfx(const std::string& ulogpfx);
	void set_procname(const std::string& procname);
	const std::string& uname();
	void write_log(const char *filename, int linenum, const char *msg, int len = 0);
	void write_log(const char *filename, int linenum, const std::string& msg);
	void write_log(const std::string& msg);
	void write_log(const char *msg);

	void save_env(char *env);

	std::string _GPP_procname;	// ³ÌÐòÃû³Æ
	std::string _GPP_uname;
	std::string _GPP_ulogpfx;
	bool _GPP_ver_flag;
	bool _GPP_do_threads;
	pthread_t _GPP_thrid;

	int _GPP_sig_pending;	// 1 or more signals pending
	int _GPP_sig_defer;	// "level" to which signals deferred
	bool _GPP_skip_log;

private:
	gpp_ctx();

	void write_version();

	static void init_once();

	static boost::once_flag once_flag;
	static gpp_ctx *_instance;
};

}
}

#endif

