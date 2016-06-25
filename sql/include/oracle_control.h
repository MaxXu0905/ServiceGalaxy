#if !defined(__ORACLE_CONTROL_H__)
#define __ORACLE_CONTROL_H__

#include "oci.h"
#include "sql_macro.h"
#include "sql_control.h"
#include "gpp_ctx.h"
#include "gpt_ctx.h"
#include "common.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;

class Oracle_ResultSet;
class Oracle_Statement;

class Oracle_Database : public Generic_Database
{
public:
	Oracle_Database();
	// Used for external connection, OCIServer and OCISession object can be null, but some functionality may restricted.
	Oracle_Database(OCIEnv *hpEnv_, OCIServer *hpServer_, OCIError *hpError_, OCISession *hpSession_, OCISvcCtx *hpService_);
	~Oracle_Database();

	dbtype_enum get_dbtype();
	void connect(const map<string, string>& conn_info);
	void disconnect();
	void commit();
	void rollback();
	Generic_Statement * create_statement();
	void terminate_statement(Generic_Statement *& stmt);

	void setNonblocking();
	bool isNonblocking();
	void cancelNonblockingCall();

	void getXAConnection(text *dbname);
	void releaseXAConnection();

private:
	void alloc();
	void free();
	void attach(const char *szServerName);
	void detach();
	void beginSession();
	void endSession();
	void login(const char *szUserName, const char *szAuthentication);
	void getErrorMsg();

	OCIEnv *hpEnv;			// the environment handle
	OCIServer *hpServer;		// the server handle
	OCIError *hpError;		// the error handle
	OCISession *hpSession;	// user session handle
	OCISvcCtx *hpService;	// the  service handle

	bool externalConnected;
	bool xaEnabled;
	bool handleOpened;
	bool sessionOpened;
	bool serverOpened;
	bool nonblocking;
	DWORD dwLoginTimeout;
	DWORD dwQueryTimeout;
	HSTMT hstmt;

	text szErrorBuf[SQL_MAX_MESSAGE_LENGTH];
	sb4 iErrorCode;

	gpp_ctx *GPP;
	gpt_ctx *GPT;

	friend class Oracle_Statement;
};

class Oracle_Statement : public Generic_Statement
{
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
	Oracle_Statement(Oracle_Database *db_);
	virtual ~Oracle_Statement();

	void prepare();
	void execute(int arrayLength);
	void getErrorMsg();
	ub2 genericToOracleType(int field_type);
	void clearNull(char *addr);
	void describe();

	Oracle_Database *db;

	OCIEnv *& hpEnv;		// the environment handle
	OCIError *& hpError;		// the error handle
	OCISvcCtx *& hpService;	// the  service handle

	OCIError *hpRowsError;	// the rows error handle in OCI_BATCH_ERRORS mode
	OCIStmt *hpStmt;

	OCIDefine **hppDefine;
	OCIBind **hppBind;
	ub4 *pRowsError;
	int iRowsErrorCount;
	sb4 *pRowsErrorCode;

	ub4 rowOffset;
	ub4 update_count;

	// 提交模式，可取值为:
	// OCI_DEFAULT
	// OCI_DESCRIBE_ONLY
	// OCI_COMMIT_ON_SUCCESS
	// OCI_EXACT_FETCH
	// OCI_BATCH_ERRORS
	// 默认为OCI_DEFAULT
	ub4 execute_mode;
	execute_mode_enum generic_execute_mode;

	text szErrorBuf[SQL_MAX_MESSAGE_LENGTH];
	string errorString;
	sb4 iErrorCode;

	gpp_ctx *GPP;
	gpt_ctx *GPT;

	friend class Oracle_Database;
	friend class Oracle_ResultSet;
};

class Oracle_ResultSet : public Generic_ResultSet
{
public:
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
	Oracle_ResultSet(Oracle_Statement *stmt_);
	virtual ~Oracle_ResultSet();

	void getErrorMsg();

	Oracle_Statement *stmt;
	OCIStmt *& hpStmt;
	OCIError *& hpError;		// the error handle

	ub4 fetched_rows;		// 预提取数
	ub4 total_rows;
	Status batch_status;

	text szErrorBuf[SQL_MAX_MESSAGE_LENGTH];
	string errorString;
	sb4 iErrorCode;

	gpp_ctx *GPP;
	gpt_ctx *GPT;

	friend class Oracle_Statement;
};

}
}

#endif

