#if !defined(__COMPILER_H__)
#define __COMPILER_H__

#include "common.h"
#include "dstream.h"

namespace ai
{
namespace sg
{

using namespace std;

enum search_type_enum
{
	SEARCH_TYPE_NONE = 0,
	SEARCH_TYPE_DBC = 1,
	SEARCH_TYPE_SDC = 2,
	SEARCH_TYPE_CAL = 3
};

struct compiler_cache_t
{
	pid_t pid;
	int functions;
	char clopt[255 + 1];
	char libname[255 + 1];
	size_t size;
	time_t mtime;
};

class compiler
{
public:
	typedef int (*func_type)(void *client, char **global, const char **in, char **out);

	compiler(search_type_enum search_type = SEARCH_TYPE_NONE, bool do_cleanup_ = true);
	~compiler();

	string get_filename() const;
	string get_libname() const;
	int create_function(const string& src_code, const map<string, int>& rplc,
		const map<string, int>& in_rplc, const map<string, int>& out_rplc);
	func_type& get_function(int index);
	void build(bool require_build = true);
	void set_cplusplus(bool cplusplus_option_);
	void set_search_type(search_type_enum search_type_);

protected:
	void init();
	void set_func(int func_size, const char *lib_name_);
	void cleanup();
	void precomp(string& dst_code, const string& src_code, const map<string, int>& rplc,
		const map<string, int>& in_rplc, const map<string, int>& out_rplc);
	void write(const char* buf);
	int new_task(const char *task);

	search_type_enum search_type;
	bool do_cleanup;
	func_type *func_vector;

	string file_name;
	int fd;
	int func_size;
	void *handle;
	bool inited;
	bool built;
	bool cplusplus_option;
	ostringstream fmt;
};

}
}

#endif

