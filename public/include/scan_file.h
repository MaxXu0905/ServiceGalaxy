#if !defined(__SCAN_FILE_H__)
#define __SCAN_FILE_H__

#include <string>
#include <vector>
#include <regex.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include "dstream.h"
#include "user_exception.h"
#include "user_assert.h"
#include "sshp_ctx.h"

using namespace std;

namespace ai
{
namespace sg
{

struct scan_info
{
	string file_dir;
	string file_name;
	int create_time;
};

// An operator() which means lhs > rhs is needed if you wan't to get file in ascending order.
class scan_compare
{
public:
	// Sort by file creation time and file_name in descending order, so get file in back will be
	// in ascending order.
	bool operator()(const scan_info& lhs, const scan_info& rhs)
	{
		if (lhs.file_name > rhs.file_name)
			return true;
		//else if (lhs.create_time == rhs.create_time && lhs.file_name > rhs.file_name)
		//	return true;
		else
			return false;
	}
};

template <typename compare = scan_compare>
class scan_file
{
public:
	// Scan file in single-directory mode.
	scan_file(const string& file_dir, const string& pattern_, int flags = 0, int file_count = 1024)
		: dir_vector(1, file_dir)
	{
		if (regcomp(&reg, pattern_.c_str(), REG_NOSUB | REG_EXTENDED | flags))
			throw bad_system(__FILE__, __LINE__, 179, bad_system::bad_reg, pattern_);

		file_vector.reserve(file_count);
	}

	// Scan file in multi-directory mode.
	scan_file(const vector<string>& dir_vector_, const string& pattern_, int flags = 0, int file_count = 1024)
		: dir_vector(dir_vector_)
	{
		if (regcomp(&reg, pattern_.c_str(), REG_NOSUB | REG_EXTENDED | flags))
			throw bad_system(__FILE__, __LINE__, 179, bad_system::bad_reg, pattern_);

		file_vector.reserve(file_count);
	}

	// Scan file in dir/sub-dirs mode.
	scan_file(const string& dir, const vector<string>& sub_dirs, const string& pattern_, int flags = 0, int file_count = 1024)
	{
		vector<string>::const_iterator iter;
		for (iter = sub_dirs.begin(); iter != sub_dirs.end(); ++iter)
			dir_vector.push_back(dir + '/' + *iter);

		if (regcomp(&reg, pattern_.c_str(), REG_NOSUB | REG_EXTENDED | flags))
			throw bad_system(__FILE__, __LINE__, 179, bad_system::bad_reg, pattern_);

		file_vector.reserve(file_count);
	}

	virtual ~scan_file()
	{
		regfree(&reg);
	}

	void set_sftp(const string& sftp_prefix_)
	{
		sftp_prefix = sftp_prefix_;
		if (sftp_prefix.empty())
			return;

		sshp_ctx *SSHP = sshp_ctx::instance();
		session = SSHP->get_session(sftp_prefix);
	}

	// Get a file in given directories. Upon file found, return true, otherwise return false.
	// In single-directory mode, return file name, otherwise return full name.
	bool get_file(string& file_name)
	{
		if (session == NULL)
			return get_local_file(file_name);
		else
			return get_sftp_file(file_name);
	}

	// Get all files in given directories.
	// In single-directory mode, return file name, otherwise return full name.
	void get_files(vector<string>& files)
	{
		if (session == NULL)
			return get_local_files(files);
		else
			return get_sftp_files(files);
	}

private:
	bool get_local_file(string& file_name)
	{
		DIR *dirp;
		dirent ent;
		dirent *result;
		struct stat stat_buf;
		string full_name;
		scan_info file_info;

		for (int i = 0; i < 2; i++) {
			while (file_vector.size() > 0) {
				vector<scan_info>::iterator iter = file_vector.begin();

				if (access((iter->file_dir + '/' + iter->file_name).c_str(), F_OK) == -1) {
					std::pop_heap(file_vector.begin(), file_vector.end(), compare());
					file_vector.pop_back();
					continue;
				}

				if (dir_vector.size() == 1)
					file_name = iter->file_name;
				else
					file_name = iter->file_dir + '/' + iter->file_name;

				std::pop_heap(file_vector.begin(), file_vector.end(), compare());
				file_vector.pop_back();
				return true;
			}

			if (i == 1)
				break;

			vector<string>::const_iterator dir_iter;
			for (dir_iter = dir_vector.begin(); dir_iter != dir_vector.end(); ++dir_iter) {
				dirp = opendir(dir_iter->c_str());
				if (!dirp)
					throw bad_file(__FILE__, __LINE__, 180, bad_file::bad_opendir, *dir_iter);

				while (readdir_r(dirp, &ent, &result) == 0 && result != 0) {
					if (regexec(&reg, ent.d_name, (size_t)0, 0, 0) != 0)
						continue;

					full_name = *dir_iter + '/' + ent.d_name;
					if (::lstat(full_name.c_str(), &stat_buf) < 0)
						continue;

					if (S_ISDIR(stat_buf.st_mode) != 0)
						continue;

					file_info.file_dir = *dir_iter;
					file_info.file_name = ent.d_name;
					file_info.create_time = stat_buf.st_mtime;
					file_vector.push_back(file_info);
				}

				closedir(dirp);
			}
		}

		return false;
	}

	void get_local_files(vector<string>& files)
	{
		DIR *dirp;
		dirent ent;
		dirent *result;
		struct stat stat_buf;
		string full_name;

		files.resize(0);
		vector<string>::const_iterator dir_iter;
		for (dir_iter = dir_vector.begin(); dir_iter != dir_vector.end(); ++dir_iter) {
			dirp = opendir(dir_iter->c_str());
			if (!dirp)
				throw bad_file(__FILE__, __LINE__, 180, bad_file::bad_opendir, *dir_iter);

			while (readdir_r(dirp, &ent, &result) == 0 && result != 0) {
				if (regexec(&reg, ent.d_name, (size_t)0, 0, 0) != 0)
					continue;

				full_name = *dir_iter + '/' + ent.d_name;
				if (::lstat(full_name.c_str(), &stat_buf) < 0)
					continue;

				if (S_ISDIR(stat_buf.st_mode) != 0)
					continue;

				files.push_back(ent.d_name);
			}

			closedir(dirp);
		}
	}

	bool get_sftp_file(string& file_name)
	{
		char sftp_name[_POSIX_PATH_MAX];
		LIBSSH2_SFTP_ATTRIBUTES attrs;
		scan_info file_info;

		for (int i = 0; i < 2; i++) {
			while (file_vector.size() > 0) {
				vector<scan_info>::iterator iter = file_vector.begin();

				if (dir_vector.size() == 1)
					file_name = iter->file_name;
				else
					file_name = iter->file_dir + '/' + iter->file_name;

				std::pop_heap(file_vector.begin(), file_vector.end(), compare());
				file_vector.pop_back();
				return true;
			}

			if (i == 1)
				break;

			while (1) {
				sftp_shared_ptr auto_sftp(session);
				LIBSSH2_SFTP *sftp = auto_sftp.get();

				vector<string>::const_iterator dir_iter;
				for (dir_iter = dir_vector.begin(); dir_iter != dir_vector.end(); ++dir_iter) {
					LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_opendir(sftp, dir_iter->c_str());
					if (!sftp_handle)
						throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Can't open directory {1} on {2} - {3}") % dir_iter->c_str() % sftp_prefix % auto_sftp.get_error()).str(SGLOCALE));

					while (libssh2_sftp_readdir(sftp_handle, sftp_name, sizeof(sftp_name), &attrs) > 0) {
						if (regexec(&reg, sftp_name, (size_t)0, 0, 0) != 0)
							continue;

						if (!LIBSSH2_SFTP_S_ISREG(attrs.permissions))
							continue;

						file_info.file_dir = *dir_iter;
						file_info.file_name = sftp_name;
						file_info.create_time = attrs.mtime;
						file_vector.push_back(file_info);
					}

					libssh2_sftp_closedir(sftp_handle);
				}
			}
		}

		return false;
	}

	void get_sftp_files(vector<string>& files)
	{
		char sftp_name[_POSIX_PATH_MAX];
		LIBSSH2_SFTP_ATTRIBUTES attrs;
		sftp_shared_ptr auto_sftp(session);
		LIBSSH2_SFTP *sftp = auto_sftp.get();

		files.resize(0);
		vector<string>::const_iterator dir_iter;
		for (dir_iter = dir_vector.begin(); dir_iter != dir_vector.end(); ++dir_iter) {
			LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_opendir(sftp, dir_iter->c_str());
			if (!sftp_handle)
				throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Can't open directory {1} on {2} - {3}") % dir_iter->c_str() % sftp_prefix % auto_sftp.get_error()).str(SGLOCALE));

			while (libssh2_sftp_readdir(sftp_handle, sftp_name, sizeof(sftp_name), &attrs) > 0) {
				if (regexec(&reg, sftp_name, (size_t)0, 0, 0) != 0)
					continue;

				if (!LIBSSH2_SFTP_S_ISREG(attrs.permissions))
					continue;

				files.push_back(sftp_name);
			}

			libssh2_sftp_closedir(sftp_handle);
		}
	}

	vector<string> dir_vector;
	regex_t reg;
	vector<scan_info> file_vector;
	string sftp_prefix;
	boost::shared_ptr<ssh_session> session;
};

}
}

#endif

