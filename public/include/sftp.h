#if !defined(__SFTP_H__)
#define __SFTP_H__

#include <string>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/detail/ios.hpp>
#include <boost/iostreams/detail/fstream.hpp>
#include <boost/iostreams/operations.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tokenizer.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sshp_ctx.h"
#include "user_exception.h"
#include "common.h"

namespace bios = boost::iostreams;
typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

namespace ai
{
namespace sg
{

using std::ios_base;

template<typename Ch>
class basic_sftp
{
public:
	typedef Ch char_type;
	struct category
		: public bios::dual_seekable,
		  public bios::device_tag,
		  public bios::closable_tag,
		  public bios::flushable_tag
	{};

	basic_sftp()
	{
		SSHP = sshp_ctx::instance();
		sftp = NULL;
		sftp_handle = NULL;
	}

	basic_sftp(const std::string& sftp_prefix, const std::string& path, ios_base::openmode mode = ios_base::in | ios_base::out)
	{
		SSHP = sshp_ctx::instance();
		sftp = NULL;
		sftp_handle = NULL;
		open(sftp_prefix, path, mode);
	}

	basic_sftp(boost::shared_ptr<ssh_session> session_, const std::string& path, ios_base::openmode mode = ios_base::in | ios_base::out)
	{
		SSHP = sshp_ctx::instance();
		sftp = NULL;
		sftp_handle = NULL;
		open(session_, path, mode);
	}

	void open(const std::string& sftp_prefix, const std::string& path, ios_base::openmode mode = ios_base::in | ios_base::out)
	{
		open(SSHP->get_session(sftp_prefix), path, mode);
	}

	void open(boost::shared_ptr<ssh_session> session_, const std::string& path, ios_base::openmode mode = ios_base::in | ios_base::out)
	{
		close();
		session = session_;
		sftp = session->get_sftp();

		unsigned long sftp_flags = 0;

		if (mode & ios_base::app)
			sftp_flags |= LIBSSH2_FXF_APPEND;
		if (mode & ios_base::in)
			sftp_flags |= LIBSSH2_FXF_READ;
		if (mode & ios_base::out)
			sftp_flags |= LIBSSH2_FXF_CREAT | LIBSSH2_FXF_WRITE;
		if (mode & ios_base::trunc)
			sftp_flags |= LIBSSH2_FXF_TRUNC;

		mode_t mask = umask(0);
		umask(mask);

		path_ = path;
		sftp_handle = libssh2_sftp_open(sftp, path_.c_str(), sftp_flags, ~mask);
		if (sftp_handle == NULL)
			throw bad_ssh(__FILE__, __LINE__, 0, (_("ERROR: Can't open file {1}, {2}") % path_ % session->get_error()).str(SGLOCALE));

		if (mode & ios_base::ate)
			seek(0, ios_base::end);
	}

	std::streamsize read(char_type* s, std::streamsize n)
	{
		return libssh2_sftp_read(sftp_handle, s, n);
	}

	std::streamsize write(const char_type* s, std::streamsize n)
	{
		return libssh2_sftp_write(sftp_handle, s, n);
	}

	std::streampos seek(bios::stream_offset off, ios_base::seekdir way, ios_base::openmode which = ios_base::in | ios_base::out)
	{
		libssh2_uint64_t offset = off;

		switch (way) {
		case ios_base::beg:
			break;
		case ios_base::cur:
			offset += libssh2_sftp_tell64(sftp_handle);
			break;
		case ios_base::end:
			{
				LIBSSH2_SFTP_ATTRIBUTES attrs;
				int retval = libssh2_sftp_stat(sftp, path_.c_str(), &attrs);
				if (retval)
					throw bad_ssh(__FILE__, __LINE__, retval, (_("ERROR: Can't stat file {1}, {2}") % path_ % session->get_error()).str(SGLOCALE));

				offset += attrs.filesize;
			}
			break;
		default:
			break;
		}

		libssh2_sftp_seek64(sftp_handle, offset);
		return offset;
	}

	void close()
	{
		if (sftp_handle) {
			libssh2_sftp_close(sftp_handle);
			sftp_handle = NULL;
		}

		if (sftp) {
			session->put_sftp(sftp);
			sftp = NULL;
		}

		path_ = "";
	}

	bool flush()
	{
		return (libssh2_channel_flush(libssh2_sftp_get_channel(sftp)) == 0);
	}

	LIBSSH2_SFTP * get_sftp()
	{
		return sftp;
	}

private:
	sshp_ctx *SSHP;
	boost::shared_ptr<ssh_session> session;
	LIBSSH2_SFTP *sftp;
	LIBSSH2_SFTP_HANDLE *sftp_handle;
	string path_;
};

typedef basic_sftp<char> sftp;
typedef basic_sftp<wchar_t> wsftp;
typedef bios::stream<sftp> sftpstream;
typedef bios::stream<wsftp> wsftpstream;

template<typename Ch>
class basic_sftp_source : public basic_sftp<Ch>
{
public:
	typedef Ch char_type;
	struct category
		: public bios::input_seekable,
		  public bios::device_tag,
		  public bios::closable_tag
	{};

	basic_sftp_source()
	{
	}

	basic_sftp_source(const std::string& sftp_prefix, const std::string& path, ios_base::openmode mode = ios_base::in)
		: basic_sftp<Ch>(sftp_prefix, path, mode & ~ios_base::out)
	{
	}

	basic_sftp_source(boost::shared_ptr<ssh_session> session_, const std::string& path, ios_base::openmode mode = ios_base::in)
		: basic_sftp<Ch>(session_, path, mode & ~ios_base::out)
	{
	}

	void open(const std::string& sftp_prefix, const std::string& path, ios_base::openmode mode = ios_base::in)
	{
		basic_sftp<Ch>::open(sftp_prefix, path, mode & ~ios_base::out);
	}

	void open(boost::shared_ptr<ssh_session> session_, const std::string& path, ios_base::openmode mode = ios_base::in)
	{
		basic_sftp<Ch>::open(session_, path, mode & ~ios_base::out);
	}
};

typedef basic_sftp_source<char> sftp_source;
typedef basic_sftp_source<wchar_t> wsftp_source;
typedef bios::stream<sftp_source> isftpstream;
typedef bios::stream<wsftp_source> iwsftpstream;

template<typename Ch>
class basic_sftp_sink : public basic_sftp<Ch>
{
public:
	typedef Ch char_type;
	struct category
		: public bios::output_seekable,
		  public bios::device_tag,
		  public bios::closable_tag,
		  public bios::flushable_tag
	{};

	basic_sftp_sink()
	{
	}

	basic_sftp_sink(const std::string& sftp_prefix, const std::string& path, ios_base::openmode mode = ios_base::out)
		: basic_sftp<Ch>(sftp_prefix, path, mode & ~ios_base::in)
	{
	}

	basic_sftp_sink(boost::shared_ptr<ssh_session> session_, const std::string& path, ios_base::openmode mode = ios_base::out)
		: basic_sftp<Ch>(session_, path, mode & ~ios_base::in)
	{
	}

	void open(const std::string& sftp_prefix, const std::string& path, ios_base::openmode mode = ios_base::out)
	{
		basic_sftp<Ch>::open(sftp_prefix, path, mode & ~ios_base::in);
	}

	void open(boost::shared_ptr<ssh_session> session_, const std::string& path, ios_base::openmode mode = ios_base::out)
	{
		basic_sftp<Ch>::open(session_, path, mode & ~ios_base::in);
	}
};

typedef basic_sftp_sink<char> sftp_sink;
typedef basic_sftp_sink<wchar_t> wsftp_sink;
typedef bios::stream<sftp_sink> osftpstream;
typedef bios::stream<wsftp_sink> owsftpstream;

}
}

#endif

