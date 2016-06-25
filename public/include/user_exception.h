/**
 * @file user_exception.h
 * @brief The definition of the user's exceptions.
 * 
 * @author Dejin Bao
 * @author Former author list: Genquan Xu
 * @version 3.5
 * @date 2005-11-18
 */

#if !defined(__USER_EXCEPTION_H__)
#define __USER_EXCEPTION_H__

#include <exception>
#include <stdexcept>
#include <string>
#include "machine.h"
#include "user_assert.h"

namespace ai
{
namespace sg
{

using std::string;
using std::exception;

/**
 * @class bad_file
 * @brief Exceptions thrown by file error.
 */
class bad_file : public exception
{
public:
/**
 * @enum bad_type
 * @brief The type of the exceptions thrown by file error.
 */
    enum bad_type
    {
        bad_stat,    ///<Error while stat a file.
        bad_lstat,    ///<Error while lstat a file.
        bad_statfs,    ///<Error while statfs a file.
        bad_sock,    ///<Error using socket.
        bad_open,    ///<Error while open a file.
        bad_seek,    ///<Error while seek a file.
        bad_read,    ///<Error while read from a file.
        bad_write,    ///<Error while write to a file.
        bad_flush,    ///<Error while flush a file.
        bad_close,    ///<Error while close a file.
        bad_remove,    ///<Error while remove a file.
        bad_rename,    ///<Error while rename a file.
        bad_opendir,    ///<Error while open a directory.
        bad_ioctl,    ///<Ioctl error.
        bad_popen    ///<Popen error.
    };

    bad_file(const string& file, int line, int sys_code_, int error_code_, const string& obj = "") throw();
    ~bad_file() throw();
    const char* error_msg() const throw();
    const char* what() const throw();

private:
    int sys_code;
    int error_code;    ///<The error code.
    string what_;    ///<The error message.
    static const char* messages[];    ///<The error content.
};

/**
 * @class bad_system
 * @brief Exceptions thrown by system error.
 */
class bad_system : public exception
{
public:
/**
 * @enum bad_type
 * @brief The type of the exceptions thrown by system error.
 */
    enum bad_type
    {
        bad_cmd,    ///<Calls system command error.
        bad_dlopen,    ///<Loads dll error.
        bad_dlsym,    ///<Gets function from dll error.
        bad_dlclose,    ///<Closes dll error.
        bad_reg,    ///<Regular expression not correct.
        bad_sem,    ///<Can't get semaphore.
        bad_shm,    ///<Can't get share memory.
        bad_msg,    ///<Can't get message.
        bad_pstat,    //<On HPUX, pstatXXX error.
        bad_alloc,    ///<memory alloc error.
        bad_select,    ///<select error.
        bad_fork,    ///<fork error.
        bad_exec,    ///<exec error.
        bad_other    ///<Other errors.
    };
    bad_system(){};
    bad_system(const string& file, int line, int sys_code_, int error_code_, const string& cause = "") throw();
    ~bad_system() throw();
    const char* error_msg() const throw();
    const char* what() const throw();

private:
    int sys_code;
    int error_code;    ///<The error code.
    string what_;    ///<The error message.
    static const char* messages[];    ///<The error content.
};

/**
 * @class bad_auth
 * @brief Exceptions thrown when username/password authentication doesn't pass.
 */
class bad_auth : public exception
{
public:
    bad_auth(const string& file, int line, int sys_code_) throw();
    ~bad_auth() throw();
    const char* what () const throw();

private:
    int sys_code;
    string what_;    ///<The error message.
};

/**
 * @class bad_param
 * @brief Exceptions thrown when parameters are not excepted.
 */
class bad_param : public exception
{
public:
    bad_param(const string& file, int line, int sys_code_, const string& cause) throw();
    ~bad_param() throw();
    const char* what () const throw();

private:
    int sys_code;
    string what_;    ///<The error message.
};

/**
 * @class bad_datetime
 * @brief Exceptions thrown when date/time are not correct.
 */
class bad_datetime : public exception
{
public:
/**
 * @enum bad_type
 * @brief The type of the exceptions thrown by date/time error.
 */
    enum bad_type
    {
        bad_year,    ///<It isn't a valid year.
        bad_month,    ///<It isn't a valid month.
        bad_day,    ///<It isn't a valid day.
        bad_hour,    ///<It isn't a valid hour.
        bad_minute,    ///<It isn't a valid minute.
        bad_second,    ///<It isn't a valid second.
        bad_other    ///<Other errors.
    };

    bad_datetime(const string& file, int line, int sys_code_, int error_code_, const string& obj = "") throw();
    ~bad_datetime() throw();
    const char* error_msg() const throw();
    const char* what () const throw();

private:
    int sys_code;
    int error_code;    ///<The error code.
    string what_;    ///<The error message.
    static const char* messages[];    ///<The error content.
};

/**
 * @class bad_msg
 * @brief Exceptions thrown when unknown error.
 */
class bad_msg : public exception
{
public:
    bad_msg(const string& file, int line, int sys_code_, const string& msg) throw();
    ~bad_msg() throw();
    const char* what () const throw();

private:
    int sys_code;
    string what_;    ///<The error message.
};

}
}

#endif

