/**
 * @file user_exception.cpp
 * @brief The implement of the user's exceptions.
 *
 * @author Dejin Bao
 * @author Former author list: Genquan Xu
 * @version 3.5
 * @date 2005-11-15
 */
#include "db_exception.h"

namespace ai
{
namespace sg
{

using namespace oracle::occi;
using namespace std;

/**
 * @fn util::bad_db::bad_db(const string& file,
 *                          int line,
 *                          const SQLException& ex,
 *                          const string& message = "") throw()
 * @brief The constructor function.
 *
 * Creates a bad_db object. To avoid the nested exception, it cannot throw a new
 * exception.
 *
 * @param[in] file The filename which throws an exception.
 * @param[in] line The line number of the file which throws an exception.
 * @param[in] ex The SQLException object.
 * @param[in] message The error message of the exception, and the default value
 * is null.
 */
bad_db::bad_db(const string& file, int line, int sys_code_, const SQLException& ex, const string& message) throw()
    : SQLException(ex)
{
    sys_code = sys_code_;

	what_ = (_("ERROR: In file {1}:{2}: exception: {3}, message: {4}")
		% file % line % ex.what() % message).str(SGLOCALE);
}

/**
 * @fn util::bad_db::~bad_db() throw()
 * @brief The destructor function.
 *
 * Destroys a bad_db object. To avoid the nested exception, it cannot throw a
 * new exception.
 */
bad_db::~bad_db() throw()
{
}

/**
 * @fn util::bad_db::what() const throw()
 * @brief Returns the error message associated with the SQLException.
 *
 * To avoid the nested exception, it cannot throw a new exception.
 */
const char* bad_db::what () const throw()
{
    return what_.c_str();
}

/**
 * @fn util::bad_oci::bad_oci(const string& file,
 *                            int line,
 *                            int error_code,
 *                            const string& message) throw()
 * @brief The constructor function.
 *
 * Creates a bad_oci object. To avoid the nested exception, it cannot throw a
 * new exception.
 *
 * @param[in] file The filename which throws an exception.
 * @param[in] line The line number of the file which throws an exception.
 * @param[in] error_code The error code of the exception.
 * @param[in] message The error message of the exception.
 */
bad_oci::bad_oci(const string& file, int line, int sys_code_, int error_code, const string& message) throw()
{
    sys_code = sys_code_;
    ostringstream fmt;
    fmt << error_code;
    strcpy(error_msg_, fmt.str().c_str());

	what_ = (_("ERROR: In file {1}:{2}: error_code: {3}, message: {4}")
		% file % line % error_code % message).str(SGLOCALE);
}

/**
 * @fn util::bad_oci::~bad_oci() throw()
 * @brief The destructor function.
 *
 * Destroys a bad_oci object. To avoid the nested exception, it cannot throw a
 * new exception.
 */
bad_oci::~bad_oci() throw()
{
}

/**
 * @fn const char* util::bad_oci::error_msg() const throw()
 * @brief Returns the error content associated with the error code.
 *
 * To avoid the nested exception, it cannot throw a new exception.
 */
const char* bad_oci::error_msg() const throw()
{
    return error_msg_;
}

/**
 * @fn const char* util::bad_oci::what() const throw()
 * @brief Returns the error message associated with the exception.
 *
 * To avoid the nested exception, it cannot throw a new exception.
 */
const char* bad_oci::what () const throw()
{
    return what_.c_str();
}

/**
 * @fn util::bad_ab::bad_ab(const string& file,
 *                            int line,
 *                            int error_code,
 *                            const string& message) throw()
 * @brief The constructor function.
 *
 * Creates a bad_oci object. To avoid the nested exception, it cannot throw a
 * new exception.
 *
 * @param[in] file The filename which throws an exception.
 * @param[in] line The line number of the file which throws an exception.
 * @param[in] error_code The error code of the exception.
 * @param[in] message The error message of the exception.
 */
bad_ab::bad_ab(const string& file, int line, int error_code, const string& message) throw()
{
    error_code_ = error_code;

	what_ = (_("ERROR: In file {1}:{2}: error_code: {3}, message: {4}")
		% file % line % error_code % message).str(SGLOCALE);
}

/**
 * @fn util::bad_ab::~bad_ab() throw()
 * @brief The destructor function.
 *
 * Destroys a bad_ab object. To avoid the nested exception, it cannot throw a
 * new exception.
 */
bad_ab::~bad_ab() throw()
{
}

/**
 * @fn int util::bad_ab::get_error_code() const throw()
 * @brief Returns the error code.
 *
 * To avoid the nested exception, it cannot throw a new exception.
 */
int bad_ab::get_error_code() const throw()
{
    return error_code_;
}

/**
 * @fn const char* util::bad_ab::error_msg() const throw()
 * @brief Returns the error content associated with the error code.
 *
 * To avoid the nested exception, it cannot throw a new exception.
 */
const char* bad_ab::error_msg() const throw()
{
    return what_.c_str();
}

/**
 * @fn const char* util::bad_ab::what() const throw()
 * @brief Returns the error message associated with the exception.
 *
 * To avoid the nested exception, it cannot throw a new exception.
 */
const char* bad_ab::what () const throw()
{
    return what_.c_str();
}

}
}

