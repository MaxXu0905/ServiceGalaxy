#if !defined(__SG_EXCEPTION_H__)
#define __SG_EXCEPTION_H__

#include <exception>
#include <stdexcept>
#include <string>

using namespace std;

namespace ai
{
namespace sg
{

class null_exception : public std::exception
{
public:
	null_exception();
	~null_exception() throw();

	const char * what() const throw();
};

class sg_exception : public std::exception
{
public:
	sg_exception(const string& file, int line, int error_code, int native_code = 0, const exception& ex = null_exception());
	sg_exception(const string& file, int line, int error_code, int native_code = 0, const string& error_msg = "");
	~sg_exception() throw();

	const char * what() const throw();
	int get_error_code() const;
	int get_native_code() const;

private:
	const string& file_;
	int line_;
	int error_code_;
	int native_code_;
	std::string msg;
};

}
}

#endif

