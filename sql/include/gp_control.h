/*
 * gp_control.h
 *
 *  Created on: 2012-8-28
 *      Author: Administrator
 */

#ifndef GPCONTROL_H_
#define GPCONTROL_H_
#include "boost/scoped_array.hpp"
#include "common.h"
#include "libpq-fe.h"
#include "sql_macro.h"
#include "sql_control.h"
#include "gpp_ctx.h"
#include "gpt_ctx.h"
#include "common.h"
namespace ai {
namespace scci {

const char gp_sql_begin[] = "BEGIN";
const char gp_sql_commit[] = "COMMIT";
const char gp_sql_rollback[] = "ROLLBACK";
const char gp_sql_deallcate[]="DEALLOCATE "; //空格是为了后面加参数
const char gp_sql_datestyle[]="set DateStyle=ISO,YMD";
const char gp_sql_search_path[]="set search_path=\"$user\",";


using namespace ai::sg;
using namespace std;
using boost::scoped_array;
class GP_ResultSet;
class GP_Statement;
class GP_Database: public Generic_Database {
public:
	GP_Database();
	virtual ~GP_Database();
	void connect(const map<string, string>& conn_info);
	void disconnect();
	void commit();
	void rollback();
	Generic_Statement * create_statement();
	void terminate_statement(Generic_Statement *& stmt);
	dbtype_enum get_dbtype();
	void execCmd(const string &command);
	// 获取序列的选择项表达式
	virtual string get_seq_item(const string& seq_name, int seq_action);
	void begin();
private:
	void prepare(const string &statementName, const string &sql);
	PGresult * prepare_exec(const string &statementName,
			const char * const params[], const int paramlengths[],
			const int binary[], int nparams);

	PGconn* _conn;
	int64_t statmentId;
	bool begined;
	friend class GP_Statement;

};
class GP_Statement: public Generic_Statement {
public:
	dbtype_enum get_dbtype();
	int getUpdateCount() const;
	Generic_ResultSet * executeQuery();
	void executeUpdate();
	void executeArrayUpdate(int arrayLength);
	void closeResultSet(Generic_ResultSet *& rset);
	bool isNull(int colIndex);
	bool isTruncated(int colIndex);
	void setNull(int paramIndex);

	void setExecuteMode(execute_mode_enum execute_mode_);
	execute_mode_enum getExecuteMode() const;

	// Not available for generic interface
	sb4 checkNonblockingResult();

	date date2native(void *x) const;
	ptime time2native(void *x) const;
	datetime datetime2native(void *x) const;
	sql_datetime sqldatetime2native(void *x) const;
	void native2date(void *addr, const date& x) const;
	void native2time(void *addr, const ptime& x) const;
	void native2datetime(void *addr, const datetime& x) const;
	void native2sqldatetime(void *addr, const sql_datetime& x) const;
	void setRowOffset(ub4 rowOffset_);
	int getRowsErrorCount() const;
	ub4 * getRowsError();
	sb4 * getRowsErrorCode();

private:
	GP_Statement(GP_Database *db_, const string &statmentName);
	virtual ~GP_Statement();
	void pushPrepareData(bind_field_t * bind_field_, int bind_count_,
			int index = 0);
	int marshall(scoped_array<const char *> &values, scoped_array<int> &lengths,
			scoped_array<int> &binaries);
	void execute(int arrayLength);
	virtual void prepare();
	virtual void clearNull(char *addr);
	friend class GP_Database;
	friend class GP_ResultSet;
	GP_Database *_db;
	string _statmentName;
	ub4 *pRowsError;
	int iRowsErrorCount;
	sb4 *pRowsErrorCode;
	ub4 rowOffset;
	int _updateCount;
	vector<string> _values;
	vector<bool> _nonnull;
	vector<bool> _binary;

};

class GP_ResultSet: public Generic_ResultSet {
public:
	virtual ~GP_ResultSet();
	dbtype_enum get_dbtype();
	Status next();
	Status batchNext();
	int getFetchRows() const;
	bool isNull(int colIndex);
	bool isTruncated(int paramIndex);
	date date2native(void *x) const;
	ptime time2native(void *x) const;
	datetime datetime2native(void *x) const;
	sql_datetime sqldatetime2native(void *x) const;
private:
	GP_ResultSet(PGresult *_res);
	PGresult *_res;
	int _fetched_rows;
	//总行数
	int _curRow;
	int _rowCount;
	friend class GP_Statement;
};

} /* namespace scci */
} /* namespace ai */
#endif /* GPCONTROL_H_ */
