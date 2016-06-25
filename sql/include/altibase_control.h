#if !defined(__ALTIBASE_CONTROL_H__)
#define __ALTIBASE_CONTROL_H__

#include <sqlcli.h>
#include "sql_macro.h"
#include "sql_control.h"
#include "gpp_ctx.h"
#include "gpt_ctx.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;

class Altibase_ResultSet;
class Altibase_Statement;

class Altibase_Database : public Generic_Database
{
public:
	Altibase_Database();
	~Altibase_Database();

	dbtype_enum get_dbtype();
	void connect(const map<string, string>& conn_info);
	void disconnect();
	void commit();
	void rollback();
	Generic_Statement * create_statement();
	void terminate_statement(Generic_Statement *& stmt);

private:
	void alloc();
	void free();
	void getErrorMsg();

	SQLHENV hpEnv; // Environment Handle
	SQLHDBC hpDbc; // Connection Handle

	bool handleOpened;

	SQLINTEGER iErrorCode;
	SQLCHAR szErrorBuf[SQL_MAX_MESSAGE_LENGTH];
	SQLSMALLINT msgLength;

	gpp_ctx *GPP;
	gpt_ctx *GPT;

	friend class Altibase_Statement;
};

class Altibase_Statement : public Generic_Statement
{
public:
	dbtype_enum get_dbtype();
	int getUpdateCount() const;
	Generic_ResultSet * executeQuery();
	void executeUpdate();
	void executeArrayUpdate(int arrayLength);
	void closeResultSet(Generic_ResultSet *&rset);

	bool isNull(int colIndex);
	bool isTruncated(int colIndex);
	void setNull(int paramIndex);

	void setExecuteMode(execute_mode_enum execute_mode_);
	execute_mode_enum getExecuteMode() const;

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
	Altibase_Statement(Altibase_Database *db_);
	virtual ~Altibase_Statement();

	void prepare();
	void execute(int arrayLength);
    void clearNull(char *addr);
	void getErrorMsg();
	SQLSMALLINT genericToCType(int field_type);
	SQLSMALLINT genericToSqlType(int field_type);

	Altibase_Database *db;

	SQLHENV& hpEnv; // Environment Handle
	SQLHDBC& hpDbc; // Connection Handle

	SQLHSTMT *hpStmt;

	ub4 *pRowsError;
	int iRowsErrorCount;
	SQLUSMALLINT *pRowsErrorCode;
	//SQLUINTEGER update_count;
	SQLINTEGER update_count;

	execute_mode_enum execute_mode;

	SQLINTEGER iErrorCode;
	SQLCHAR szErrorBuf[SQL_MAX_MESSAGE_LENGTH];
	SQLSMALLINT msgLength;

	gpp_ctx *GPP;
	gpt_ctx *GPT;

	friend class Altibase_Database;
	friend class Altibase_ResultSet;
};

class Altibase_ResultSet : public Generic_ResultSet
{
public:
	dbtype_enum get_dbtype();
	Status next();
	Status batchNext();
	int getFetchRows() const;

	bool isNull(int colIndex);
	bool isTruncated(int paramIndex);
	//void clearNull(char *addr);

	date date2native(void *x) const;
	ptime time2native(void *x) const;
	datetime datetime2native(void *x) const;
	sql_datetime sqldatetime2native(void *x) const;

private:
	Altibase_ResultSet(Altibase_Statement *stmt_);
	virtual ~Altibase_ResultSet();

	void getErrorMsg();


	Altibase_Statement *stmt;
	SQLHDBC *hpDbc;
	//OCIError *& hpError;		// the error handle
	SQLHSTMT *hpStmt;

	SQLUINTEGER fetched_rows;		// Ô¤ÌáÈ¡Êý
	SQLUINTEGER total_rows;
	Status batch_status;

	SQLCHAR szErrorBuf[SQL_MAX_MESSAGE_LENGTH];
	SQLINTEGER iErrorCode;
	SQLSMALLINT msgLength;

	gpp_ctx *GPP;
	gpt_ctx *GPT;

	friend class Altibase_Statement;
};

}
}

#endif

