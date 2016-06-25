/////////////////////////////////////////////////////////////////////////////////
// modification history
//-------------------------------------------------------------------------------
// add method
// datetime& datetime::operator=(const string& dts)
// datetime& datetime::operator=(const char* pdatestr)
// datetime& datetime::assign(const string& dts, const char* format)
//--------------------------------------------------------------------------------
#ifndef __DB_COMMON_H__
#define __DB_COMMON_H__

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <cctype>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <libgen.h>
#include <string>
#include <vector>
#include <regex.h>
#include <dirent.h>
#include <algorithm>
#include <functional>
#include <netdb.h>
#include <fcntl.h>
#ifdef HPUX
#include <sys/param.h>
#include <sys/pstat.h>
#endif
#ifdef TRUE64
#include <sys/procfs.h>
#endif
#include "machine.h"
#include "database.h"
#include "db_exception.h"
#include "user_exception.h"
#include "log_file.h"
#include "user_assert.h"
#include "macro.h"

namespace ai
{
namespace sg
{

using namespace std;

struct param {
	string user_name;
	string password;
	string connect_string;
	int port;
	key_t ipc_key;
	string log_path;
	log_file::switch_mode mode;
	string host_id;
	string module_id;
	string proc_id;
	string thread_id;
	database *db;
	//信控需要的一个内存入口指针
	char* sys_param;
};

class db_common {
public:
	// Get file sn(range from 0) from database.
	static unsigned int get_file_sn(database& db, const string& sn_name = "file_sequence");
	// Get database password according to user_name and connect_string.
	static bool get_passwd(database& db, const string& user_name, const string& connect_string, string& passwd);
	static void load_param(param& para);
};

}
}

#endif

