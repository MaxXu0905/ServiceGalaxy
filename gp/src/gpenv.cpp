#include "gpenv.h"
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/format.hpp>

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

using namespace std;

gpenv gpenv::_instance;
boost::mutex gpenv::env_mutex;

gpenv& gpenv::instance()
{
	return _instance;
}

/*
 * reads a file and inserts all lines
 *	beginning with a C identifier and having at least one '=' character
 *	into the local processes environment (via putenv(3)).
 *	This function returns 0 on success and -1 on failure.
 *	The only reasons for failure are an inaccessible file or an inability
 *	to allocate space.
 */
bool gpenv::read_env(const string& filename, const string& label)
{
	int found;
	string str;
	string cur_label;

	/*
	 * Lock the environment mutex for the
	 * duration of this routine.
	 */
	bi::scoped_lock<boost::mutex> lock(env_mutex);

	// Now open the file for reading.
	ifstream ifs(filename.c_str());
	if (!ifs)
		return true;

	if (label.empty())
		found = 1; // no section to find
	else
		found = 0; // need to find section matching input label

	while (true) {
		if (ifs.peek() == EOF)
			break;

		std::getline(ifs, str);
		if (str.length() == 0)
			continue;

		if (*str.rbegin() == '\r')
			str.resize(str.length() - 1);

		string::iterator iter = str.begin();
		string::iterator var_iter;

		// skip leading white space
		while (*iter == ' ' || *iter == '\t')
			iter++;

		if (*iter == '[') {
			// check for valid label
			iter++; // skip ]
			while (*iter == ' ' || *iter == '\t')
				iter++;
			var_iter = iter; // beginning of label name
			while (*iter && *iter != ']')
				iter++;
			if (*iter == ']') {
				// valid label
				// allow [] to indicate global section again
				// copy current label name
				cur_label = string(var_iter, iter);
			}
			continue;
		}

		if (!cur_label.empty()) {
			// not the global section
			if (label.empty() || cur_label != label)
				continue; // wrong section
			found = 1;
		}

		// first character must be alphabetic or underscore
		if (*iter != '_' && !isalpha(*iter))
			continue;

		var_iter = iter; // start of the variable=value
		for (iter++; iter != str.end(); iter++) {
			/*
			 * rest of chars must be alphanumerics or
			 * underscores up to the equals sign
			 */
			if (!isalnum(*iter) && *iter != '_')
				break;
		}
		if (*iter != '=')
			continue; // not valid format

		/*
		 * look for environment variables, allocate space,
		 * and expand any variables.
		 */
		string dst_str;
		expand_env(dst_str, str);
		if (dst_str.empty())
			return false; // error already printed


		::putenv(::strdup(dst_str.c_str()));
	}

	if (!found)
		GPP->write_log(__FILE__, __LINE__, (_("WARN: Label {1} not found in environment file") % label).str(SGLOCALE));

	return true;
}

void gpenv::expand_env(string& dst_str, const string& src_str)
{
	char *found_var;		/* The found environment variable */
	string::const_iterator iter;
	char next_char;

	dst_str = "";
	for (iter = src_str.begin(); iter != src_str.end(); ++iter) {
		if (*iter == '=')
			break;
		dst_str += *iter;
	}

	if (iter == src_str.end())
		return;

	dst_str += *iter++;

	while (iter != src_str.end()) {
		if (iter + 1 == src_str.end())
			next_char = '\0';
		else
			next_char = *(iter + 1);

		if (*iter == '\\') {
			if (next_char == '$' || next_char == '\\')
				++iter;
			dst_str += *iter++;
			continue;
		}

		if (*iter == '$' && next_char == '{') {
			/* Potential character replacement { */
			string::size_type pos = src_str.find('}', iter - src_str.begin() + 2);
			if (pos == string::npos) {
				/* No ending brace found */
				dst_str += *iter++;
				dst_str += *iter++;
				continue;
			}

			string::const_iterator end_iter = src_str.begin() + pos;

			// Found an end brace
			string lookup_var = string(iter + 2, end_iter);
			if ((found_var = ::getenv(lookup_var.c_str())) != NULL)
				dst_str += found_var;

			iter = end_iter + 1;
		} else {
			dst_str += *iter++;
		}
	}
}

/*
 * Process "-E app_name" if present in command line argument vector, and
 * modify argv so that any subsequent processing doesn't need to know
 * about -E.  The global app_name will hold the appname argument.
 * Return the new argument count.
 */
int gpenv::getargv_env(int argc, char **argv)
{
	int shift;
	int nargc;
	string app_name;

	nargc = argc; // keep track of new argument count

	for (argc--, argv++;argc > 0 && *argv != (char *)NULL; argc--, argv++) {
		if (argv[0][0] == '-') { // found -option
			if (argv[0][1] == '-') {
				/* found -- ; don't eat application options */
				return nargc;
			}

			if (argv[0][1] != 'E') {
				// didn't find -E - look at next option
				continue;
			}

			// found -E
			if (argv[0][2] != '\0') {
				// found  -E<name>
				app_name = &argv[0][2];
				shift = 1;
			}
			else if (argv[1] != (char *)NULL) {
				// found -E <name>
				app_name = argv[1];
				shift = 2;
			} else {
				/*
				 * no appname for -E; return original argument
				 * count
				 */
				return nargc;
			}

			// adjust argv and argc to skip over "-E"
			argc -= shift;
			nargc -= shift;
			/*
			 * shift rest of argv over to skip "-Eappname" or
			 * "-E appname"
			 */
			for (;argc > 0 && argv[shift] != NULL; argc--, argv++)
				argv[0] = argv[shift];
			break;
		}
	}

	/*
	 * call read_env to read in environment from default file, from the
	 * section for the application provided
	 */
	if (!app_name.empty())
		read_env(app_name);

	return nargc; // return adjusted argument count
}

void gpenv::expand_var(string& dst_str, const string& src_str)
{
	char *found_var;		/* The found environment variable */
	string::const_iterator iter;
	char next_char;

	dst_str = "";
	for (iter = src_str.begin(); iter != src_str.end();) {
		if (iter + 1 == src_str.end())
			next_char = '\0';
		else
			next_char = *(iter + 1);

		if (*iter == '\\') {
			if (next_char == '$' || next_char == '\\')
				++iter;
			dst_str += *iter++;
			continue;
		}

		if (*iter == '$' && next_char == '{') {
			/* Potential character replacement { */
			string::size_type pos = src_str.find('}', iter - src_str.begin() + 2);
			if (pos == string::npos) {
				/* No ending brace found */
				dst_str += *iter++;
				dst_str += *iter++;
				continue;
			}

			string::const_iterator end_iter = src_str.begin() + pos;

			// Found an end brace
			string lookup_var = string(iter + 2, end_iter);
			if ((found_var = ::getenv(lookup_var.c_str())) != NULL)
				dst_str += found_var;

			iter = end_iter + 1;
		} else {
			dst_str += *iter++;
		}
	}
}

void gpenv::putenv(const char *env)
{
	bi::scoped_lock<boost::mutex> lock(env_mutex);

	char *mycopy = new char[strlen(env) + 1];
	strcpy(mycopy, env);
	::putenv(mycopy);
	GPP->save_env(mycopy);
}

char * gpenv::getenv(const char *env)
{
	return ::getenv(env);
}

gpenv::gpenv()
{
	GPP = gpp_ctx::instance();
}

gpenv::~gpenv()
{
}

}
}

