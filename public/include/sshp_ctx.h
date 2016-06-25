#if !defined(__SSHP_CTX_H__)
#define __SSHP_CTX_H__

#include <exception>
#include <string>
#include <map>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/once.hpp>

namespace ai
{
namespace sg
{

class bad_ssh : public std::exception
{
public:
	bad_ssh(const std::string& file, int line, int error_code, const std::string& msg) throw();
	~bad_ssh() throw();
	const char* what () const throw();

private:
	int error_code_;
	std::string what_;
};

class ssh_session
{
public:
	ssh_session(const std::string& ssh_prefix_);
	~ssh_session();

	LIBSSH2_SFTP * get_sftp();
	void put_sftp(LIBSSH2_SFTP *sftp);
	LIBSSH2_CHANNEL * open_channel();
	void close_channel(LIBSSH2_CHANNEL *channel);
	void free_channel(LIBSSH2_CHANNEL *channel);
	void scp(const std::string& src, const std::string& dst);
	const char * get_error();

private:
	std::string ssh_prefix;
	libssh2_socket_t sock;
	LIBSSH2_SESSION *session;
	LIBSSH2_SFTP *sftp_session;
};

class sftp_shared_ptr
{
public:
	sftp_shared_ptr(const std::string& ssh_prefix);
	sftp_shared_ptr(boost::shared_ptr<ssh_session> _session);
	~sftp_shared_ptr();

	LIBSSH2_SFTP * get();
	LIBSSH2_SFTP& operator*();
	const char * get_error();

private:
	boost::shared_ptr<ssh_session> session;
	LIBSSH2_SFTP *sftp;
};

class channel_shared_ptr
{
public:
	channel_shared_ptr(const std::string& ssh_prefix);
	channel_shared_ptr(boost::shared_ptr<ssh_session> _session);
	~channel_shared_ptr();

	LIBSSH2_CHANNEL * get();
	LIBSSH2_CHANNEL& operator*();
	const char * get_error();
	int get_exit_status();
	std::string get_exit_signal();

private:
	boost::shared_ptr<ssh_session> session;
	LIBSSH2_CHANNEL *channel;
	bool opened;
};

class sshp_ctx
{
public:
	static sshp_ctx * instance();
	~sshp_ctx();

	boost::shared_ptr<ssh_session> get_session(const std::string& ssh_prefix);
	void erase_session(const std::string& ssh_prefix);
	void clear();

	bool _SSHP_compress;

private:
	sshp_ctx();

	static void init_once();

	boost::mutex _SSHP_session_mutex;
	std::map<std::string, boost::shared_ptr<ssh_session> > _SSHP_sessions;

	static boost::once_flag once_flag;
	static sshp_ctx *_instance;
};

}
}

#endif

