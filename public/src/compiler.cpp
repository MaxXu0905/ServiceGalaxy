#include <dlfcn.h>
#include "compiler.h"

namespace ai
{
namespace sg
{

compiler::compiler(search_type_enum search_type_, bool do_cleanup_)
	: search_type(search_type_)
{
	char *ptr = ::getenv("SGCNOCLEAN");
	if (ptr == NULL)
		do_cleanup = do_cleanup_;
	else if (strcasecmp(ptr, "Y") == 0)
		do_cleanup = false;

	func_vector = NULL;
	fd = -1;
	func_size = 0;
	handle = NULL;
	inited = false;
	built = false;
	cplusplus_option = false;
}

compiler::~compiler()
{
	if (fd != -1) {
		::close(fd);
		fd = -1;
	}

	if (handle) {
		::dlclose(handle);
		handle = NULL;
	}

	if (do_cleanup)
		cleanup();

	if (func_vector)
		delete []func_vector;
}

string compiler::get_filename() const
{
	return file_name;
}

string compiler::get_libname() const
{
#if defined(__hpux)
	return file_name.substr(0, file_name.length() - 1) + "sl";
#else
	return file_name.substr(0, file_name.length() - 1) + "so";
#endif
}

int compiler::create_function(const string& src_code, const map<string, int>& rplc,
	const map<string, int>& in_rplc, const map<string, int>& out_rplc)
{
	string dst_code;
	string tmp;

	if (!inited)
		init();

	if (built)
		throw bad_system(__FILE__, __LINE__, 2, bad_system::bad_dlopen, (_("ERROR: create_function() can't be called after build()")).str(SGLOCALE));

	fmt.str("");
	fmt << "fun_" << func_size;
	tmp = fmt.str();

	precomp(dst_code, src_code, rplc, in_rplc, out_rplc);
	write("#if defined(__cplusplus)\n");
	write("extern \"C\"\n");
	write("#endif\n");
	write("int ");
	write(tmp.c_str());
	write("(void *instance_mgr, char **global, const char **in, char **out)\n");
	write("{\n");
	write(dst_code.c_str());
	write("\treturn 0;\n");
	write("}\n\n");

	return func_size++;
}

compiler::func_type& compiler::get_function(int index)
{
	if (index < 0 || index >= func_size)
		throw bad_param(__FILE__, __LINE__, 1, (_("ERROR: index out of bound")).str(SGLOCALE));

	if (func_vector != NULL && func_vector[index] != NULL)
		return func_vector[index];

	if (!built)
		throw bad_system(__FILE__, __LINE__, 2, bad_system::bad_dlopen, (_("ERROR: get_function() can't be called before build()")).str(SGLOCALE));

	if (handle == NULL) {
		string lib_name = file_name.substr(0, file_name.length() - 1) + "so";
		handle = ::dlopen(lib_name.c_str(), RTLD_LAZY);
		if (!handle)
			throw bad_system(__FILE__, __LINE__, 3, bad_system::bad_dlopen, ::dlerror());
	}

	fmt.str("");
	fmt << "fun_" << index;
	string func_name = fmt.str();
	func_type fn = func_type(::dlsym(handle, func_name.c_str()));
	if (!fn)
		throw bad_system(__FILE__, __LINE__, 122, bad_system::bad_dlsym, func_name);

	if (func_vector == NULL)
		func_vector = new func_type[func_size];
	func_vector[index] = fn;
	return func_vector[index];
}

void compiler::build(bool require_build)
{
	if (func_size <= 0)
		throw bad_system(__FILE__, __LINE__, 123, bad_system::bad_cmd, (_("ERROR: No function provided")).str(SGLOCALE));

	if (built)
		throw bad_system(__FILE__, __LINE__, 2, bad_system::bad_dlopen, (_("ERROR: built() already done")).str(SGLOCALE));

	::close(fd);
	fd = -1;

	if (require_build) {
		char *ptr;
		string cc;
		string cflags;

#if defined(__hpux)
		string lib_name = file_name.substr(0, file_name.length() - 1) + "sl";
#else
		string lib_name = file_name.substr(0, file_name.length() - 1) + "so";
#endif
		string obj_name = file_name.substr(0, file_name.length() - 1) + "o";
		string log_name = file_name.substr(0, file_name.length() - 1) + "log";

		if (!cplusplus_option) {
			ptr = ::getenv("CC");
			if (ptr == NULL || ptr[0] == '\0')
				cc = "cc";
			else
				cc = ptr;
		} else {
			ptr = ::getenv("CXX");
			if (ptr == NULL || ptr[0] == '\0') {
#if defined(sun)
				cc = "CC";
#elif defined(__hpux)
				cc = "aCC";
#elif defined(_AIX)
	 			cc = "xlC";
#elif defined(__linux__)
	 			cc = "g++";
#else
				cc = "cc";
#endif
			} else {
				cc = ptr;
			}
		}

		string full_cc = string("/usr/bin/") + cc;
		if (access(full_cc.c_str(), X_OK) != -1) {
			cc = full_cc;
			goto SKIP_LABEL;
		}

		full_cc = string("/bin/") + cc;
		if (access(full_cc.c_str(), X_OK) != -1)
			cc = full_cc;

SKIP_LABEL:
		if (!cplusplus_option)
			ptr = ::getenv("CFLAGS");
		else
			ptr = ::getenv("CXXFLAGS");
		if (ptr == NULL) {
			cflags = "";
		} else {
			cflags = ptr;
			cflags += ' ';
		}

		string cmd = cc;
		cmd += " ";

		/* Special compile options that:
		 * 1) we are built with, and
		 * 2) that users must use when they use us to build their apps.
		 */
#if defined(sun)
#ifdef __sparcv9
		cmd += "-xarch=v9 -w ";
#elif defined(__amd64)
		cmd += "-xarch=amd64 -fnonstd  -w ";
#else
		cmd += "-w ";
#endif /* __sparcv9 */
		cmd += "-G -Kpic ";
#elif defined(_IBMR2)
#if defined(__64BIT__)
		cmd += "-q64 -D_LARGE_FILES -D__XCOFF32__ -D__XCOFF64__ -brtl -qstaticinline ";
#else
		cmd += "-brtl -qstaticinline -qrtti=all ";
#endif /* __64BIT__ */
		cmd += "-G ";
#elif defined(__hpux)
#if defined(__LP64__) && defined(__BIGMSGQUEUE_ENABLED)
#ifdef __ia64
		cmd += "+DD64 +Olit=all -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#else
		cmd += "+DA2.0W -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#endif /* __ia64 */
#elif defined(__LP64__)
#ifdef __ia64
		cmd += "+DD64 +Olit=all -Wl,+s ";
#else
		cmd += "+DA2.0W -Wl,+s ";
#endif /* __ia64 */
#elif defined(__BIGMSGQUEUE_ENABLED)
#ifdef __ia64
		cmd += "+DD32 +Olit=all -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#else
		cmd += "+DA1.1 -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#endif /* __ia64 */
#else
#ifdef __ia64
		cmd += "+DD32 +Olit=all -Wl,+s ";
#else
		cmd += "+DA1.1 -Wl,+s ";
#endif /* __ia64 */
#endif /* __LP64__, __BIGMSGQUEUE_ENABLED */
		cmd += "-b -dynamic ";
#elif defined(__linux__)
#if defined(__x86_64__)
		cmd += "-m64 ";
#else
		cmd += "-m32 ";
#endif
		cmd += "-shared -fPIC ";
#endif

#if defined(sun) || (defined(__hpux) && defined(__ia64))
		// -R/usr/lib/lwp is already included
		cmd += "-mt ";
#endif

		cmd += cflags;
		cmd += "-I${SGROOT}/include -I${APPROOT}/include -I${BOOST_ROOT} -o ";
		cmd += lib_name;
		cmd += " ";
		cmd += file_name;

#ifdef DYNAMIC
		cmd += "  -Bdynamic ";
#endif

		cmd += " -L${SGROOT}/lib -lm -lc -lnsl -lrt -ldl";

		switch (search_type) {
		case SEARCH_TYPE_DBC:
			cmd += " -L${APPROOT}/lib -ldbcsearch -lsearch";
			break;
		case SEARCH_TYPE_SDC:
			cmd += " -L${APPROOT}/lib -lsdcsearch -lsearch";
			break;
		case SEARCH_TYPE_CAL:
			cmd += " -L${APPROOT}/lib -lcalfunc";
			break;
		case SEARCH_TYPE_NONE:
		default:
			break;
		}

#if defined(_AIX)
		cmd += " -brtl -qstaticinline";
#endif

#ifdef DEBUG
		cmd += " -g";
#endif

		cmd += " 2> " + log_name;

		dout << "command: "<< cmd << std::endl;

		int status = new_task(cmd.c_str());
#if defined(WIFEXITED) && defined(WEXITSTATUS)
		if (WIFEXITED(status))
			status = WEXITSTATUS(status);
		else
			status = -1;
#endif

		if (status)
			throw bad_system(__FILE__, __LINE__, 117, bad_system::bad_cmd, (_("ERROR: command={1}, status={2}, {3}") % cmd % status % ::strerror(errno)).str(SGLOCALE));

		// If build success, we'll unlink log_name immediately.
		::unlink(log_name.c_str());
	}

	built = true;

	func_vector = new func_type[func_size];
	for (int i = 0; i < func_size; i++) {
		func_vector[i] = NULL;
		func_vector[i] = get_function(i);
	}
}

void compiler::set_cplusplus(bool cplusplus_option_)
{
	cplusplus_option = cplusplus_option_;
}

void compiler::set_search_type(search_type_enum search_type_)
{
	search_type = search_type_;
}


void compiler::init()
{
	int tries = 0;
	while (1) {
		char *temp_name = ::tempnam(NULL, "sgfun");
		if (temp_name == NULL)
			throw bad_file(__FILE__, __LINE__, 115, bad_file::bad_open, (_("ERROR: tempnam() failure, {1}") % strerror(errno)).str(SGLOCALE));

		file_name = temp_name;
		file_name += ".c";
		string obj_name = temp_name;
		obj_name += ".o";
		string lib_name = temp_name;
#if defined(__hpux)
		lib_name += ".sl";
#else
		lib_name += ".so";
#endif
		string log_name = temp_name;
		log_name += ".log";

		::free(temp_name);

		if ((fd = ::open(file_name.c_str(), O_EXCL | O_CREAT | O_WRONLY, 0666)) == -1) {
			if (++tries == 10)
				throw bad_file(__FILE__, __LINE__, 116, bad_file::bad_open, (_("ERROR: tempnam() failure, {1}") % strerror(errno)).str(SGLOCALE));
			continue;
		}

		// Include head files.
		write("#include <stdio.h>\n");
		write("#include <stdlib.h>\n");
		write("#include <math.h>\n");
		write("#include <errno.h>\n");
		write("#include <assert.h>\n");
		write("#include <string.h>\n");
		write("#include <strings.h>\n");
		write("#include <stdarg.h>\n");
		write("#include <time.h>\n");
		write("#include <unistd.h>\n");
		write("#include <ctype.h>\n");
		write("#include <fcntl.h>\n");

		if (cplusplus_option) {
			write("#include <string>\n");
			write("#include <list>\n");
			write("#include <vector>\n");
			write("#include <map>\n");
			write("#include <set>\n");
			write("#include <iomanip>\n");
			write("#include <algorithm>\n");
			write("#include <iostream>\n");
			write("#include \"dbc_function.h\"\n");
			write("\nusing namespace std;\n\n");

			const char *types[] = { "short", "int", "long", "float", "double", };
			const char *formats[] = { "%d", "%d", "%ld", "%f", "%lf" };

			for (int i = 0; i < 5; i++) {
				write ("string& operator+=(string& str, ");
				write(types[i]);
				write(" value)\n");
				write("{\n");
				write("\tchar buf[32];\n\n");
				write("\tsprintf(buf, \"");
				write(formats[i]);
				write("\", value);\n");
				write("\tstr += buf;\n");
				write("\treturn str;\n");
				write("}\n\n");
			}
		}

		if (search_type == SEARCH_TYPE_DBC || search_type == SEARCH_TYPE_SDC)
			write("#include \"search.h\"\n");
		else if(search_type == SEARCH_TYPE_CAL)
			write("#include \"cal_func.h\"\n");
		write("\n");

		handle = 0;
		inited = true;
		return;
	}
}

void compiler::set_func(int func_size_, const char *lib_name_)
{
	inited = false;
	built = false;

	func_size = func_size;
	string lib_name = lib_name_;
	file_name = lib_name.substr(0, lib_name.length() - 2) + "c";

	inited = true;
	built = true;
}

void compiler::cleanup()
{
	string lib_name = file_name.substr(0, file_name.length() - 1) + "so";
	string obj_name = file_name.substr(0, file_name.length() - 1) + "o";
	string log_name = file_name.substr(0, file_name.length() - 1) + "log";

	if (!file_name.empty())
		::unlink(file_name.c_str());

	if (!obj_name.empty())
		::unlink(obj_name.c_str()); // It may not succeed, just skip it.

	if (!lib_name.empty())
		::unlink(lib_name.c_str());

	if (!log_name.empty())
		::unlink(log_name.c_str());
}

void compiler::precomp(string& dst_code, const string& src_code, const map<string, int>& rplc,
	const map<string, int>& in_rplc, const map<string, int>& out_rplc)
{
	string::const_iterator iter;
	string token;

	dst_code = "";
	for (iter = src_code.begin(); iter != src_code.end();) {
		if (*iter == '\\') {
			// This charactor and next are a whole.
			dst_code += *iter;
			++iter;
			dst_code += *iter;
			++iter;
		} else if (*iter == '\"') {
			// Const string.
			dst_code += *iter;
			++iter;
			while (iter != src_code.end()) {
				// Skip const string.
				dst_code += *iter;
				++iter;
				if (*(iter-1) == '\"' && *(iter-2) != '\\')
					break;
			}
		} else if (*iter == '/' && (iter  + 1) != src_code.end() && *(iter + 1) == '*') {
			// Multi Line Comments
			dst_code += *iter;
			++iter;
			dst_code += *iter;
			++iter;
			while (iter != src_code.end()) {
				// Skip comment string.
				dst_code += *iter;
				++iter;
				if (*(iter - 1) == '/' && *(iter - 2) == '*')
					break;
			}
		} else if (*iter == '$') {
			// Input variables
			dst_code += "in[";
			iter++;
			if (*iter >= '0' && *iter <= '9') {
				for (; iter != src_code.end(); ++iter) {
					if (*iter >= '0' && *iter <= '9')
                        			dst_code += *iter;
                    			else
                        			break;
				}
			} else if (isalnum(*iter) || *iter == '_' || *iter == '.') {
				token = "";
				for (; iter != src_code.end(); ++iter) {
					if (isalnum(*iter) || *iter == '_' || *iter == '.')
						token += *iter;
					else
						break;
				}
				map<string, int>::const_iterator map_iter = in_rplc.find(token);
				if (map_iter == in_rplc.end()) {
					// Not found.
					throw bad_msg(__FILE__, __LINE__, 115, (_("ERROR: Input variable {1} not found in map.") % token).str(SGLOCALE));
				} else {
					fmt.str("");
					fmt << map_iter->second;
					dst_code += fmt.str();
				}
			}
			dst_code += ']';
		} else if (*iter == '#') {
			// Output variables
			dst_code += "out[";
			iter++;
			if (*iter >= '0' && *iter <= '9') {
				for (; iter != src_code.end(); ++iter) {
					if (*iter >= '0' && *iter <= '9')
						dst_code += *iter;
					else
						break;
				}
			} else if (isalnum(*iter) || *iter == '_' || *iter == '.') {
				token = "";
				for (; iter != src_code.end(); ++iter) {
					if (isalnum(*iter) || *iter == '_' || *iter == '.')
						token += *iter;
					else
						break;
				}
				map<string, int>::const_iterator map_iter = out_rplc.find(token);
				if (map_iter == out_rplc.end()) {
					// Not found.
					throw bad_msg(__FILE__, __LINE__, 116, (_("ERROR: Output variable {1} not found in map.") % token).str(SGLOCALE));
				} else {
					fmt.str("");
					fmt << map_iter->second;
					dst_code += fmt.str();
				}
			}
			dst_code += ']';
		} else if (isalpha(*iter) || *iter == '_' || *iter == '.') {
			// Key word.
			token = "";
			token += *iter;
			for (++iter; iter != src_code.end(); ++iter) {
				if (isalnum(*iter) || *iter == '_' || *iter == '.')
					token += *iter;
				else
					break;
			}
			map<string, int>::const_iterator map_iter = rplc.find(token);
			if (map_iter == rplc.end()) {
				// Not found, no need to replace.
				dst_code += token;
			} else {
				fmt.str("");
				fmt << map_iter->second;
				dst_code += "global[";
				dst_code += fmt.str();
				dst_code += ']';
			}
		} else {
			// Other case
			dst_code += *iter;
			++iter;
		}
	}
}

void compiler::write(const char *buf)
{
	size_t len = ::strlen(buf);
	if (::write(fd, buf, len) != len)
		throw bad_file(__FILE__, __LINE__, 124, bad_file::bad_write, file_name);
}

int compiler::new_task(const char *task)
{
	int status = 0;
	pid_t ret_code;
	struct sigaction act;
	struct sigaction oact;

	act.sa_handler = SIG_DFL;
	act.sa_flags = 0;
	::sigemptyset(&act.sa_mask);
	::sigaction(SIGCHLD, &act, &oact);

	pid_t pid = ::fork();
	switch (pid) {
	case 0:
		ret_code = ::execl("/bin/sh", "/bin/sh", "-c", task, (char *)0);
		::sigaction(SIGCHLD, &oact, NULL);
		::exit(ret_code);
		// NOTREACHED
	case -1:
		// fork error
		::sigaction(SIGCHLD, &oact, NULL);
		return -1;
		// NOTREACHED
	default:
		/*
		 * This 'while' effectively implements waitpid(), which I think
		 * should have used in the first place.
		 * Note that the block comments for this function state that if
		 * _GP_P_NOWAIT is specified, there is no wait. But here, the
		 * parent process is doing a wait() regardless of mode's value.
		 * The parent either waits (the _GP_P_NOWAIT case) for the child
		 * to do a fork() and exit(), or for the child to do the exec().
		 * At least now, with the addition of the while loop,
		 * the parent will be waiting for the correct child process,
		 * whereas before, the single wait() call was terminating with the
		 * death of a *previously* forked child (from unrelated code!),
		 * and this was mis-interpreted (in one CR094635 test scenario)
		 * as a failure to exec cleanupsrv.
		 */
		while ((ret_code = wait(&status)) != -1 && ret_code != pid) {
			/*
			 * Reset status, in case a later wait() call returns -1.
			 * (But note that there is no provision to properly
			 * report, using errno, this type of failure. At least
			 * this line of code keeps us from reporting a leftover
			 * status from a previous wait() call.)
			 */
			status = 0;
		}
		::sigaction(SIGCHLD, &oact, NULL);
		return status;
		// NOTREACHED
	}
}

}
}

