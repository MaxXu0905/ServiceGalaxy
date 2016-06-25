/**
 * @file database.h
 * @brief The definition of the database class.
 * 
 * @author Dejin Bao
 * @author Former author list: Genquan Xu
 * @version 3.5.0
 * @date 2005-11-15
 */

#if !defined(__DATABASE_H__)
#define __DATABASE_H__

#if defined(MEM_DEBUG)
#undef new
#undef delete
#endif
#include <occi.h>
#if defined(MEM_DEBUG)
#define new new(__FILE__, __LINE__)
#define delete handle_delete(__FILE__, __LINE__),delete
#endif

#include <string>
#include "user_assert.h"

namespace ai
{
namespace sg
{

using namespace oracle::occi;
using namespace std;

/**
 * @class database
 * @brief The database class is a simple encapsulation of the Oracle C++ Call 
 * Interface (OCCI).
 *
 * The database class provides:
 * <ul>
 * <li type="disc"> 
 * Connects the database.
 * </li>
* <li type="disc"> 
 * Disconnects the database.
 * </li>
 * <li type="disc"> 
 * Creates the Statement object.
 * </li>
 * <li type="disc"> 
 * Terminates the Statement object.
 * </li>
 * </li>
 * <li type="disc"> 
 * Commit.
 * </li>
 * </li>
 * <li type="disc"> 
 * Rollback.
 * </li>
 * </li>
 * <li type="disc"> 
 * Provides a pointer of the Environment object for the Oracle Call Interface (OCI).
 * </li>
 * </li>
 * <li type="disc"> 
 * Provides a pointer of the Connection object for the Oracle Call Interface (OCI).
 * </li>
 * </ul>
 */
class database
{
public:
    database(const string& user_name_,
             const string& password_,
             const string& connect_string_,
             unsigned int min_conn,
             unsigned int max_conn,
             unsigned int incr_conn = 1);
    database(const string& user_name_,
             const string& password_,
             const string& connect_string_ = "");
    database(const database& the_db);
    virtual ~database();
    Statement* create_statement(const string& sql_stmt);
    Statement* create_statement();
    void terminate_statement(Statement*& stmt);
    void commit();
    void rollback();
    Environment* get_env() const;
    Connection* get_conn() const;
    void connect();
    void disconnect();
private:
    const string user_name;    ///<The database username.
    const string password;    ///<The database password.
    const string connect_string;    ///<The database connect string.
    Environment* env;    ///<A pointer of a OCCI environment for the database.
    ConnectionPool* pool;
    ///<A pointer of a pool of connections for the database.
    Connection* conn;
    ///<A pointer of a connection for the database.
    const int clean;    ///<A flag for the destructor function.
    bool connected;    ///<The connection status.
};

}
}

#endif
