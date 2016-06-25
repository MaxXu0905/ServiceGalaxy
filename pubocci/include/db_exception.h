/**
 * @file user_exception.h
 * @brief The definition of the user's exceptions.
 * 
 * @author Dejin Bao
 * @author Former author list: Genquan Xu
 * @version 3.5
 * @date 2005-11-18
 */

#if !defined(__DB_EXCEPTION_H__)
#define __DB_EXCEPTION_H__

#if defined(MEM_DEBUG)
#undef new
#undef delete
#endif
#include <occi.h>
#if defined(MEM_DEBUG)
#define new new(__FILE__, __LINE__)
#define delete handle_delete(__FILE__, __LINE__),delete
#endif

#include <exception>
#include <stdexcept>
#include <string.h>
#include "user_assert.h"
#include "machine.h"

namespace ai
{
namespace sg
{

using namespace oracle::occi;
using namespace std;

/**
 * @class bad_db
 * @brief Exceptions thrown by OCCI.
 */
class bad_db : public SQLException
{
public:
    bad_db(const string& file, int line, int sys_code_, const SQLException& ex, const string& message = "") throw();
    ~bad_db() throw();
    const char* what () const throw();

private:
    int sys_code;
    string what_;    ///<The error message.
};

/**
 * @class bad_oci
 * @brief Exceptions thrown by OCI.
 */
class bad_oci : public exception
{
public:
    bad_oci(const string& file, int line, int sys_code_, int error_code, const string& message) throw();
    ~bad_oci() throw();
    const char* error_msg() const throw();
    const char* what () const throw();

private:
    int sys_code;
    char error_msg_[32];    ///<The error content.
    string what_;    ///<The error message.
};

/**
 * @class bad_ab
 * @brief Exceptions thrown by Altibase.
 */
class bad_ab : public exception
{
public:
    bad_ab(const string& file, int line, int error_code, const string& message) throw();
    ~bad_ab() throw();
    int get_error_code() const throw();
    const char* error_msg() const throw();
    const char* what () const throw();

private:
    int error_code_;
    string what_;    ///<The error message.
};

}
}

#endif

