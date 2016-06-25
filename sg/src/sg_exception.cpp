#include "sg_internal.h"

namespace ai
{
namespace sg
{

null_exception::null_exception()
{
}

null_exception::~null_exception() throw()
{
}

const char * null_exception::what() const throw()
{
	return "";
}

sg_exception::sg_exception(const string& file, int line, int error_code, int native_code, const exception& ex)
	: file_(file),
	  line_(line),
	  error_code_(error_code),
	  native_code_(native_code)
{
	msg = (_("ERROR: In file {1}:{2}: {3}, {4}")
		% file % line % sgt_ctx::strerror(error_code_) % uunix::strerror(native_code_)).str(SGLOCALE);
}

sg_exception::sg_exception(const string& file, int line, int error_code, int native_code, const string& error_msg)
	: file_(file),
	  line_(line),
	  error_code_(error_code),
	  native_code_(native_code)
{
	msg = (_("ERROR: In file {1}:{2}: {3}, {4}")
		% file % line % sgt_ctx::strerror(error_code_) % uunix::strerror(native_code_)).str(SGLOCALE);
	if (!error_msg.empty()) {
		msg += ", ";
		msg += error_msg;
	}
}

sg_exception::~sg_exception() throw()
{
}

const char * sg_exception::what() const throw()
{
	return msg.c_str();
}

int sg_exception::get_error_code() const
{
	return error_code_;
}

int sg_exception::get_native_code() const
{
	return native_code_;
}

}
}

