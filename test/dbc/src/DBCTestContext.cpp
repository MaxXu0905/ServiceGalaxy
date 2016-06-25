/*
 * DBCTextContext.cpp
 *
 *  Created on: 2012-4-23
 *      Author: Administrator
 */

#include "DBCTestContext.h"
#include "FileUtils.h"
namespace ai {
namespace test {
namespace dbc {
DBCTest_Context::DBCTest_Context() {
	db = dbc = 0;
	dbcpid = 0;
}

DBCTest_Context::~DBCTest_Context() {
}
int DBCTest_Context::startDbc() {
	int pid = fork();
	if (pid == 0) {
		execlp("dbc_server", "dbc_server", (char *) 0);
		exit(-1);
	} else if (pid > 0) {
		sleep(1);
		return dbcpid = pid;
	} else {
		return -1;
	}

}
int DBCTest_Context::stopDbc() {
	int status = 0;
	if (dbcpid) {
		kill(dbcpid, SIGTERM);
		waitpid(dbcpid, &status, 0);
	}
	return status;
}
void DBCTest_Context::connect(const string & configfile){
	dbc_mgr = &dbc_api::instance();
	map<string, string> conn_info;
	AttributeProvider attribute(conn_info,configfile);
	conn_info["user_name"] = attribute.get("oracle_username");
	conn_info["password"] = attribute.get("oracle_password");
	conn_info["connect_string"] =attribute.get("oracle_connect_string");
	Generic_Database *temdb= new Oracle_Database();
	temdb->connect(conn_info);
	db=new DBTemplate(temdb,DBTYPE_ORACLE);

	conn_info["user_name"] = attribute.get("dbc_username");
	conn_info["password"] = attribute.get("dbc_password");
	temdb = new Dbc_Database();
	temdb->connect(conn_info);
	dbc=new DBTemplate(temdb,DBTYPE_DBC);
}
void DBCTest_Context::disconnect(){
	if (dbc) {
		delete dbc;
		delete db;
		db = dbc = 0;
	}
}
void DBCTest_Context::create(const string &configfile ) {
	if (!db) {
		startDbc();
		connect(configfile);

	}
}

void DBCTest_Context::destory() {
	if (dbc) {
		disconnect();
		stopDbc();
	}
}
bool DBCTest_Context::loadConfig(const string& filename, const string& configfile,
		map<string, string> &context) {
	destory();
	bool re = FileUtils::genfile(filename.c_str(), getenv("DBCPROFILE"), context,
			configfile.c_str());
	create(configfile);
	return re;
}
bool DBCTest_Context::loadFILE(const string& filename, const string& configfile){
	map<string,string> empty;
	AttributeProvider config(empty,configfile);
	FileUtils::copy(filename,config.get("dbc_test_load_FILE"));
	return true;
}
bool DBCTest_Context::loadConfig(const string& filename, const string& configfile){
	map<string, string> empty;
	return loadConfig(filename, configfile, empty);
}
int DBCTest_Context::getdb_count(const string& table_name, const string &where) {
	return db->rowcount(table_name,where);
}
int DBCTest_Context::getdbc_count(const string& table_name, const string & where) {
	return  dbc->rowcount(table_name,where);
}
} /* namespace dbc */
} /* namespace test */
} /* namespace ai */
