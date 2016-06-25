/////////////////////////////////////////////////////////////////////////////////
// modification history
//-------------------------------------------------------------------------------
// add method
// datetime& datetime::operator=(const string& dts)
// datetime& datetime::operator=(const char* pdatestr)
// datetime& datetime::assign(const string& dts, const char* format)
//-------------------------------------------------------------------------------

#include "dstream.h"
#include "db_common.h"
#include "common.h"

namespace ai
{
namespace sg
{

using namespace oracle::occi;
using namespace std;

unsigned int db_common::get_file_sn(database& db, const string & sn_name)
{
    string sql_stmt = "select ";
    sql_stmt += sn_name;
    sql_stmt += ".nextval from dual";
    Statement* stmt = 0;
    ResultSet* rset = 0;
    unsigned int ret;

    try {
        stmt = db.create_statement(sql_stmt);
        rset = stmt->executeQuery();
        rset->next();
        ret = rset->getUInt(1);
    } catch (SQLException& ex) {
        if (rset)
            stmt->closeResultSet(rset);
        db.terminate_statement(stmt);
        throw bad_db(__FILE__, __LINE__, 91, ex, sn_name);
    }

    stmt->closeResultSet(rset);
    db.terminate_statement(stmt);
    return ret;
}

bool db_common::get_passwd(database& db, const string& user_name,
                        const string& connect_string, string& passwd)
{
    string sql_stmt = "select passwd from sys_passwd where user_name = '" + user_name
                      + "' and connect_string = '" + connect_string + '\'';
    Statement* stmt = 0;
    ResultSet* rset = 0;
    bool ret;

    try {
        stmt = db.create_statement(sql_stmt);
        rset = stmt->executeQuery();
        if (rset->next()) {
            passwd = rset->getString(1);
            ret = true;
        } else {
            ret = false;
        }
    } catch (SQLException& ex) {
        if (rset)
            stmt->closeResultSet(rset);
        db.terminate_statement(stmt);
        throw bad_db(__FILE__, __LINE__, 92, ex, sql_stmt);
    }

    stmt->closeResultSet(rset);
    db.terminate_statement(stmt);
    return ret;
}

void db_common::load_param(param& para)
{
    string user_name;
    string password;
    string connect_string;

    common::read_db_info(user_name, password, connect_string);
    // Database will disconnect when exit its scope.
    database db(user_name, password, connect_string);
    string sql_stmt = "select user_name, passwd, connect_string, ipc_key, udp_port from v_sys_proc_para "
        "where module_id = '" + para.module_id + "' and process_id = '" + para.proc_id + '\'';

    dout << "sql_stmt = [" << sql_stmt << "]" << std::endl;

    Statement* stmt = 0;
    ResultSet* rset = 0;
    try
    {
        stmt = db.create_statement(sql_stmt);
        rset = stmt->executeQuery();
        if (!rset->next())
            throw bad_param(__FILE__, __LINE__, 10, (_("ERROR: No record in view v_sys_proc_para")).str(SGLOCALE));
        para.user_name = rset->getString(1);
        char text[1024];
        common::decrypt(rset->getString(2).c_str(), text);
        para.password = text;
        para.connect_string = rset->getString(3);
        para.ipc_key = rset->getInt(4);
        para.port = rset->getInt(5);
    }
    catch (SQLException& ex)
    {
        if (rset)
            stmt->closeResultSet(rset);
        db.terminate_statement(stmt);
        throw  bad_db(__FILE__, __LINE__, 11, ex, sql_stmt);
    }

    stmt->closeResultSet(rset);
    db.terminate_statement(stmt);

    dout << "user_name = [" << para.user_name << "]\n";
    dout << "connect_string = [" << para.connect_string << "]\n";
    dout << "ipc_key = " << para.ipc_key << "\n";
    dout << "port = " << para.port << "\n";
    dout << std::flush;
}

}
}

