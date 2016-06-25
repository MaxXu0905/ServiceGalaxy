/////////////////////////////////////////////////////////////////////////////////
// modification history
//-------------------------------------------------------------------------------
// add method
// datetime& datetime::operator=(const string& dts)
// datetime& datetime::operator=(const char* pdatestr)
// datetime& datetime::assign(const string& dts, const char* format)
//-------------------------------------------------------------------------------

#include <stdarg.h>
#include "common.h"
#include "dstream.h"
#include <boost/asio.hpp>
#include "sftp.h"

namespace ai
{
namespace sg
{

using namespace std;
namespace basio = boost::asio;

static int const KEY_LEN = 8;
static int const key[] = { 0x4A, 0x50, 0x4C, 0x53, 0x43, 0x57, 0x43, 0x44 };

void common::encrypt(const char* text, char* cipher)
{
    int len = strlen(text);
    int tmp;
    for (int i = 0; i < len; i++) {
        tmp = static_cast<int>(text[i]) ^ key[i % KEY_LEN];
        cipher[i * 2] = ((tmp >> 4) & 0x0f) + 'a';
        cipher[i * 2 + 1] = (tmp & 0x0f) + 'a';
    }
    cipher[len * 2] = '\0';
}

void common::decrypt(const char* cipher, char* text)
{
    int len = strlen(cipher) / 2;
    int tmp;
    for (int i = 0; i < len; i++) {
        tmp = (cipher[i * 2] - 'a') << 4;
        tmp |= cipher[i * 2 + 1] - 'a';
        text[i] = static_cast<char>(tmp ^ key[i % KEY_LEN]);
    }
    text[len] = '\0';
}

void common::string_to_array(const string& the_str, char delimiter, vector<string>& the_vector)
{
    string dst;
    the_vector.resize(0);
    for (int i = 0; i < the_str.length(); i++) {
        if (the_str[i] != delimiter) {
            if (the_str[i] == ' ')
                continue;
            dst += the_str[i];
        } else {
            if (!dst.empty()) {
                the_vector.push_back(dst);
                dst = "";
            }
        }
    }
    if (!dst.empty())
        the_vector.push_back(dst);
}

void common::string_to_full_array(const string& the_str, char delimiter, vector<string>& the_vector)
{
    string dst;
    the_vector.resize(0);
    for (int i = 0; i < the_str.length(); i++) {
        if (the_str[i] != delimiter) {
            dst += the_str[i];
        } else {
             the_vector.push_back(dst);
             dst = "";
        }
    }
    the_vector.push_back(dst);
}

void common::read_db_info(string& user_name, string& password, string& connect_string)
{
    ifstream fs(".passwd");
    if (!fs)
        throw bad_file(__FILE__, __LINE__, 89, bad_file::bad_open, (_("ERROR: Can't open file .passwd")).str(SGLOCALE));
    char ch;
    user_name = "";
    while (fs.get(ch)) {
        if (ch == '\n')
            break;
        else
            user_name += ch;
    }
    password = "";
    while (fs.get(ch)) {
        if (ch == '\n')
            break;
        else
            password += ch;
    }
    connect_string = "";
    while (fs.get(ch)) {
        if (ch == '\n')
            break;
        else
            connect_string += ch;
    }
}

void common::write_db_info(const string& user_name, const string& password, const string& connect_string)
{
    ofstream fs(".passwd");
    if (!fs)
        throw bad_file(__FILE__, __LINE__, 90, bad_file::bad_open, (_("ERROR: Can't open file .passwd")).str(SGLOCALE));
    fs << user_name << '\n';
    fs << password << '\n';
    fs << connect_string << '\n';
}

bool common::read_profile(const string& file_name, const string& section, const string& key, string& value)
{
    string text;
    ifstream fs(file_name.c_str());
    while (fs.good()) { // Find section.
        fs >> std::ws;
        std::getline(fs, text);
        if (text.length() == 0)
            continue;
        if (text[0] != '[' || text[text.length() -1] != ']')
            continue;
        size_t pos1 = 0;
        size_t pos2 = 0;
        for (int i = 1; i < text.length() - 1; i++) {
            if (text[i] != ' ') {
                if (pos1 == 0)
                    pos1 = i;
                pos2 = i + 1;
            }
        }
        if (section != string(text.begin() + pos1, text.begin() + pos2))
            continue;
        else
            break;
    }

    while (fs.good()) { // Find key.
        fs >> std::ws;
        std::getline(fs, text);
        if (text.length() == 0)
            continue;
        if (text[0] == '[') // Next section, so not found.
            return false;
        size_t pos1 = text.find_first_of('=', 0);
        if (pos1 == string::npos) // Not found.
            continue;
        size_t pos2 = text.find_first_of(' ', 0); // Max value if not found.
        if (key != string(text.begin(), text.begin() + std::min(pos1, pos2)))
            continue;
        pos1 = text.find_first_not_of(' ', pos1 + 1);
        pos2 = text.find_first_of(' ', pos1);
        if (pos2 == string::npos) // Not found.
            value.assign(text.begin() + pos1, text.end());
        else
            value.assign(text.begin() + pos1, text.begin() + pos2);
        return true;
    }

    return false;
}

void common::ltrim(char* the_str, const char* fill)
{
    char* ptr;
    for (ptr = the_str; *ptr; ptr++) {
        if (!strchr(fill, *ptr))
            break;
    }
    strcpy(the_str, ptr);
}

void common::ltrim(char* the_str, char ch)
{
    char* ptr;
    for (ptr = the_str; *ptr; ptr++) {
        if (*ptr != ch)
            break;
    }
    strcpy(the_str, ptr);
}

void common::rtrim(char* the_str, const char* fill)
{
    int i;
    int len = strlen(the_str);
    for (i = len - 1; i >= 0; i--) {
        if (!strchr(fill, the_str[i]))
            break;
    }
    the_str[i + 1] = '\0';
}

void common::rtrim(char* the_str, char ch)
{
    int i;
    int len = strlen(the_str);
    for (i = len - 1; i >= 0; i--) {
        if (the_str[i] != ch)
            break;
    }
    the_str[i + 1] = '\0';
}

void common::trim(char* the_str, const char* fill)
{
    rtrim(the_str, fill);
    ltrim(the_str, fill);
}

void common::trim(char* the_str, char ch)
{
    rtrim(the_str, ch);
    ltrim(the_str, ch);
}

void common::upper(char* the_str)
{
    for (char* ptr = the_str; *ptr; ptr++)
        *ptr = toupper(*ptr);
}

void common::upper(std::string& the_str)
{
    for (int i  = 0; i < the_str.size(); i ++)
        the_str[i] = toupper( the_str[i] );
}

void common::lower(char* the_str)
{
    for (char* ptr = the_str; *ptr; ptr++)
        *ptr = tolower(*ptr);
}

void common::lower(std::string& the_str)
{
    for (int i  = 0; i < the_str.size(); i ++)
        the_str[i] = tolower( the_str[i] );
}

int common::super_sscanf(const char *buf,const char *fmt,...)
{
    va_list arglist;
    int value_type = -1;
    int is_format;
    int is_token;

    va_start(arglist,fmt);
    is_format = 0;
    is_token = 0;

    while (1) {
        if (*fmt == '%') {
            is_format ++;
        } else if (*fmt == 's' || *fmt == 'S') {
            if (is_format == 0 || is_token == 1)
                goto _next_char;
            value_type = 0;
            is_token ++;
        } else if (*fmt == 'd' || *fmt == 'd' || *fmt == 'i' || *fmt == 'i') {
            if (is_format == 0 || is_token == 1)
                goto _next_char;
            is_token ++;
            value_type = 1;
        } else if (*fmt == 'c' || *fmt == 'C') {
            if (is_format == 0 || is_token == 1)
                goto _next_char;
            value_type = 2;
            is_token ++;
        } else {
            _next_char:
            if (*fmt != 0 && *fmt == *buf) {
                buf ++;
                fmt ++;
                continue;
            } else if (*fmt == 0x20) {
                fmt ++;
                continue;
            }

            if (is_format != 1 || is_token != 1) {
                return -1;
            }

            if (value_type == 0) {
                char *pstr = va_arg(arglist,char*);
                while (*buf != 0) {
                    if (*buf == '\n'||*buf == '\r') {
                        buf ++;
                        continue;
                    }
                    if (*buf == *fmt) {
                        break;
                    }
                    *pstr = *buf;
                    buf ++;
                    pstr ++;
                }
                is_format = is_token = 0;
                buf ++;
                *pstr = 0;
            } else if (value_type == 1) {
                int *num = va_arg(arglist,int*);
                *num = 0;
                while (*buf != 0) {
                    if (*buf == *fmt)
                        break;
                    if (*buf < '0' || *buf > '9') {
                        buf ++;
                        continue;
                    }
                    *num = *num * 10 + *buf - '0';
                    buf ++;
                }
                is_format = is_token = 0;
                buf ++;
            } else if (value_type == 2) {
                char *pch = va_arg(arglist,char*);
                *pch = 0;
                if (*buf != 0) {
                    *pch = *buf;
                }
                while (*buf != 0) {
                    if (*buf == *fmt)
                        break;
                    buf ++;
                }
                buf ++;
            }
            is_format = is_token = 0;
        }
        if (*fmt == 0 || *buf == 0)
            break;
        fmt ++;
    }
    return 0;
}

void common::round(int mode, int precision, double& value)
{
    int const factor[] =
    { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

    assert(precision >= 0 && precision < 10);
    // 费用计算完成后进行取整
    double epsilon;
    if (value >= 0.0)
        epsilon = 1e-10;
    else
        epsilon = -1e-10;
    switch (mode) {
    case FLOOR:
        value = ::floor(value * factor[precision] + epsilon) / factor[precision];
        break;
    case ROUND:
#ifdef TRUE64
        value = ::nint(value * factor[precision] + epsilon) / factor[precision];
#else
        value = ::round(value * factor[precision] + epsilon) / factor[precision];
#endif
        break;
    case CEIL:
        value = ::ceil(value * factor[precision] - epsilon) / factor[precision];
        break;
    default:
        assert(0);
        break;
    }
}

char * common::ceil(char *value, long base)
{
	return reinterpret_cast<char *>(reinterpret_cast<long>(value + base - 1) & ~(base - 1));
}

void common::get_ip(string& ip)
{
    char hostname[32];
    gethostname(hostname, 32);
    hostent* ent = gethostbyname(hostname);
    ostringstream fmt;
    fmt << (ent->h_addr_list[0][0] & 0xff);
    for (int i = 1; i < ent->h_length; i++)
        fmt << '.' << (ent->h_addr_list[0][i] & 0xff);
    ip = fmt.str();
}

bool common::mkdir(void *sftp, const string& path)
{
	string str = path;
	string part;
	boost::char_separator<char> sep("/");
	tokenizer tokens(str, sep);

	if (sftp == NULL) {
		for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter) {
			part += "/";
			part += *iter;

			if (::mkdir(part.c_str(), 0755) == -1) {
				if (errno != EEXIST)
					return false;
			}
		}
	} else {
		LIBSSH2_SFTP *sftp_session = reinterpret_cast<LIBSSH2_SFTP *>(sftp);

		for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter) {
			part += "/";
			part += *iter;

			if (libssh2_sftp_mkdir(sftp_session, part.c_str(), 0755) != 0) {
				int last_error = libssh2_sftp_last_error(sftp_session);
				if (last_error != LIBSSH2_FX_FAILURE && last_error != LIBSSH2_FX_FILE_ALREADY_EXISTS)
					return false;
			}
		}
	}

	return true;
}

bool common::rename(const char* src, const char* dst)
{
    struct stat src_stat;
    struct stat dst_stat;
    char dst_tmp[PATH_MAX];
    if (::stat(src, &src_stat) < 0)
        return false;
    int i;
    for (i = strlen(dst) - 1; i >= 0; i--) {
        if (dst[i] == '/')
            break;
    }
    memcpy(dst_tmp, dst, i + 1);
    dst_tmp[i + 1] = '\0';
    if (::stat(dst_tmp, &dst_stat) < 0)
        return false;
    if (src_stat.st_dev == dst_stat.st_dev) { // 同一个文件系统
        if (::rename(src, dst) != -1)
            return true;
        return false;
    } else { // 不同文件系统
        string cmd = "mv ";
        cmd += src;
        cmd += " ";
        cmd += dst;

    	int status = ::system(cmd.c_str());
#if defined(WIFEXITED) && defined(WEXITSTATUS)
    	if (WIFEXITED(status))    /* non-zero if status for normal termination */
        	return (WEXITSTATUS(status) == 0);
    	else	/* error occurred */
        	return false;
#else
    	return (status == 0);
#endif
    }
}

bool common::rename(void *sftp, const char *src, const char *dst)
{
    if (sftp == NULL) {
        return rename(src, dst);
    } else {
        int retval = libssh2_sftp_rename(reinterpret_cast<LIBSSH2_SFTP *>(sftp), src, dst);
        if (retval == LIBSSH2_ERROR_SFTP_PROTOCOL) {
            libssh2_sftp_unlink(reinterpret_cast<LIBSSH2_SFTP *>(sftp), dst);
            retval = libssh2_sftp_rename(reinterpret_cast<LIBSSH2_SFTP *>(sftp), src, dst);
        }

        return (retval == 0);
    }
}

bool common::remove(void *sftp, const char *src)
{
	if (sftp == NULL)
		return (::remove(src) != -1);
	else
		return (libssh2_sftp_unlink(reinterpret_cast<LIBSSH2_SFTP *>(sftp), src) != 0);
}

istream * common::create_istream(stream_type_enum stream_type, const string& prefix, const string& filename)
{
	switch (stream_type) {
	case STREAM_TYPE_IFSTREAM:
		return new ifstream(filename.c_str());
	case STREAM_TYPE_ISFTPSTREAM:
		return new isftpstream(prefix, filename);
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Invalid stream_type {1}") % stream_type).str(SGLOCALE));
	}
}

istream * common::create_istream(stream_type_enum stream_type, const string& prefix, const string& filename, ios_base::openmode mode)
{
	switch (stream_type) {
	case STREAM_TYPE_IFSTREAM:
		return new ifstream(filename.c_str(), mode);
	case STREAM_TYPE_ISFTPSTREAM:
		return new isftpstream(prefix, filename, mode);
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Invalid stream_type {1}") % stream_type).str(SGLOCALE));
	}
}

ostream * common::create_ostream(stream_type_enum stream_type, const string& prefix, const string& filename)
{
	switch (stream_type) {
	case STREAM_TYPE_OFSTREAM:
		return new ofstream(filename.c_str());
	case STREAM_TYPE_OSFTPSTREAM:
		return new osftpstream(prefix, filename);
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Invalid stream_type {1}") % stream_type).str(SGLOCALE));
	}
}

ostream * common::create_ostream(stream_type_enum stream_type, const string& prefix, const string& filename, ios_base::openmode mode)
{
	switch (stream_type) {
	case STREAM_TYPE_OFSTREAM:
		return new ofstream(filename.c_str(), mode);
	case STREAM_TYPE_OSFTPSTREAM:
		return new osftpstream(prefix, filename, mode);
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Invalid stream_type {1}") % stream_type).str(SGLOCALE));
	}
}

iostream * common::create_iostream(stream_type_enum stream_type, const string& prefix, const string& filename)
{
	switch (stream_type) {
	case STREAM_TYPE_FSTREAM:
		return new fstream(filename.c_str());
	case STREAM_TYPE_SFTPSTREAM:
		return new sftpstream(prefix, filename);
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Invalid stream_type {1}") % stream_type).str(SGLOCALE));
	}
}

iostream * common::create_iostream(stream_type_enum stream_type, const string& prefix, const string& filename, ios_base::openmode mode)
{
	switch (stream_type) {
	case STREAM_TYPE_FSTREAM:
		return new fstream(filename.c_str(), mode);
	case STREAM_TYPE_SFTPSTREAM:
		return new sftpstream(prefix, filename, mode);
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Invalid stream_type {1}") % stream_type).str(SGLOCALE));
	}
}

bool common::is_open(void *stream, stream_type_enum stream_type)
{
	switch (stream_type) {
	case STREAM_TYPE_IFSTREAM:
		return reinterpret_cast<ifstream *>(stream)->is_open();
	case STREAM_TYPE_OFSTREAM:
		return reinterpret_cast<ofstream *>(stream)->is_open();
	case STREAM_TYPE_FSTREAM:
		return reinterpret_cast<fstream *>(stream)->is_open();
	case STREAM_TYPE_ISFTPSTREAM:
		return reinterpret_cast<isftpstream *>(stream)->is_open();
	case STREAM_TYPE_OSFTPSTREAM:
		return reinterpret_cast<osftpstream *>(stream)->is_open();
	case STREAM_TYPE_SFTPSTREAM:
		return reinterpret_cast<sftpstream *>(stream)->is_open();
	default:
		return false;
	}
}

void common::close(void *stream, stream_type_enum stream_type)
{
	switch (stream_type) {
	case STREAM_TYPE_IFSTREAM:
		reinterpret_cast<ifstream *>(stream)->close();
		break;
	case STREAM_TYPE_OFSTREAM:
		reinterpret_cast<ofstream *>(stream)->close();
		break;
	case STREAM_TYPE_FSTREAM:
		reinterpret_cast<fstream *>(stream)->close();
		break;
	case STREAM_TYPE_ISFTPSTREAM:
		reinterpret_cast<isftpstream *>(stream)->close();
		break;
	case STREAM_TYPE_OSFTPSTREAM:
		reinterpret_cast<osftpstream *>(stream)->close();
		break;
	case STREAM_TYPE_SFTPSTREAM:
		reinterpret_cast<sftpstream *>(stream)->close();
		break;
	}
}

bool common::is_local(const string& sftp_prefix)
{
	string::size_type pos = sftp_prefix.find("@");
	if (pos == string::npos)
		return false;

	string usrname(sftp_prefix.begin(), sftp_prefix.begin() + pos);
	string hostaddr(sftp_prefix.begin() + pos + 1, sftp_prefix.end());

	const char *ptr = getenv("LOGNAME");
	if (ptr == NULL)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: environment variable LOGNAME not set")).str(SGLOCALE));

	if (usrname != ptr)
		return false;

	string host_name = basio::ip::host_name();
	if (strcasecmp(host_name.c_str(), hostaddr.c_str()) == 0)
		return true;

	string ipaddr;
	get_ip(ipaddr);

	if (ipaddr == hostaddr)
		return true;

	return false;
}

bool common::parse(string& dir_name, string& sftp_prefix)
{
	if (dir_name.length() > 7 && memcmp(dir_name.c_str(), "sftp://", 7) == 0) {
		std::vector<std::string> token_vector;
		boost::char_separator<char> sep(":@");
		string subpath = dir_name.substr(7, dir_name.length() - 7);
		tokenizer tokens(subpath, sep);

		for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
			token_vector.push_back(*iter);

		if (token_vector.size() == 2) {
			// host:directory
			const char *ptr = getenv("LOGNAME");
			if (ptr == NULL)
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: environment variable LOGNAME not set")).str(SGLOCALE));

			sftp_prefix = ptr;
			sftp_prefix += "@";
			sftp_prefix += token_vector[0];
			dir_name = token_vector[1];
		} else if (token_vector.size() == 3) {
			// usrname@host:directory
			sftp_prefix = token_vector[0];
			sftp_prefix += "@";
			sftp_prefix += token_vector[1];
			dir_name = token_vector[2];
		} else {
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Invalid dir_name {1} given") % dir_name).str(SGLOCALE));
		}

		return true;
	}

	return false;
}

bool common::is_prog_run()
{
	pid_t pid = getpid();
	uid_t uid = getuid();
	uid_t euid = geteuid();

#if defined(AIX)
	DIR* dirp;
	dirent ent;
	dirent* result;
	char full_name[1024];
	int fd;
	char proc_name[1024];
	struct stat statbuf;
	size_t i;

	sprintf(full_name, "/proc/%ld/psinfo", (long)pid);
	if (stat(full_name, &statbuf) != -1 && (uid == statbuf.st_uid || euid == statbuf.st_uid)) {
		fd = open(full_name, O_RDONLY);
		if (fd == -1)
			return false;
		lseek(fd, 184, SEEK_SET);
		read(fd, proc_name, sizeof(proc_name));
		close(fd);
	} else {
		return false;
	}

	vector<string> tmp_array;
	int ch;
	common::string_to_array(proc_name, ' ', tmp_array);
	char* self_argv[256];
	for (i = 0; i < tmp_array.size(); i++)
		self_argv[i] = const_cast<char*>(tmp_array[i].c_str());

	char sz_self_proc_name[512];
	strcpy(sz_self_proc_name, tmp_array[0].c_str());
	string self_proc_name = basename(sz_self_proc_name);
	string self_host_id;
	string self_proc_id;
	vector<string> self_threads;
	optind = 0;
	while ((ch = getopt(tmp_array.size(), self_argv, ":h:p:t:")) != -1) {
		switch (ch) {
		case 'h':
			self_host_id = optarg;
			break;
		case 'p':
			self_proc_id = optarg;
			break;
		case 't':
			self_threads.push_back(optarg);
			break;
		}
	}

	dirp = opendir("/proc");
	if (!dirp)
		return false;

	while (readdir_r(dirp, &ent, &result) == 0 && result != 0) {
		if (!strcmp(ent.d_name, ".") || !strcmp(ent.d_name, "..")
			|| atol(ent.d_name) == 0 || atol(ent.d_name) == pid)
			continue;
		sprintf(full_name, "/proc/%s/psinfo", ent.d_name);

		if (stat(full_name, &statbuf) != -1 && (uid == statbuf.st_uid || euid == statbuf.st_uid)) {
			fd = open(full_name,O_RDONLY);
			if (fd == -1)
				continue;
			lseek(fd, 184, SEEK_SET);
			read(fd, proc_name, 1024);
			close(fd);

			common::string_to_array(proc_name, ' ', tmp_array);
			char* other_argv[256];
			for (i = 0; i < tmp_array.size(); i++)
				other_argv[i] = const_cast<char*>(tmp_array[i].c_str());
			char sz_other_proc_name[512];
			strcpy(sz_other_proc_name, tmp_array[0].c_str());
			string other_proc_name = basename(sz_other_proc_name);
			string other_host_id;
			string other_proc_id;
			vector<string> other_threads;
			optind = 0;
			while ((ch = getopt(tmp_array.size(), other_argv, ":h:p:t:")) != -1) {
				switch (ch) {
				case 'h':
					other_host_id = optarg;
					break;
				case 'p':
					other_proc_id = optarg;
					break;
				case 't':
					other_threads.push_back(optarg);
					break;
				}
			}
			if (self_proc_name != other_proc_name || self_host_id != other_host_id
				|| self_proc_id != other_proc_id)
				continue;
			if (self_threads.size() == 0 || other_threads.size() == 0)
				return true;
			for (vector<string>::const_iterator iter = self_threads.begin(); iter != self_threads.end(); ++iter) {
				if (std::find(other_threads.begin(), other_threads.end(), *iter) != other_threads.end())
					return true;
			}
		}
	}

	closedir(dirp);
	return false;

#elif defined(HPUX)

	pst_status proc_status;
	vector<string> self_argvs;
	vector<string> proc_argvs;

	if (pstat_getproc(&proc_status, sizeof(struct pst_status), 0, pid) == 1) {
		string_to_array(proc_status.pst_cmd, ' ', self_argvs);
		if (self_argvs.size() < 7) {
			return false;
		}
	} else {
		return false;
	}

	proc_status.pst_idx = 0;
	while (1) {
		int nret;
		nret = pstat_getproc(&proc_status, sizeof(struct pst_status), 1, proc_status.pst_idx);
		proc_status.pst_idx++;
		if (nret == 1) {
			if (uid != proc_status.pst_uid && euid != proc_status.pst_uid)
				continue;

			proc_argvs.clear();
			string_to_array(proc_status.pst_cmd, ' ', proc_argvs);
			if (proc_argvs.size() >= 7) {
				int i;
				for ( i = 0; i < 7; i++) {
					if (self_argvs[i] != proc_argvs[i])
						break;
				}
				if (i >= 7 && proc_status.pst_pid != pid) {
					return true;
				}
			}
		} else if (errno == EOVERFLOW) {// if match 64bits process and this program was compiled under 32bits mode this error would occur
			continue;
		} else
			break;
	}
	return false;

#elif defined(TRUE64)

	DIR* dirp;
	dirent ent;
	dirent* result;
	char full_name[1024];
	int fd;
	char proc_name[1024];
	struct prpsinfo psinfo;
	char arglist[PSARGS_SZ + 1];
	struct stat statbuf;
	size_t i;

	sprintf(full_name, "/proc/%ld", (long)pid);
	if (stat(full_name, &statbuf) != -1 && (uid == statbuf.st_uid || euid == statbuf.st_uid)) {
		fd = open(full_name, O_RDONLY);
		if (fd == -1)
			return false;
		if (ioctl(fd, PIOCPSINFO, &psinfo) < 0) {
			close(fd);
			return false;
		}
		close(fd);
	} else {
		return false;
	}

	vector<string> tmp_array;
	int ch;
	memcpy(arglist, psinfo.pr_psargs, PSARGS_SZ);
	arglist[PSARGS_SZ] = 0;
	for (i = 0; i < PSARGS_SZ; i++) {
		arglist[i] = (arglist[i] == 0)?' ':arglist[i];
	}
	common::string_to_array(arglist, ' ', tmp_array);
	char* self_argv[256];
	for (int i = 0; i < tmp_array.size(); i++)
		self_argv[i] = const_cast<char*>(tmp_array[i].c_str());

	char sz_self_proc_name[512];
	strcpy(sz_self_proc_name, tmp_array[0].c_str());
	string self_proc_name = basename(sz_self_proc_name);
	string self_host_id;
	string self_proc_id;
	vector<string> self_threads;
	optind = 0;
	while ((ch = getopt(tmp_array.size(), self_argv, ":h:p:t:")) != -1) {
		switch (ch) {
		case 'h':
			self_host_id = optarg;
			break;
		case 'p':
			self_proc_id = optarg;
			break;
		case 't':
			self_threads.push_back(optarg);
			break;
		}
	}

	dirp = opendir("/proc");
	if (!dirp)
		return false;
	while (readdir_r(dirp, &ent, &result) == 0 && result != 0) {
		if (!strcmp(ent.d_name, ".") || !strcmp(ent.d_name, "..")
			|| atol(ent.d_name) == 0 || atol(ent.d_name) == pid)
			continue;
		sprintf(full_name, "/proc/%s", ent.d_name);
		if (stat(full_name, &statbuf) != -1 && (uid == statbuf.st_uid || euid == statbuf.st_uid)) {
			fd = open(full_name,O_RDONLY);
			if (fd == -1)
				continue;
			if (ioctl(fd, PIOCPSINFO, &psinfo) < 0) {
				close(fd);
				continue;
			}
			close(fd);

			memcpy(arglist, psinfo.pr_psargs, PSARGS_SZ);
			arglist[PSARGS_SZ] = 0;
			for (i = 0; i < PSARGS_SZ; i ++) {
				arglist[i] = (arglist[i] == 0)?' ':arglist[i];
			}
			common::string_to_array(arglist, ' ', tmp_array);
			char* other_argv[256];
			for (int i = 0; i < tmp_array.size(); i++)
				other_argv[i] = const_cast<char*>(tmp_array[i].c_str());
			char sz_other_proc_name[512];
			strcpy(sz_other_proc_name, tmp_array[0].c_str());
			string other_proc_name = basename(sz_other_proc_name);
			string other_host_id;
			string other_proc_id;
			vector<string> other_threads;
			optind = 0;
			while ((ch = getopt(tmp_array.size(), other_argv, ":h:p:t:")) != -1) {
				switch (ch) {
				case 'h':
					other_host_id = optarg;
					break;
				case 'p':
					other_proc_id = optarg;
					break;
				case 't':
					other_threads.push_back(optarg);
					break;
				}
			}

			if (self_proc_name != other_proc_name || self_host_id != other_host_id
				self_proc_id != other_proc_id)
				continue;
			if (self_threads.size() == 0 || other_threads.size() == 0)
				return true;
			for (vector<string>::const_iterator iter = self_threads.begin(); iter != self_threads.end(); ++iter) {
				if (std::find(other_threads.begin(), other_threads.end(), *iter) != other_threads.end())
					return true;
			}
		}
	}
	closedir(dirp);
	return false;

#elif defined(LINUX)

	DIR* dirp;
	dirent ent;
	dirent* result;
	char full_name[1024];
	int fd;
	char proc_name[1024];
	struct stat statbuf;
	size_t len;
	size_t i;

	sprintf(full_name, "/proc/%ld/cmdline", (long)pid);
	if (stat(full_name, &statbuf) != -1 && (uid == statbuf.st_uid || euid == statbuf.st_uid)) {
		fd = open(full_name, O_RDONLY);
		if (fd == -1)
			return false;
		len = read(fd, proc_name, sizeof(proc_name));
		for (i = 0; i < len - 1; i++) {
			if (proc_name[i] == '\0')
				proc_name[i] = ' ';
		}
		::close(fd);
	} else {
		return false;
	}

	vector<string> tmp_array;
	int ch;
	if (proc_name[0] == '-')
		common::string_to_array(proc_name + 1, ' ', tmp_array);
	else
		common::string_to_array(proc_name, ' ', tmp_array);
	char* self_argv[256];
	for (i = 0; i < tmp_array.size(); i++)
		self_argv[i] = const_cast<char*>(tmp_array[i].c_str());

	char sz_self_proc_name[512];
	strcpy(sz_self_proc_name, tmp_array[0].c_str());
	string self_proc_name = basename(sz_self_proc_name);
	string self_host_id;
	string self_proc_id;
	vector<string> self_threads;
	optind = 0;
	while ((ch = getopt(tmp_array.size(), self_argv, ":h:p:t:")) != -1) {
		switch (ch) {
		case 'h':
			self_host_id = optarg;
			break;
		case 'p':
			self_proc_id = optarg;
			break;
		case 't':
			self_threads.push_back(optarg);
			break;
		}
	}

	dirp = opendir("/proc");
	if (!dirp)
		return false;

	while (readdir_r(dirp, &ent, &result) == 0 && result != 0) {
		if (!strcmp(ent.d_name, ".") || !strcmp(ent.d_name, "..")
			|| atol(ent.d_name) == 0 || atol(ent.d_name) == pid)
			continue;
		sprintf(full_name, "/proc/%s/cmdline", ent.d_name);

		if (stat(full_name, &statbuf) != -1 && (uid == statbuf.st_uid || euid == statbuf.st_uid)) {
			fd = open(full_name,O_RDONLY);
			if (fd == -1)
				continue;
			len = read(fd, proc_name, sizeof(proc_name));
			if (len <= 0) {
				::close(fd);
				continue;
			}
			for (i = 0; i < len - 1; i++) {
				if (proc_name[i] == '\0')
					proc_name[i] = ' ';
			}
			::close(fd);

			if (proc_name[0] == '-')
				common::string_to_array(proc_name + 1, ' ', tmp_array);
			else
				common::string_to_array(proc_name, ' ', tmp_array);

			char* other_argv[256];
			for (i = 0; i < tmp_array.size(); i++)
				other_argv[i] = const_cast<char*>(tmp_array[i].c_str());
			char sz_other_proc_name[512];
			strcpy(sz_other_proc_name, tmp_array[0].c_str());
			string other_proc_name = basename(sz_other_proc_name);
			string other_host_id;
			string other_proc_id;
			vector<string> other_threads;
			optind = 0;
			while ((ch = getopt(tmp_array.size(), other_argv, ":h:p:t:")) != -1) {
				switch (ch) {
				case 'h':
					other_host_id = optarg;
					break;
				case 'p':
					other_proc_id = optarg;
					break;
				case 't':
					other_threads.push_back(optarg);
					break;
				}
			}
			if (self_proc_name != other_proc_name || self_host_id != other_host_id
				|| self_proc_id != other_proc_id)
				continue;
			if (self_threads.size() == 0 || other_threads.size() == 0)
				return true;
			for (vector<string>::const_iterator iter = self_threads.begin(); iter != self_threads.end(); ++iter) {
				if (std::find(other_threads.begin(), other_threads.end(), *iter) != other_threads.end())
					return true;
			}
		}
	}
	closedir(dirp);
	return false;

#else

	return false;

#endif

}

//计算两个数的最大公约数(greatest common divisor)
int common::grt_com_div(int a ,int b)
{
    int tmp;
    if (a < b) {
        tmp = a;
        a = b;
        b = tmp;
    }

    if (a%b == 0)
        return b;

    return grt_com_div(b,a%b);
}


//计算多个数的最大公约数
int common::grt_com_div(vector<int>& numbers)
{
    vector<int>::iterator it;

    it = numbers.begin();
    int lcm = *it;

    for (it++; it != numbers.end();it++)
        lcm = grt_com_div(lcm,*it);

    return lcm;
}

//计算两个数的最小公倍数
int common::ls_com_mul(int a, int b)
{
    return a*b/grt_com_div(a,b);
}

//计算多个数的最小公倍数(lease common multiple)
int common::ls_com_mul(vector<int>& numbers)
{
    vector<int>::iterator it;
    it = numbers.begin();
    int lcm = *it;

    for (it++; it!= numbers.end();it++)
        lcm = ls_com_mul(lcm,*it);

    return lcm;
}


calendar::calendar(time_t dd)
{
    // Call tzset and we can get correct timezone. Care that it's a negtive integer.
    tzset();
    time_zone = -timezone;
    duration_ = dd;
}

calendar& calendar::operator=(time_t dd)
{
    duration_ = dd;
    return *this;
}

calendar& calendar::operator+=(const calendar& rhs)
{
    duration_ += rhs.duration_;
    return *this;
}

calendar& calendar::operator+=(time_t dd)
{
    duration_ += dd;
    return *this;
}

calendar& calendar::operator-=(const calendar& rhs)
{
    duration_ -= rhs.duration_;
    return *this;
}

calendar& calendar::operator-=(time_t dd)
{
    duration_ -= dd;
    return *this;
}

time_t calendar::operator-(const calendar& rhs) const
{
    return duration_ - rhs.duration_;
}

bool calendar::operator>(const calendar& rhs) const
{
    return duration_ > rhs.duration_;
}

bool calendar::operator>=(const calendar& rhs) const
{
    return duration_ >= rhs.duration_;
}

bool calendar::operator<(const calendar& rhs) const
{
    return duration_ < rhs.duration_;
}

bool calendar::operator<=(const calendar& rhs) const
{
    return duration_ <= rhs.duration_;
}

bool calendar::operator==(const calendar& rhs) const
{
    return duration_ == rhs.duration_;
}

time_t calendar::duration() const
{
    return duration_;
}

void calendar::check_date(int y, int m, int d)
{
    int const days[2][13] =
    {
        { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
    };
    int idx;
    // Check year.
    if (y < 1900)
        throw bad_datetime(__FILE__, __LINE__, 93, bad_datetime::bad_year);
    // Check month.
    if (m < 1 || m > 12)
        throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month);
    // Check day.
    if (y % 400 == 0)
        idx = 1;
    else if (y % 100 == 0)
        idx = 0;
    else if (y % 4 == 0)
        idx = 1;
    else
        idx = 0;
    if (d < 1 || d > days[idx][m])
        throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day);
}

void calendar::check_time(int h, int m, int s)
{
    if (h < 0 || h > 23)
        throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour);
    if (m < 0 || m > 59)
        throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute);
    if (s < 0 || s > 59)
        throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second);
}

int calendar::last_day() const
{
    tm tm;
    localtime_r(&duration_, &tm);
    tm.tm_mon += 1;
    tm.tm_mday = 0;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    mktime(&tm);
    return tm.tm_mday;
}

date::date(int y, int m, int d)
{
    check_date(y, m, d);
    tm tm;
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = 0;
    duration_ = mktime(&tm);
}

date::date(const string& ds)
{
    tm tm;
    if (ds.length() == 8) {
        if (!(isdigit(ds[0]) && isdigit(ds[1]) && isdigit(ds[2]) && isdigit(ds[3])))
            throw bad_datetime(__FILE__, __LINE__, 93, bad_datetime::bad_year, ds);
        if (!(isdigit(ds[4]) && isdigit(ds[5])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, ds);
        if (!(isdigit(ds[6]) && isdigit(ds[7])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, ds);
        tm.tm_year = ds[0] * 1000 + ds[1] * 100 + ds[2] * 10 + ds[3] - '0' * 1111;
        tm.tm_mon = ds[4] * 10 + ds[5] - '0' * 11;
        tm.tm_mday = ds[6] * 10 + ds[7] - '0' * 11;
    } else if (ds.length() == 10) {
        if (!((ds[4] == ':' && ds[7] == ':') || (ds[4] == '-' && ds[7] == '-')))
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, ds);
        if (!(isdigit(ds[0]) && isdigit(ds[1]) && isdigit(ds[2]) && isdigit(ds[3])))
            throw bad_datetime(__FILE__, __LINE__, 93, bad_datetime::bad_year, ds);
        if (!(isdigit(ds[5]) && isdigit(ds[6])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, ds);
        if (!(isdigit(ds[8]) && isdigit(ds[9])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, ds);
        tm.tm_year = ds[0] * 1000 + ds[1] * 100 + ds[2] * 10 + ds[3] - '0' * 1111;
        tm.tm_mon = ds[5] * 10 + ds[6] - '0' * 11;
        tm.tm_mday = ds[8] * 10 + ds[9] - '0' * 11;
    } else {
        throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, ds);
    }
    check_date(tm.tm_year, tm.tm_mon, tm.tm_mday);
    tm.tm_year -= 1900;
    tm.tm_mon--;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = 0;
    duration_ = mktime(&tm);
}

date::date(const date& d)
: calendar(d.duration_)
{
}

date::date(time_t dd)
: calendar(dd)
{
}

date& date::operator=(const date& d)
{
    this->duration_ = d.duration_;
    return *this;
}

date& date::operator=(time_t dd)
{
    this->duration_ = dd;
    return *this;
}

int date::year() const
{
    tm tm;
    localtime_r(&duration_, &tm);
    return tm.tm_year + 1900;
}

int date::month() const
{
    tm tm;
    localtime_r(&duration_, &tm);
    return tm.tm_mon + 1;
}

int date::day() const
{
    tm tm;
    localtime_r(&duration_, &tm);
    return tm.tm_mday;
}

int date::week() const
{
    // 19700101 is Thursday.
    return((duration_ + time_zone) / 86400 + 3) % 7 + 1;
}

void date::next_month(int n)
{
    tm tm;
    localtime_r(&duration_, &tm);
    tm.tm_mon += n;
    tm.tm_mday = 1;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    duration_ = mktime(&tm);
}

void date::add_months(int n)
{
    int const days[2][12] =
    {
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
    };
    tm tm1,tm2;
    time_t t;
    int idx,y,m;
    bool flag = false;

    localtime_r(&duration_, &tm1);

    y = tm1.tm_year;
    m = tm1.tm_mon;

    if (y % 400 == 0)
        idx = 1;
    else if (y % 100 == 0)
        idx = 0;
    else if (y % 4 == 0)
        idx = 1;
    else
        idx = 0;

    flag = (this->day() == days[idx][m]);

    tm1.tm_mon += n;
    tm1.tm_mday = 1;
    t = mktime(&tm1);
    localtime_r(&t,&tm2);

    y = tm2.tm_year;
    m = tm2.tm_mon;

    if (y % 400 == 0)
        idx = 1;
    else if (y % 100 == 0)
        idx = 0;
    else if (y % 4 == 0)
        idx = 1;
    else
        idx = 0;

    if (flag || (this->day() > days[idx][tm2.tm_mon]))
        tm2.tm_mday = days[idx][tm2.tm_mon];
    else
        tm2.tm_mday = this->day();
    duration_ = mktime(&tm2);
}

double date::months_between(const date& rhs)
{
    tm tm_lhs, tm_rhs;
    time_t t;
    int y, m;
    double d;

    localtime_r(&duration_, &tm_lhs);
    t = rhs.duration();
    localtime_r(&t, &tm_rhs);

    y = tm_lhs.tm_year - tm_rhs.tm_year;
    m = tm_lhs.tm_mon - tm_rhs.tm_mon;
    d = (tm_lhs.tm_mday - tm_rhs.tm_mday) / 31.0;

    return y * 12 + m + d;
}

void date::iso_string(string& ds) const
{
    tm tm;
    ostringstream fmt;
    localtime_r(&duration_, &tm);
    fmt << std::setfill('0') << std::setw(4) << tm.tm_year + 1900
    << std::setw(2) << tm.tm_mon + 1
    << std::setw(2) << tm.tm_mday;
    ds = fmt.str();
}

void date::iso_extended_string(string& ds) const
{
    tm tm;
    ostringstream fmt;
    localtime_r(&duration_, &tm);
    fmt << std::setfill('0') << std::setw(4) << tm.tm_year + 1900
    << '-' << std::setw(2) << tm.tm_mon + 1
    << '-' << std::setw(2) << tm.tm_mday;
    ds = fmt.str();
}

ptime::ptime(int h, int m, int s)
{
    check_time(h, m, s);
    duration_ = h * 3600 + m * 60 + s;
}

ptime::ptime(const string& ts)
{
    int h;
    int m;
    int s;
    if (ts.length() == 6) {
        if (!(isdigit(ts[0]) && isdigit(ts[1])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, ts);
        if (!(isdigit(ts[2]) && isdigit(ts[3])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, ts);
        if (!(isdigit(ts[4]) && isdigit(ts[5])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, ts);
        h = ts[0] * 10 + ts[1] - '0' * 11;
        m = ts[2] * 10 + ts[3] - '0' * 11;
        s = ts[4] * 10 + ts[5] - '0' *11;
    } else if (ts.length() == 8) {
        if (!((ts[2] == ':' && ts[5] == ':') || (ts[2] == '-' && ts[5] == '-')))
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, ts);
        if (!(isdigit(ts[0]) && isdigit(ts[1])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, ts);
        if (!(isdigit(ts[3]) && isdigit(ts[4])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, ts);
        if (!(isdigit(ts[6]) && isdigit(ts[7])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, ts);
        h = ts[0] * 10 + ts[1] - '0' * 11;
        m = ts[3] * 10 + ts[4] - '0' * 11;
        s = ts[6] * 10 + ts[7] - '0' *11;
    } else {
        throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, ts);
    }

    check_time(h, m, s);
    duration_ = h * 3600 + m * 60 + s;
}

ptime::ptime(const ptime& t)
: calendar(t.duration_)
{
}

ptime::ptime(time_t dd)
: calendar(dd)
{
}

ptime& ptime::operator=(const ptime& t)
{
    duration_ = t.duration_;
    return *this;
}

ptime& ptime::operator=(time_t dd)
{
    this->duration_ = dd;
    return *this;
}

int ptime::hour() const
{
    return duration_ / 3600;
}

int ptime::minute() const
{
    return(duration_ % 3600) / 60;
}

int ptime::second() const
{
    return duration_ % 60;
}

void ptime::iso_string(string& ts) const
{
    ostringstream fmt;
    fmt << std::setfill('0') << std::setw(2) << hour()
    << std::setw(2) << minute()
    << std::setw(2) << second();
    ts = fmt.str();
}

void ptime::iso_extended_string(string& ts) const
{
    ostringstream fmt;
    fmt << std::setfill('0') << std::setw(2) << hour()
    << '-' << std::setw(2) << minute()
    << '-' << std::setw(2) << second();
    ts = fmt.str();
}

datetime::datetime(int y, int m, int d, int h, int mi, int s)
{
    check_date(y, m, d);
    check_time(h, mi, s);
    tm tm;
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = s;
    tm.tm_isdst = 0;
    duration_ = mktime(&tm);
}

datetime::datetime(const string& dts)
{
    int h;
    int m;
    int s;
    tm tm;
    if (dts.length() == 14) {
        if (!(isdigit(dts[0]) && isdigit(dts[1]) && isdigit(dts[2]) && isdigit(dts[3])))
            throw bad_datetime(__FILE__, __LINE__, 93,bad_datetime::bad_year, dts);
        if (!(isdigit(dts[4]) && isdigit(dts[5])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, dts);
        if (!(isdigit(dts[6]) && isdigit(dts[7])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, dts);
        if (!(isdigit(dts[8]) && isdigit(dts[9])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, dts);
        if (!(isdigit(dts[10]) && isdigit(dts[11])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, dts);
        if (!(isdigit(dts[12]) && isdigit(dts[13])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, dts);
        tm.tm_year = dts[0] * 1000 + dts[1] * 100 + dts[2] * 10 + dts[3] - '0' * 1111;
        tm.tm_mon = dts[4] * 10 + dts[5] - '0' * 11;
        tm.tm_mday = dts[6] * 10 + dts[7] - '0' * 11;
        h = dts[8] * 10 + dts[9] - '0' * 11;
        m = dts[10] * 10 + dts[11] - '0' * 11;
        s = dts[12] * 10 + dts[13] - '0' * 11;
    } else if (dts.length() == 19) {
        if (!((dts[4] == ':' && dts[7] == ':' && dts[10] == ':' && dts[13] == ':' && dts[16] == ':')
              || (dts[4] == '-' && dts[7] == '-' && dts[10] == '-' && dts[13] == '-' && dts[16] == '-')))
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
        if (!(isdigit(dts[0]) && isdigit(dts[1]) && isdigit(dts[2]) && isdigit(dts[3])))
            throw bad_datetime(__FILE__, __LINE__, 93, bad_datetime::bad_year, dts);
        if (!(isdigit(dts[5]) && isdigit(dts[6])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, dts);
        if (!(isdigit(dts[8]) && isdigit(dts[9])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, dts);
        if (!(isdigit(dts[11]) && isdigit(dts[12])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, dts);
        if (!(isdigit(dts[14]) && isdigit(dts[15])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, dts);
        if (!(isdigit(dts[17]) && isdigit(dts[18])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, dts);
        tm.tm_year = dts[0] * 1000 + dts[1] * 100 + dts[2] * 10 + dts[3] - '0' * 1111;
        tm.tm_mon = dts[5] * 10 + dts[6] - '0' * 11;
        tm.tm_mday = dts[8] * 10 + dts[9] - '0' * 11;
        h = dts[11] * 10 + dts[12] - '0' * 11;
        m = dts[14] * 10 + dts[15] - '0' * 11;
        s = dts[17] * 10 + dts[18] - '0' * 11;
    } else {
        throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
    }
    check_time(h, m, s);
    check_date(tm.tm_year, tm.tm_mon, tm.tm_mday);
    tm.tm_year -= 1900;
    tm.tm_mon--;
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = s;
    tm.tm_isdst = 0;
    duration_ = mktime(&tm);
}

datetime::datetime(const datetime& dt)
: calendar(dt.duration_)
{
}

datetime::datetime(time_t dd)
: calendar(dd)
{
}

datetime& datetime::operator=(const datetime& dt)
{
    duration_ = dt.duration_;
    return *this;
}

datetime& datetime::operator=(time_t dd)
{
    this->duration_ = dd;
    return *this;
}

datetime& datetime::operator=(const string& dts)
{
    int h;
    int m;
    int s;
    tm tm;
    if (dts.length() == 14) {
        if (!(isdigit(dts[0]) && isdigit(dts[1]) && isdigit(dts[2]) && isdigit(dts[3])))
            throw bad_datetime(__FILE__, __LINE__, 93, bad_datetime::bad_year, dts);
        if (!(isdigit(dts[4]) && isdigit(dts[5])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, dts);
        if (!(isdigit(dts[6]) && isdigit(dts[7])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, dts);
        if (!(isdigit(dts[8]) && isdigit(dts[9])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, dts);
        if (!(isdigit(dts[10]) && isdigit(dts[11])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, dts);
        if (!(isdigit(dts[12]) && isdigit(dts[13])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, dts);
        tm.tm_year = dts[0] * 1000 + dts[1] * 100 + dts[2] * 10 + dts[3] - '0' * 1111;
        tm.tm_mon = dts[4] * 10 + dts[5] - '0' * 11;
        tm.tm_mday = dts[6] * 10 + dts[7] - '0' * 11;
        h = dts[8] * 10 + dts[9] - '0' * 11;
        m = dts[10] * 10 + dts[11] - '0' * 11;
        s = dts[12] * 10 + dts[13] - '0' * 11;
    } else if (dts.length() == 19) {
        if (!((dts[4] == ':' && dts[7] == ':' && dts[10] == ':' && dts[13] == ':' && dts[16] == ':')
              || (dts[4] == '-' && dts[7] == '-' && dts[10] == '-' && dts[13] == '-' && dts[16] == '-')))
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
        if (!(isdigit(dts[0]) && isdigit(dts[1]) && isdigit(dts[2]) && isdigit(dts[3])))
            throw bad_datetime(__FILE__, __LINE__, 93, bad_datetime::bad_year, dts);
        if (!(isdigit(dts[5]) && isdigit(dts[6])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, dts);
        if (!(isdigit(dts[8]) && isdigit(dts[9])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, dts);
        if (!(isdigit(dts[11]) && isdigit(dts[12])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, dts);
        if (!(isdigit(dts[14]) && isdigit(dts[15])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, dts);
        if (!(isdigit(dts[17]) && isdigit(dts[18])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, dts);
        tm.tm_year = dts[0] * 1000 + dts[1] * 100 + dts[2] * 10 + dts[3] - '0' * 1111;
        tm.tm_mon = dts[5] * 10 + dts[6] - '0' * 11;
        tm.tm_mday = dts[8] * 10 + dts[9] - '0' * 11;
        h = dts[11] * 10 + dts[12] - '0' * 11;
        m = dts[14] * 10 + dts[15] - '0' * 11;
        s = dts[17] * 10 + dts[18] - '0' * 11;
    } else {
        throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
    }
    check_time(h, m, s);
    check_date(tm.tm_year, tm.tm_mon, tm.tm_mday);
    tm.tm_year -= 1900;
    tm.tm_mon--;
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = s;
    tm.tm_isdst = 0;

    this->duration_ = mktime(&tm);

    return *this;
}

datetime& datetime::operator=(const char* pdatestr)
{
    string dts(pdatestr);
    return  *this = dts;
}

datetime& datetime::assign(const string& dts, const char* format)
{
    const char* pos;
    string year;
    string month;
    string day;
    string hour;
    string minute;
    string second;
    string result;

    try {
        if ((pos = ::strstr(format, "YYYY")) != NULL) {
            year = dts.substr(pos - format, 4);
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "MM")) != NULL) {
            month = dts.substr(pos - format, 2);
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "DD")) != NULL) {
            day = dts.substr(pos - format, 2);
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "hh")) != NULL) {
            hour = dts.substr(pos - format, 2);
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "mi")) != NULL) {
            minute = dts.substr(pos - format, 2);
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "ss")) != NULL) {
            second = dts.substr(pos - format, 2);
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }
    } catch (std::out_of_range) {
        throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
    }

    result = year + ':' + month + ':' + day + ':' + hour + ':' + minute + ':' + second;

    return *this = result;
}

datetime datetime::operator+(time_t dd) const
{
    datetime dt(*this);
    dt += dd;
    return dt;
}

datetime datetime::operator-(time_t dd) const
{
    datetime dt(*this);
    dt -= dd;
    return dt;
}

int datetime::year() const
{
    tm tm;
    localtime_r(&duration_, &tm);
    return tm.tm_year + 1900;
}

bool datetime::is_leap() const
{
    tm tm;
    localtime_r(&duration_, &tm);
    int y = tm.tm_year + 1900;
    return(y % 400 == 0) ? true : ((y % 100 == 0) ? false : (y % 4 == 0));
}

int datetime::month() const
{
    tm tm;
    localtime_r(&duration_, &tm);
    return tm.tm_mon + 1;
}

int datetime::day() const
{
    tm tm;
    localtime_r(&duration_, &tm);
    return tm.tm_mday;
}

int datetime::hour() const
{
    return((duration_ + time_zone) % 86400) / 3600;
}

int datetime::minute() const
{
    return(duration_ % 3600) / 60;
}

int datetime::second() const
{
    return duration_ % 60;
}

int datetime::week() const
{
    // 19700101 is Thursday.
    return((duration_ + time_zone) / 86400 + 3) % 7 + 1;
}

time_t datetime::date() const
{
    return duration_ - (duration_ + time_zone) % 86400;
}

time_t datetime::time() const
{
    return(duration_ + time_zone) % 86400;
}

void datetime::next_month(int n)
{
    tm tm;
    localtime_r(&duration_, &tm);
    tm.tm_mon += n;
    tm.tm_mday = 1;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    duration_ = mktime(&tm);
}

void datetime::next_day(int n)
{
    tm tm;
    localtime_r(&duration_, &tm);
    tm.tm_mday += n;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    duration_ = mktime(&tm);
}

void datetime::next_hour(int n)
{
    tm tm;
    localtime_r(&duration_, &tm);
    tm.tm_hour += n;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    duration_ = mktime(&tm);
}

void datetime::add_months(int n)
{
    tm tm;
    localtime_r(&duration_, &tm);
    tm.tm_mon += n;
    duration_ = mktime(&tm);
}

void datetime::subtract_months(int n)
{
    tm tm;
    localtime_r(&duration_, &tm);
    tm.tm_mon -= n;
    duration_ = mktime(&tm);
}

void datetime::hours_cover(time_t dd, vector<int>& hours) const
{
    hours.resize(0);
    // Get first hour in between.
    datetime dt(*this);
    dt += 3599; // One hour subtract 1 second, so we can get first hour covered.
    int hh = dt.hour();
    int count = (duration_ % 3600 + dd) / 3600;
    for (int i = 0; i < count; i++)
        hours.push_back(hh++ % 24);
}

void datetime::days_cover(time_t dd, vector<int>& days) const
{
    days.resize(0);
    // Get first day in between.
    datetime dt(*this);
    dt += 86399; // One day subtract 1 second, so we can get first day covered.
    int count = ((duration_ + time_zone) % 86400 + dd) / 86400;
    for (int i = 0; i < count; i++) {
        days.push_back(dt.day());
        dt += 86400;
    }
}

void datetime::month_cover(time_t dd, vector<int>& months) const
{
    months.resize(0);
    datetime dt(*this);
    // So that if it's the first time of month, next_month will still be itself.
    dt -= 1;
    datetime end(*this);
    end += dd;
    while (1) {
        dt.next_month();
        if (dt < end)
            break;
        months.push_back(dt.month());
    }
}

void datetime::iso_string(string& dts) const
{
    tm tm;
    char buf[15];
    localtime_r(&duration_, &tm);
    if (tm.tm_year > 8099)
        tm.tm_year = 8099;
    strftime(buf, 15, "%Y%m%d%H%M%S", &tm);
    dts = buf;
}

void datetime::iso_extended_string(string& dts) const
{
    tm tm;
    char buf[32];
    localtime_r(&duration_, &tm);
    if (tm.tm_year > 8099)
        tm.tm_year = 8099;
    strftime(buf, 20, "%Y-%m-%d-%H-%M-%S", &tm);
    dts = buf;
}

sql_datetime::sql_datetime(int y, int m, int d, int h, int mi, int s)
{
    dateYYYY = y;
    dateMM = m;
    dateDD = d;
    timeHH = h;
    timeMI = mi;
    timeSS = s;
}

sql_datetime::sql_datetime(const string& dts)
{
    if (dts.length() == 14) {
        if (!(isdigit(dts[0]) && isdigit(dts[1]) && isdigit(dts[2]) && isdigit(dts[3])))
            throw bad_datetime(__FILE__, __LINE__, 93,bad_datetime::bad_year, dts);
        if (!(isdigit(dts[4]) && isdigit(dts[5])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, dts);
        if (!(isdigit(dts[6]) && isdigit(dts[7])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, dts);
        if (!(isdigit(dts[8]) && isdigit(dts[9])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, dts);
        if (!(isdigit(dts[10]) && isdigit(dts[11])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, dts);
        if (!(isdigit(dts[12]) && isdigit(dts[13])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, dts);
        dateYYYY = dts[0] * 1000 + dts[1] * 100 + dts[2] * 10 + dts[3] - '0' * 1111;
        dateMM = dts[4] * 10 + dts[5] - '0' * 11;
        dateDD = dts[6] * 10 + dts[7] - '0' * 11;
        timeHH = dts[8] * 10 + dts[9] - '0' * 11;
        timeMI = dts[10] * 10 + dts[11] - '0' * 11;
        timeSS = dts[12] * 10 + dts[13] - '0' * 11;
    } else if (dts.length() == 19) {
        if (!((dts[4] == ':' && dts[7] == ':' && dts[10] == ':' && dts[13] == ':' && dts[16] == ':')
              || (dts[4] == '-' && dts[7] == '-' && dts[10] == '-' && dts[13] == '-' && dts[16] == '-')))
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
        if (!(isdigit(dts[0]) && isdigit(dts[1]) && isdigit(dts[2]) && isdigit(dts[3])))
            throw bad_datetime(__FILE__, __LINE__, 93, bad_datetime::bad_year, dts);
        if (!(isdigit(dts[5]) && isdigit(dts[6])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, dts);
        if (!(isdigit(dts[8]) && isdigit(dts[9])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, dts);
        if (!(isdigit(dts[11]) && isdigit(dts[12])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, dts);
        if (!(isdigit(dts[14]) && isdigit(dts[15])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, dts);
        if (!(isdigit(dts[17]) && isdigit(dts[18])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, dts);
        dateYYYY = dts[0] * 1000 + dts[1] * 100 + dts[2] * 10 + dts[3] - '0' * 1111;
        dateMM = dts[5] * 10 + dts[6] - '0' * 11;
        dateDD = dts[8] * 10 + dts[9] - '0' * 11;
        timeHH = dts[11] * 10 + dts[12] - '0' * 11;
        timeMI = dts[14] * 10 + dts[15] - '0' * 11;
        timeSS = dts[17] * 10 + dts[18] - '0' * 11;
    } else {
        throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
    }
}

sql_datetime::sql_datetime(const sql_datetime& dt)
{
    dateYYYY = dt.dateYYYY;
    dateMM = dt.dateMM;
    dateDD = dt.dateDD;
    timeHH = dt.timeHH;
    timeMI = dt.timeMI;
    timeSS = dt.timeSS;
}

sql_datetime& sql_datetime::operator=(const sql_datetime& dt)
{
    dateYYYY = dt.dateYYYY;
    dateMM = dt.dateMM;
    dateDD = dt.dateDD;
    timeHH = dt.timeHH;
    timeMI = dt.timeMI;
    timeSS = dt.timeSS;
    return *this;
}

sql_datetime& sql_datetime::operator=(const string& dts)
{
    if (dts.length() == 14) {
        if (!(isdigit(dts[0]) && isdigit(dts[1]) && isdigit(dts[2]) && isdigit(dts[3])))
            throw bad_datetime(__FILE__, __LINE__, 93, bad_datetime::bad_year, dts);
        if (!(isdigit(dts[4]) && isdigit(dts[5])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, dts);
        if (!(isdigit(dts[6]) && isdigit(dts[7])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, dts);
        if (!(isdigit(dts[8]) && isdigit(dts[9])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, dts);
        if (!(isdigit(dts[10]) && isdigit(dts[11])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, dts);
        if (!(isdigit(dts[12]) && isdigit(dts[13])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, dts);
        dateYYYY = dts[0] * 1000 + dts[1] * 100 + dts[2] * 10 + dts[3] - '0' * 1111;
        dateMM = dts[4] * 10 + dts[5] - '0' * 11;
        dateDD = dts[6] * 10 + dts[7] - '0' * 11;
        timeHH = dts[8] * 10 + dts[9] - '0' * 11;
        timeMI = dts[10] * 10 + dts[11] - '0' * 11;
        timeSS = dts[12] * 10 + dts[13] - '0' * 11;
    } else if (dts.length() == 19) {
        if (!((dts[4] == ':' && dts[7] == ':' && dts[10] == ':' && dts[13] == ':' && dts[16] == ':')
              || (dts[4] == '-' && dts[7] == '-' && dts[10] == '-' && dts[13] == '-' && dts[16] == '-')))
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
        if (!(isdigit(dts[0]) && isdigit(dts[1]) && isdigit(dts[2]) && isdigit(dts[3])))
            throw bad_datetime(__FILE__, __LINE__, 93, bad_datetime::bad_year, dts);
        if (!(isdigit(dts[5]) && isdigit(dts[6])))
            throw bad_datetime(__FILE__, __LINE__, 94, bad_datetime::bad_month, dts);
        if (!(isdigit(dts[8]) && isdigit(dts[9])))
            throw bad_datetime(__FILE__, __LINE__, 95, bad_datetime::bad_day, dts);
        if (!(isdigit(dts[11]) && isdigit(dts[12])))
            throw bad_datetime(__FILE__, __LINE__, 96, bad_datetime::bad_hour, dts);
        if (!(isdigit(dts[14]) && isdigit(dts[15])))
            throw bad_datetime(__FILE__, __LINE__, 97, bad_datetime::bad_minute, dts);
        if (!(isdigit(dts[17]) && isdigit(dts[18])))
            throw bad_datetime(__FILE__, __LINE__, 98, bad_datetime::bad_second, dts);
        dateYYYY = dts[0] * 1000 + dts[1] * 100 + dts[2] * 10 + dts[3] - '0' * 1111;
        dateMM = dts[5] * 10 + dts[6] - '0' * 11;
        dateDD = dts[8] * 10 + dts[9] - '0' * 11;
        timeHH = dts[11] * 10 + dts[12] - '0' * 11;
        timeMI = dts[14] * 10 + dts[15] - '0' * 11;
        timeSS = dts[17] * 10 + dts[18] - '0' * 11;
    } else {
        throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
    }

    return *this;
}

sql_datetime& sql_datetime::operator=(const char* pdatestr)
{
    string dts(pdatestr);
    return  *this = dts;
}

sql_datetime& sql_datetime::assign(const string& dts, const char* format)
{
    const char* pos;

    try {
        if ((pos = ::strstr(format, "YYYY")) != NULL) {
            dateYYYY = ::atoi(dts.substr(pos - format, 4).c_str());
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "MM")) != NULL) {
            dateMM = ::atoi(dts.substr(pos - format, 2).c_str());
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "DD")) != NULL) {
            dateDD = ::atoi(dts.substr(pos - format, 2).c_str());
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "hh")) != NULL) {
            timeHH = ::atoi(dts.substr(pos - format, 2).c_str());
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "mi")) != NULL) {
            timeMI = ::atoi(dts.substr(pos - format, 2).c_str());
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }

        if ((pos = ::strstr(format, "ss")) != NULL) {
            timeSS = ::atoi(dts.substr(pos - format, 2).c_str());
        } else {
            throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, format);
        }
    } catch (std::out_of_range) {
        throw bad_datetime(__FILE__, __LINE__, 99, bad_datetime::bad_other, dts);
    }

    return *this;
}

int sql_datetime::year() const
{
    return dateYYYY;
}

int sql_datetime::month() const
{
    return dateMM;
}

int sql_datetime::day() const
{
    return dateDD;
}

int sql_datetime::hour() const
{
    return timeHH;
}

int sql_datetime::minute() const
{
    return timeMI;
}

int sql_datetime::second() const
{
    return timeSS;
}

bool sql_datetime::is_leap() const
{
    return(dateYYYY % 400 == 0) ? true : ((dateYYYY % 100 == 0) ? false : (dateYYYY % 4 == 0));
}

void sql_datetime::iso_string(string& dts) const
{
    char buf[15];
    ::sprintf(buf, "%04d%02d%02d%02d%02d%02d", dateYYYY, dateMM, dateDD, timeHH, timeMI, timeSS);
    dts = buf;
}

void sql_datetime::iso_extended_string(string& dts) const
{
    char buf[20];
    ::sprintf(buf, "%04d-%02d-%02d-%02d-%02d-%02d", dateYYYY, dateMM, dateDD, timeHH, timeMI, timeSS);
    dts = buf;
}

}
}

