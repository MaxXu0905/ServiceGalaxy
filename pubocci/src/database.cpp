/**
 * @file database.cpp
 * @brief The implement of the database class.
 * 
 * @author Dejin Bao
 * @author Former author list: Genquan Xu
 * @version 3.5
 * @date 2005-11-15
 */

#include "database.h"

namespace ai
{
namespace sg
{

using namespace oracle::occi;
using namespace std;

/**
 * @fn util::database::database(const string& user_name_, 
 *                              const string& password_, 
 *                              const string& connect_string_, 
 *                              unsigned int min_conn, 
 *                              unsigned int max_conn, 
 *                              unsigned int incr_conn = 1)
 * @brief The constructor function.
 *
 * Creates a connection pool based on the parameters specified.
 *
 * @param[in] user_name_ The database username.
 * @param[in] password_ The database password.
 * @param[in] connect_string_ The database connect string.
 * @param[in] min_conn The minimum number of connections in the pool. The 
 * minimum number of connections are opened by this method. Additional 
 * connections are opened only when necessary. Valid values are 1 and greater.
 * @param[in] max_conn The maximum number of connections in the pool. Valid 
 * values are 1 and greater.
 * @param[in] incr_conn The increment by which to increase the number of 
 * connections to be opened if current number of connections is less than 
 * max_conn. Valid values are 1 and greater. The default value is 1.
 *
 * @see util::database::database(const string&, const string&, const string&)
 * @see util::database::database(const database&)
 */
database::database(const string& user_name_,
                   const string& password_,
                   const string& connect_string_,
                   unsigned int min_conn,
                   unsigned int max_conn,
                   unsigned int incr_conn)
    : user_name(user_name_),
      password(password_),
      connect_string(connect_string_),
      clean(0)
{
//  env = Environment::createEnvironment(Environment::THREADED_UNMUTEXED);
    env = Environment::createEnvironment(Environment::THREADED_MUTEXED);
    pool = env->createConnectionPool(user_name, password, connect_string, 
                                     min_conn, max_conn, incr_conn);
    conn = pool->createConnection(user_name, password);
    connected = true;
}

/**
 * @overload util::database::database(const string& user_name_, const string& password_, const string& connect_string_ = "")
 *
 * This method establishes a connection to the database specified.
 *
 * @param[in] user_name_ The database username.
 * @param[in] password_ The database password.
 * @param[in] connect_string_ The database connect string. The default value is
 * null.
 *
 * @see util::database::database(const string&, const string&, const string&, unsigned int, unsigned int, unsigned int)
 * @see util::database::database(const database&)
 */
database::database(const string& user_name_,
                   const string& password_,
                   const string& connect_string_)
    : user_name(user_name_),
      password(password_),
      connect_string(connect_string_),
      clean(1)
{
//  env = Environment::createEnvironment();
    env = Environment::createEnvironment(Environment::THREADED_MUTEXED);
    pool = 0;
    conn = env->createConnection(user_name, password, connect_string);
    connected = true;
}

/**
 * @overload util::database::database(const database& the_db)
 *
 * Creates a pooled connection from the database specified.
 *
 * @param[in] the_db The database object.
 *
 * @see util::database::database(const string&, const string&, const string&, unsigned int, unsigned int, unsigned int)
 * @see util::database::database(const string&, const string&, const string&)
 */
database::database(const database& the_db)
    : user_name(the_db.user_name),
      password(the_db.password),
      connect_string(the_db.connect_string),
      clean(2)
{
    assert(the_db.clean == 0);
    env = the_db.env;
    pool = the_db.pool;
    conn = pool->createConnection(user_name, password);
    connected = true;
}

/**
 * @fn util::database::~database()
 * @brief The destructor function.
 *
 * Destroys a database object based on the clean flag.
 */
database::~database()
{
    disconnect();
    switch (clean)
    {
    case 0:
        env->terminateConnectionPool(pool);
        Environment::terminateEnvironment(env);
        break;
    case 1:
        Environment::terminateEnvironment(env);
        break;
    case 2:
        break;
    default:
        assert(0);
    }
}

/**
 * @fn Statement* util::database::create_statement(const string& sql_stmt)
 * @brief Creates a Statement object with the SQL statement specified.
 *
 * Searches the cache for a specified SQL statement and returns it; if not found, 
 * creates a new statement.
 *
 * @param[in] sql_stmt The SQL string to be assiocated with the Statement object.
 *
 * @return A pointer of the Statement object.
 *
 * @see Statement* util::database::create_statement()
 */
Statement* database::create_statement(const string& sql_stmt)
{
    return conn->createStatement(sql_stmt);
}

/**
 * @overload Statement* util::database::create_statement()
 *
 * Creates a Statement object with an empty SQL statment. And a new SQL statement
 * can be associated with the Statement object by some methods such as setSQL() 
 * or executeQuery() and so on.
 *
 * @return A pointer of the Statement object.
 *
 * @see Statement* util::database::create_statement(const string&)
 */
Statement* database::create_statement()
{
    return conn->createStatement();
}

/**
 * @fn void util::database::terminate_statement(Statement*& stmt)
 * @brief Closes a Statement object and frees all resources associated with it.
 *
 * The Statement object will be evaluated to NULL.
 *
 * @param[in,out] stmt The Statement object.
 */
void database::terminate_statement(Statement*& stmt)
{
    if (stmt)
        conn->terminateStatement(stmt);
    stmt = 0;
}

/**
 * @fn void util::database::commit()
 * @brief Commits all changes made since the previous commit or rollback, and 
 * releases any database locks currently held by the session.
 */
void database::commit()
{
    conn->commit();
}

/**
 * @fn void util::database::rollback()
 * @brief Drops all changes made since the previous commit or rollback, and 
 * releases any database locks currently held by the session.
 */
void database::rollback()
{
    conn->rollback();
}

/**
 * @fn Environment* database::get_env() const
 * @brief Provides a pointer of the Environment object for the Oracle Call Interface (OCI).
 *
 * @return A pointer of the Environment object.
 */
Environment* database::get_env() const
{
    return env;
}

/**
 * @fn Connection* database::get_conn() const
 * @brief Provides a pointer of the Connection object for the Oracle Call Interface (OCI).
 *
 * @return A pointer of the Connection object.
 */
Connection* database::get_conn() const
{
    return conn;
}

/**
 * @fn void util::database::connect()
 * @brief Establishes a connection to the database specified.
 */
void database::connect()
{
    if (!connected)
    {
        switch (clean)
        {
        case 0:
        case 2:
            if (conn == 0)
                conn = pool->createConnection(user_name, password);
            break;

        case 1:
            if (conn == 0)
                conn = env->createConnection(user_name, password, connect_string);
            break;
        default:
            assert(0);
            break;
        }
        connected = true;
    }
}

/**
 * @fn void util::database::disconnect()
 * @brief Terminates the connection, and free all related system resources.
 */
void database::disconnect()
{
    if (connected)
    {
        rollback();
        switch (clean)
        {
        case 0:
        case 2:
            pool->terminateConnection(conn);
            break;
        case 1:
            env->terminateConnection(conn);
            break;
        default:
            assert(0);
            break;
        }
        connected = false;
    }
}

}
}

