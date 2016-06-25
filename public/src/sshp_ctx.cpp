#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "sshp_ctx.h"
#include "user_exception.h"
#include "machine.h"
#include "sftp.h"

using namespace std;

namespace ai
{
namespace sg
{

int const RETRIES = 100;

bad_ssh::bad_ssh(const string& file, int line, int error_code, const string& msg) throw()
{
	error_code_ = error_code;

	what_ = (_("ERROR: In file {1}:{2}: {3} - error_code {4}")
		% file % line % msg % error_code).str(SGLOCALE);
}

bad_ssh::~bad_ssh() throw()
{
}

const char* bad_ssh::what () const throw()
{
	return what_.c_str();
}

ssh_session::ssh_session(const string& ssh_prefix_)
	: ssh_prefix(ssh_prefix_)
{
	sock = -1;
	session = NULL;
	sftp_session = NULL;

	string::size_type pos = ssh_prefix.find("@");
	string usrname = ssh_prefix.substr(0, pos);
	string hostaddr = ssh_prefix.substr(pos + 1, ssh_prefix.length() - pos - 1);
	sockaddr_in sin;
	int retval;

	memset(&sin, 0, sizeof(sin));
	sin.sin_port = htons(22);
	if (::inet_aton(hostaddr.c_str(), &sin.sin_addr)) {
		sin.sin_family = AF_INET;
	} else {
		hostent *phe;

		if ((phe = gethostbyname(hostaddr.c_str())) == 0)
			throw bad_ssh(__FILE__, __LINE__, errno, (_("ERROR: Can't get host by name {1}") % hostaddr).str(SGLOCALE));

		sin.sin_family = phe->h_addrtype;
		memcpy(reinterpret_cast<char*>(&sin.sin_addr), phe->h_addr_list[0], phe->h_length);
	}

	for (int retries = 0;; retries++) {
		if (session) {
			libssh2_session_disconnect(session, (_("INFO: ssh session disconnect normally")).str(SGLOCALE).c_str());
			libssh2_session_free(session);
			session = NULL;
		}

		if (sock != -1) {
			close(sock);
			sock = -1;
		}

		sock = socket(AF_INET, SOCK_STREAM, 0);

		if (connect(sock, reinterpret_cast<struct sockaddr *>(&sin), sizeof(sin)))
			throw bad_ssh(__FILE__, __LINE__, errno, (_("ERROR: Can't connect to remote host {1} via ssh") % ssh_prefix).str(SGLOCALE));

		session = libssh2_session_init();
		if (session == NULL)
			throw bad_ssh(__FILE__, __LINE__, errno, (_("ERROR: Failed to initialize ssh session")).str(SGLOCALE));

		sshp_ctx *SSHP = sshp_ctx::instance();
		if (SSHP->_SSHP_compress && libssh2_session_flag(session, LIBSSH2_FLAG_COMPRESS, 1) != 0)
			throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Failed to set compress for ssh session, {1}") % get_error()).str(SGLOCALE));

		libssh2_session_set_blocking(session, 1);
		libssh2_session_set_timeout(session, 60000);

		retval = libssh2_session_handshake(session, sock);
		if (retval) {
			if (retries < RETRIES) {
				sleep(1);
				continue;
			}

			throw bad_ssh(__FILE__, __LINE__, retval, (_("ERROR: Failed to establish ssh session for {1}, {2}, error_code {3}") % ssh_prefix % get_error() % retval).str(SGLOCALE));
		}

		const char *ptr = getenv("HOME");
		if (ptr == NULL)
			throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Can't get environment HOME")).str(SGLOCALE));

		string public_key = ptr;
		public_key += "/.ssh/id_rsa.pub";

		string private_key = ptr;
		private_key += "/.ssh/id_rsa";

		if (libssh2_userauth_publickey_fromfile(session, usrname.c_str(), public_key.c_str(), private_key.c_str(), NULL)) {
			public_key = ptr;
			public_key += "/.ssh/id_dsa.pub";

			string private_key = ptr;
			private_key += "/.ssh/id_dsa";

			retval = libssh2_userauth_publickey_fromfile(session, usrname.c_str(), public_key.c_str(), private_key.c_str(), NULL);
			if (retval)
				throw bad_ssh(__FILE__, __LINE__, retval, (_("ERROR: Authentication by public key failed for {1}, {2}") % ssh_prefix % get_error()).str(SGLOCALE));
		}

		break;
	}

	sftp_session = NULL;
}

ssh_session::~ssh_session()
{
	if (sftp_session) {
		libssh2_sftp_shutdown(sftp_session);
		sftp_session = NULL;
	}

	if (session) {
		libssh2_session_disconnect(session, (_("INFO: ssh session disconnect normally")).str(SGLOCALE).c_str());
		libssh2_session_free(session);
		session = NULL;
	}

	if (sock != -1) {
		close(sock);
		sock = -1;
	}
}

LIBSSH2_SFTP * ssh_session::get_sftp()
{
	LIBSSH2_SFTP *retval;

	if (sftp_session != NULL) {
		retval = sftp_session;
		sftp_session = NULL;
		return retval;
	}

	retval = libssh2_sftp_init(session);
	if (retval == NULL)
		throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Failed to initialize sftp session, {1}") % get_error()).str(SGLOCALE));

	return retval;
}

void ssh_session::put_sftp(LIBSSH2_SFTP *sftp)
{
	if (sftp_session == NULL)
		sftp_session = sftp;
	else
		libssh2_sftp_shutdown(sftp);
}

LIBSSH2_CHANNEL * ssh_session::open_channel()
{
	LIBSSH2_CHANNEL *retval = libssh2_channel_open_session(session);
	if (retval == NULL)
		throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Failed to open channel session, {1}") % get_error()).str(SGLOCALE));

	return retval;
}

void ssh_session::close_channel(LIBSSH2_CHANNEL *channel)
{
	libssh2_channel_close(channel);
}

void ssh_session::free_channel(LIBSSH2_CHANNEL *channel)
{
	libssh2_channel_free(channel);
}

void ssh_session::scp(const std::string& src, const std::string& dst)
{
	FILE *fp = fopen(src.c_str(), "rb");
	if (!fp)
		throw bad_ssh(__FILE__, __LINE__, errno, (_("ERROR: Can't open file {1} - {2}") % src % strerror(errno)).str(SGLOCALE));

	struct stat statbuf;
	stat(src.c_str(), &statbuf);

	LIBSSH2_CHANNEL *channel = libssh2_scp_send(session, dst.c_str(), statbuf.st_mode & 0777, statbuf.st_size);
	if (!channel)
		throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Can't open a session on {1} - {2}") % ssh_prefix % get_error()).str(SGLOCALE));

	char buf[4096];
	do {
		size_t nread = fread(buf, 1, sizeof(buf), fp);
		if (nread <= 0)
			break;

		char *ptr = buf;
		do {
			int rc = libssh2_channel_write(channel, ptr, nread);
			if (rc < 0)
				throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Can't write on {1} - {2}") % ssh_prefix % get_error()).str(SGLOCALE));

			ptr += rc;
			nread -= rc;
		} while (nread);
	} while (1);

	libssh2_channel_send_eof(channel);
	libssh2_channel_wait_eof(channel);
	libssh2_channel_wait_closed(channel);
	libssh2_channel_free(channel);
}

const char * ssh_session::get_error()
{
	char *errmsg = NULL;

	libssh2_session_last_error(session, &errmsg, NULL, 0);
	return errmsg;
}

sftp_shared_ptr::sftp_shared_ptr(const string& ssh_prefix)
{
	sshp_ctx *SSHP = sshp_ctx::instance();
	session = SSHP->get_session(ssh_prefix);
	sftp = session->get_sftp();
}

sftp_shared_ptr::sftp_shared_ptr(boost::shared_ptr<ssh_session> _session)
	: session(_session)
{
	sftp = session->get_sftp();
}

sftp_shared_ptr::~sftp_shared_ptr()
{
	session->put_sftp(sftp);
}

LIBSSH2_SFTP * sftp_shared_ptr::get()
{
	return sftp;
}

LIBSSH2_SFTP& sftp_shared_ptr::operator*()
{
	return *sftp;
}

const char * sftp_shared_ptr::get_error()
{
	return session->get_error();
}

channel_shared_ptr::channel_shared_ptr(const string& ssh_prefix)
{
	sshp_ctx *SSHP = sshp_ctx::instance();
	session = SSHP->get_session(ssh_prefix);
	channel = session->open_channel();
	opened = true;
}

channel_shared_ptr::channel_shared_ptr(boost::shared_ptr<ssh_session> _session)
	: session(_session)
{
	channel = session->open_channel();
	opened = true;
}

channel_shared_ptr::~channel_shared_ptr()
{
	if (opened) {
		session->close_channel(channel);
		opened = false;
	}

	session->free_channel(channel);
}

LIBSSH2_CHANNEL * channel_shared_ptr::get()
{
	return channel;
}

LIBSSH2_CHANNEL& channel_shared_ptr::operator*()
{
	return *channel;
}

const char * channel_shared_ptr::get_error()
{
	return session->get_error();
}

int channel_shared_ptr::get_exit_status()
{
	if (opened) {
		session->close_channel(channel);
		opened = false;
	}

	return libssh2_channel_get_exit_status(channel);
}

string channel_shared_ptr::get_exit_signal()
{
	if (opened) {
		session->close_channel(channel);
		opened = false;
	}

	char *exitsignal = NULL;
	libssh2_channel_get_exit_signal(channel, &exitsignal, NULL, NULL, NULL, NULL, NULL);
	string result = exitsignal;
	return result;
}

boost::once_flag sshp_ctx::once_flag;
sshp_ctx * sshp_ctx::_instance = NULL;

sshp_ctx * sshp_ctx::instance()
{
	if (_instance == NULL)
		boost::call_once(once_flag, sshp_ctx::init_once);
	return _instance;
}

sshp_ctx::~sshp_ctx()
{
	libssh2_exit();
}

boost::shared_ptr<ssh_session> sshp_ctx::get_session(const string& ssh_prefix)
{
	map<string, boost::shared_ptr<ssh_session> >::iterator iter;
	boost::mutex::scoped_lock lock(_SSHP_session_mutex);

	iter = _SSHP_sessions.find(ssh_prefix);
	if (iter != _SSHP_sessions.end())
		return iter->second;

	boost::shared_ptr<ssh_session> session(new ssh_session(ssh_prefix));
	_SSHP_sessions[ssh_prefix] = session;
	return session;
}

void sshp_ctx::erase_session(const string& ssh_prefix)
{
	boost::mutex::scoped_lock lock(_SSHP_session_mutex);

	map<string, boost::shared_ptr<ssh_session> >::iterator iter = _SSHP_sessions.find(ssh_prefix);
	if (iter != _SSHP_sessions.end())
		_SSHP_sessions.erase(iter);
}

void sshp_ctx::clear()
{
	boost::mutex::scoped_lock lock(_SSHP_session_mutex);

	_SSHP_sessions.clear();
}

sshp_ctx::sshp_ctx()
{
	char *ptr = getenv("SGCOMPRESS");
	if (ptr == NULL || strcasecmp(ptr, "Y") != 0)
		_SSHP_compress = false;
	else
		_SSHP_compress = true;

	if (libssh2_init(0))
		throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Failed to initialize ssh library")).str(SGLOCALE));
}

void sshp_ctx::init_once()
{
	_instance = new sshp_ctx();
}

}
}

