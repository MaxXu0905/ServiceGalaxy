#include "oracle_control.h"
#include "user_exception.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;

Oracle_Database::Oracle_Database()
{
	hpEnv = NULL;
	hpServer = NULL;
	hpError = NULL;
	hpSession = NULL;
	hpService = NULL;

	externalConnected = false;
	handleOpened = false;
	sessionOpened = false;
	serverOpened = false;
	nonblocking = false;
	//renyz 增加默任值
	xaEnabled = false;
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	alloc();
}

Oracle_Database::Oracle_Database(OCIEnv *hpEnv_, OCIServer *hpServer_, OCIError *hpError_,
	OCISession *hpSession_, OCISvcCtx *hpService_)
	: hpEnv(hpEnv_),
	  hpServer(hpServer_),
	  hpError(hpError_),
	  hpSession(hpSession_),
	  hpService(hpService_)
{
	externalConnected = true;
	xaEnabled = false;
	handleOpened = true;
	sessionOpened = true;
	serverOpened = true;
	nonblocking = false;

	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	if (hpEnv == NULL)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: bad param given, OCIEnv object is null")).str(SGLOCALE));
	if (hpError == NULL)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: bad param given, OCIError object is null")).str(SGLOCALE));
	if (hpService == NULL)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: bad param given, OCISvcCtx object is null")).str(SGLOCALE));
}

Oracle_Database::~Oracle_Database()
{
	if (xaEnabled)
		releaseXAConnection();
	else if (!externalConnected)
		disconnect();
}

dbtype_enum Oracle_Database::get_dbtype()
{
	return DBTYPE_ORACLE;
}

void Oracle_Database::connect(const map<string, string>& conn_info)
{
	map<string, string>::const_iterator iter;

	iter = conn_info.find("user_name");
	if (iter == conn_info.end())
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: missing user_name")).str(SGLOCALE));
	string user_name = iter->second;

	iter = conn_info.find("password");
	if (iter == conn_info.end())
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: missing password")).str(SGLOCALE));
	string password = iter->second;

	iter = conn_info.find("connect_string");
	if (iter == conn_info.end())
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: missing connect_string")).str(SGLOCALE));
	string connect_string = iter->second;

	alloc();
	attach(connect_string.c_str());
	login(user_name.c_str(), password.c_str());
}

void Oracle_Database::disconnect()
{
	if (!externalConnected) {
		endSession();
		detach();
		free();
	}
}

void Oracle_Database::commit()
{
	int ret_code = OCITransCommit(hpService, hpError, OCI_DEFAULT);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

void Oracle_Database::rollback()
{
	int ret_code = OCITransRollback(hpService, hpError, OCI_DEFAULT);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

Generic_Statement * Oracle_Database::create_statement()
{
	return dynamic_cast<Generic_Statement *>(new Oracle_Statement(this));
}

void Oracle_Database::terminate_statement(Generic_Statement *& stmt)
{
	if (stmt) {
		delete stmt;
		stmt = NULL;
	}
}

// 数据库连接时默认状态为BlockingMode，
// 如果改变了，就不能再改回来
void Oracle_Database::setNonblocking()
{
	if (hpServer == NULL)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: OCIServer object is null, functionality not supported")).str(SGLOCALE));

	// 如果不是非阻塞方式，则设为非阻塞方式
	if (!nonblocking) {
		int ret_code = OCIAttrSet(reinterpret_cast<dvoid *>(hpServer), static_cast<ub4>(OCI_HTYPE_SERVER),
			NULL, static_cast<ub4>(0), static_cast<ub4>(OCI_ATTR_NONBLOCKING_MODE), hpError);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		nonblocking = true;
	}
}

bool Oracle_Database::isNonblocking()
{
	return nonblocking;
}

// 在非阻塞方式下取消一个长时间运行的操作
void Oracle_Database::cancelNonblockingCall()
{
	int ret_code;

	if (hpServer == NULL)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: OCIServer object is null, functionality not supported")).str(SGLOCALE));

	if (!nonblocking)
		return;

	// cancel a long-running OCI call
	ret_code = OCIBreak(hpServer, hpError);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	//
	// After issuing an OCIBreak() while an OCI call is
	// in progress, you must issue an OCIReset() call to
	// reset the asynchronous operation and protocol.
	//
	// But my current OCI version(8.0.4) doesn't supprot
	// the OCIReset invocation
	//
	ret_code = OCIReset(hpServer, hpError);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

void Oracle_Database::getXAConnection(text *dbname)
{
	hpEnv = xaoEnv(dbname);
	hpService = xaoSvcCtx(dbname);

	int ret_code = OCIHandleAlloc(reinterpret_cast<dvoid *>(hpEnv),
		reinterpret_cast<dvoid **>(&hpError), OCI_HTYPE_ERROR, 0, NULL);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	xaEnabled = true;
}

void Oracle_Database::releaseXAConnection()
{
	int ret_code = OCIHandleFree(reinterpret_cast<dvoid *>(hpError), static_cast<ub4>(OCI_HTYPE_ERROR));
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	xaEnabled = false;
}

void Oracle_Database::alloc()
{
	if (!handleOpened) {
		int ret_code;

		ret_code = OCIInitialize(OCI_THREADED, NULL, NULL, NULL, NULL);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIEnvInit(&hpEnv, OCI_DEFAULT, 0, NULL);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

 		ret_code = OCIHandleAlloc(reinterpret_cast<dvoid *>(hpEnv),
			reinterpret_cast<dvoid **>(&hpServer), OCI_HTYPE_SERVER, 0, NULL);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIHandleAlloc(reinterpret_cast<dvoid *>(hpEnv),
			reinterpret_cast<dvoid **>(&hpError), OCI_HTYPE_ERROR, 0, NULL);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIHandleAlloc(reinterpret_cast<dvoid *>(hpEnv),
			reinterpret_cast<dvoid **>(&hpService), OCI_HTYPE_SVCCTX, 0, NULL);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIAttrSet(reinterpret_cast<dvoid *>(hpService), OCI_HTYPE_SVCCTX,
			reinterpret_cast<dvoid *>(hpServer), static_cast<ub4>(0), OCI_ATTR_SERVER, hpError);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIHandleAlloc(reinterpret_cast<dvoid *>(hpEnv),
			reinterpret_cast<dvoid **>(&hpSession), OCI_HTYPE_SESSION, 0, NULL);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		handleOpened = true;
	}
}

void Oracle_Database::free()
{
	if (handleOpened) {
		int ret_code;

		ret_code = OCIHandleFree(reinterpret_cast<dvoid *>(hpServer), static_cast<ub4>(OCI_HTYPE_SERVER));
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIHandleFree(reinterpret_cast<dvoid *>(hpService), static_cast<ub4>(OCI_HTYPE_SVCCTX));
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIHandleFree(reinterpret_cast<dvoid *>(hpSession), static_cast<ub4>(OCI_HTYPE_SESSION));
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIHandleFree(reinterpret_cast<dvoid *>(hpError), static_cast<ub4>(OCI_HTYPE_ERROR));
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIHandleFree(reinterpret_cast<dvoid *>(hpEnv), static_cast<ub4>(OCI_HTYPE_ENV));
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		handleOpened = false;
	}
}

void Oracle_Database::attach(const char *szServerName)
{
	if (!serverOpened) {
		int ret_code;

		ret_code = OCIServerAttach(hpServer, hpError, reinterpret_cast<const text *>(szServerName), ::strlen(szServerName), OCI_DEFAULT);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		/* We may output version info in the future
		 * char szVersion[1024];
		 * const int nVersionLength = 1024;
		 * OCIServerVersion(hpServer, hpError, reinterpret_cast<OraText *>(szVersion), nVersionLength, OCI_HTYPE_SERVER);
		 * GPP->write_log(__FILE__, __LINE__, szVersion);
		*/

		serverOpened = true;
	}
}

void Oracle_Database::detach()
{
	if (serverOpened) {
		int ret_code = OCIServerDetach(hpServer, hpError, OCI_DEFAULT);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		serverOpened = false;
	}
}

void Oracle_Database::beginSession()
{
	if (!sessionOpened) {
		int ret_code;

		ret_code = OCISessionBegin(hpService, hpError, hpSession, OCI_CRED_RDBMS, OCI_DEFAULT);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = OCIAttrSet(reinterpret_cast<dvoid *>(hpService), OCI_HTYPE_SVCCTX,
		   	reinterpret_cast<dvoid *>(hpSession), static_cast<ub4>(0), OCI_ATTR_SESSION, hpError);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		sessionOpened = true;
	}
}

void Oracle_Database::endSession()
{
	if (sessionOpened) {
		int ret_code = OCISessionEnd(hpService, hpError, hpSession, OCI_DEFAULT);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		sessionOpened = false;
	}
}

void Oracle_Database::login(const char*szUserName, const char*szAuthentication)
{
	int ret_code;

	ret_code = OCIAttrSet(reinterpret_cast<dvoid *>(hpSession), OCI_HTYPE_SESSION,
		reinterpret_cast<dvoid *>(const_cast<char *>(szUserName)),
		static_cast<ub4>(::strlen(szUserName)), OCI_ATTR_USERNAME, hpError);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	ret_code = OCIAttrSet(reinterpret_cast<dvoid *>(hpSession), OCI_HTYPE_SESSION,
		reinterpret_cast<dvoid *>(const_cast<char *>(szAuthentication)),
		static_cast<ub4>(::strlen(szAuthentication)), OCI_ATTR_PASSWORD, hpError);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	beginSession();
}

void Oracle_Database::getErrorMsg()
{
	OCIErrorGet(reinterpret_cast<dvoid *>(hpError),
		static_cast<ub4>(1),
		reinterpret_cast<text *>(NULL),
		&iErrorCode,
		reinterpret_cast<text*>(szErrorBuf),
		static_cast<ub4>(sizeof(szErrorBuf)),
		OCI_HTYPE_ERROR);
}

Oracle_Statement::Oracle_Statement(Oracle_Database *db_)
	: db(db_),
	  hpEnv(db_->hpEnv),
	  hpError(db_->hpError),
	  hpService(db_->hpService)
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	hpRowsError = NULL;
	hpStmt = NULL;

	hppDefine = NULL;
	hppBind = NULL;
	pRowsError = NULL;
	iRowsErrorCount = 0;
	pRowsErrorCode = NULL;

	rowOffset = 0;
	update_count = 0;
	execute_mode = OCI_DEFAULT;
	prepared = false;

	int ret_code = OCIHandleAlloc(reinterpret_cast<dvoid *>(hpEnv),
		reinterpret_cast<dvoid **>(&hpRowsError), OCI_HTYPE_ERROR, 0, NULL);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
	}

	ret_code = OCIHandleAlloc(reinterpret_cast<dvoid *>(hpEnv), reinterpret_cast<dvoid **>(&hpStmt),
		OCI_HTYPE_STMT, static_cast<size_t>(0), NULL);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
	}
}

Oracle_Statement::~Oracle_Statement()
{
	delete []pRowsErrorCode;
	delete []pRowsError;
	delete []hppBind;
	delete []hppDefine;

	int ret_code = OCIHandleFree(reinterpret_cast<dvoid *>(hpRowsError), static_cast<ub4>(OCI_HTYPE_ERROR));
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
	}

	ret_code = OCIHandleFree(hpStmt, OCI_HTYPE_STMT);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
	}
}

dbtype_enum Oracle_Statement::get_dbtype()
{
	return DBTYPE_ORACLE;
}

int Oracle_Statement::getUpdateCount() const
{
	return update_count;
}

Generic_ResultSet * Oracle_Statement::executeQuery()
{
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0, (_("ERROR: bind() should be called before executeQuery()")).str(SGLOCALE));

	execute(data->size());
	Oracle_ResultSet *grset = new Oracle_ResultSet(this);
	grset->bind(data, index);
	return dynamic_cast<Generic_ResultSet *>(grset);
}

void Oracle_Statement::executeUpdate()
{
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0, (_("ERROR: bind() should be called before executeUpdate()")).str(SGLOCALE));

	execute(iteration + 1);
	iteration = 0;
}

void Oracle_Statement::executeArrayUpdate(int arrayLength)
{
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0, (_("ERROR: bind() should be called before executeArrayUpdate()")).str(SGLOCALE));

	execute(arrayLength);
}

void Oracle_Statement::closeResultSet(Generic_ResultSet *& rset)
{
	if (rset) {
		Oracle_ResultSet *grset = dynamic_cast<Oracle_ResultSet *>(rset);
		delete grset;
		rset = NULL;
	}
}

bool Oracle_Statement::isNull(int colIndex)
{
	if (colIndex <= 0 || colIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		sb2 *ind = reinterpret_cast<sb2 *>(reinterpret_cast<char *>(data) + field.ind_offset);
		if (ind[iteration] == -1)
			return true;
		else
			return false;
	}
}

bool Oracle_Statement::isTruncated(int colIndex)
{
	if (colIndex <= 0 || colIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		sb2 *ind = reinterpret_cast<sb2 *>(reinterpret_cast<char *>(data) + field.ind_offset);
		if (ind[iteration] > 0)
			return true;
		else
			return false;
	}
}

void Oracle_Statement::setNull(int paramIndex)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset == -1) // No indicator
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Field indicator is not defined")).str(SGLOCALE));

	reinterpret_cast<sb2 *>(reinterpret_cast<char *>(data) + field.ind_offset)[iteration] = -1;
}

void Oracle_Statement::prepare()
{
	if (prepared)
		return;

	int ret_code;
	string sql_str;
	int array_size = data->size();

	if (!newTable.empty()) {
		int pos = 0;
		string tmp_str = data->getSQL(index);

		for (const table_field_t *table_fields = data->getTableFields(index); table_fields->pos != -1; ++table_fields) {
			if (oldTable.empty() || table_fields->table_name == oldTable) {
				sql_str += tmp_str.substr(pos, table_fields->pos);
				sql_str += newTable;
				pos = table_fields->pos + table_fields->table_name.length();
			}
		}
		sql_str += tmp_str.substr(pos);
	} else {
		sql_str = data->getSQL(index);
	}

	ret_code = OCIStmtPrepare(hpStmt, hpError, reinterpret_cast<const text *>(sql_str.c_str()),
		static_cast<ub4>(sql_str.length()), OCI_NTV_SYNTAX, OCI_DEFAULT);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
	}

	if (select_count > 0 && select_fields[0].field_type == SQLTYPE_ANY)
		describe();

	if (pRowsError)
		delete []pRowsError;
	pRowsError = new ub4[array_size];

	if (pRowsErrorCode)
		delete []pRowsErrorCode;
	pRowsErrorCode = new sb4[array_size];

	if (select_count) { // 是选择语句
		if (hppDefine)
			delete []hppDefine;
		hppDefine = new OCIDefine *[select_count];

		for (int i = 0; i < select_count; i++) {
			ret_code = OCIDefineByPos(hpStmt,
				&hppDefine[i],
				hpError,
				static_cast<ub4>(i + 1),
				reinterpret_cast<dvoid *>(reinterpret_cast<char *>(data) + select_fields[i].field_offset),
				static_cast<sb4>(select_fields[i].field_length),
				genericToOracleType(select_fields[i].field_type),
				select_fields[i].ind_offset == -1 ? NULL : reinterpret_cast<dvoid *>(reinterpret_cast<char *>(data) + select_fields[i].ind_offset),
				NULL,
				NULL,
				OCI_DEFAULT);
			if (ret_code != OCI_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
			}
		}
	}

	if (bind_count) { // 需要绑定变量
		if (hppBind)
			delete hppBind;
		hppBind = new OCIBind *[bind_count];

		for (int i = 0; i < bind_count; i++) {
			// 必须使用该函数，而不能用OCIBindByPos，这样同名的只需绑定一次
			ret_code = OCIBindByName(hpStmt,
				&hppBind[i],
				hpError,
				reinterpret_cast<const text *>(bind_fields[i].field_name.c_str()),
				static_cast<sb4>(bind_fields[i].field_name.length()),
				reinterpret_cast<dvoid *>(reinterpret_cast<char *>(data) + bind_fields[i].field_offset),
				static_cast<sb4>(bind_fields[i].field_length),
				genericToOracleType(bind_fields[i].field_type),
				bind_fields[i].ind_offset == -1 ? NULL : reinterpret_cast<dvoid *>(reinterpret_cast<char *>(data) + bind_fields[i].ind_offset),
				NULL,
				NULL,
				static_cast<ub4>(0),
				NULL,
				OCI_DEFAULT);
			if (ret_code != OCI_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
			}
		}
	}

	prepared = true;
}

// 在调用之前需要设置提交模式
// OCI_DEFAULT
// OCI_DESCRIBE_ONLY
// OCI_COMMIT_ON_SUCCESS
// OCI_EXACT_FETCH
// OCI_BATCH_ERRORS
// 对no-select语句，实际执行次数为iRows-rowOffset
void Oracle_Statement::execute(int arrayLength)
{
	sb4 status;
	int ret_code;

	// 执行
	status = OCIStmtExecute(hpService, hpStmt, hpError,
		arrayLength, rowOffset, NULL, NULL, execute_mode);

	// 处理的行数
	ret_code = OCIAttrGet(reinterpret_cast<dvoid *>(hpStmt), OCI_HTYPE_STMT,
		reinterpret_cast<dvoid *>(&update_count), NULL, OCI_ATTR_ROW_COUNT, hpError);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
	}

	// 取得错误个数
	ret_code = OCIAttrGet(reinterpret_cast<dvoid *>(hpStmt), OCI_HTYPE_STMT,
		&iRowsErrorCount, NULL, OCI_ATTR_NUM_DML_ERRORS, hpError);

	if (iRowsErrorCount) { // 有错误记录
		// 取得错误记录的位置信息
		for (int i = 0; i < iRowsErrorCount; i++) {
			ret_code = OCIParamGet(reinterpret_cast<dvoid *>(hpError), OCI_HTYPE_ERROR,
				hpError, reinterpret_cast<dvoid **>(&hpRowsError), i);
			if (ret_code != OCI_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
			}

			ret_code = OCIAttrGet(reinterpret_cast<dvoid *>(hpRowsError), OCI_HTYPE_ERROR,
				&pRowsError[i], NULL, OCI_ATTR_DML_ROW_OFFSET, hpError);
			if (ret_code != OCI_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
			}

			ret_code = OCIErrorGet(reinterpret_cast<dvoid *>(hpRowsError), static_cast<ub4>(1),
				NULL, &pRowsErrorCode[i], NULL, static_cast<ub4>(0), OCI_HTYPE_ERROR);
			if (ret_code != OCI_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
			}
  		}
	}

	if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO && status != OCI_NO_DATA) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
	}
}

// 默认模式为OCI_DEFAULT
void Oracle_Statement::setExecuteMode(execute_mode_enum execute_mode_)
{
	generic_execute_mode = execute_mode_;

	switch (generic_execute_mode) {
	case EM_DESCRIBE_ONLY:
		execute_mode = OCI_DESCRIBE_ONLY;
		break;
	case EM_COMMIT_ON_SUCCESS:
		execute_mode = OCI_COMMIT_ON_SUCCESS;
		break;
	case EM_EXACT_FETCH:
		execute_mode = OCI_EXACT_FETCH;
		break;
	case EM_BATCH_ERRORS:
		execute_mode = OCI_BATCH_ERRORS;
		break;
	case EM_DEFAULT:
	default:
		execute_mode = OCI_DEFAULT;
		break;
	}
}

execute_mode_enum Oracle_Statement::getExecuteMode() const
{
	return generic_execute_mode;
}

sb4 Oracle_Statement::checkNonblockingResult()
{
	try {
		int ret_code = OCIStmtExecute(hpService, hpStmt, hpError, static_cast<ub4>(1),
			static_cast<ub4>(0), NULL, NULL, execute_mode);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
		}

		OCIHandleFree(hpStmt, OCI_HTYPE_STMT);
		hpStmt = NULL;
		return 0;
	} catch(bad_sql& ex) {
		if (db->isNonblocking()) {
			if(ex.get_error_code() == OCI_STILL_EXECUTING)
				return OCI_STILL_EXECUTING;
		}

		OCIHandleFree(hpStmt, OCI_HTYPE_STMT);
		hpStmt = NULL;
		throw;
	}
}

void Oracle_Statement::getErrorMsg()
{
	OCIErrorGet(reinterpret_cast<dvoid *>(hpError),
		static_cast<ub4>(1),
		reinterpret_cast<text *>(NULL),
		&iErrorCode,
		szErrorBuf,
		static_cast<ub4>(sizeof(szErrorBuf)),
		OCI_HTYPE_ERROR);

	errorString = reinterpret_cast<char *>(szErrorBuf);
	if (data != NULL) {
		if (*errorString.rbegin() != '\n')
			errorString += "\n";
		errorString += "SQL is ";
		errorString += data->getSQL(index);
	}
}

ub2 Oracle_Statement::genericToOracleType(int field_type)
{
	switch (field_type) {
	case SQLTYPE_CHAR:
	case SQLTYPE_UCHAR:
		return SQLT_CHR;
	case SQLTYPE_SHORT:
	case SQLTYPE_INT:
	case SQLTYPE_LONG:
		return SQLT_INT;
	case SQLTYPE_USHORT:
	case SQLTYPE_UINT:
	case SQLTYPE_ULONG:
		return SQLT_UIN;
	case SQLTYPE_FLOAT:
	case SQLTYPE_DOUBLE:
		return SQLT_FLT;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return SQLT_STR;
	case SQLTYPE_DATE:
		return SQLT_ODT;
	case SQLTYPE_TIME:
		return SQLT_ODT;
	case SQLTYPE_DATETIME:
		return SQLT_ODT;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field_type")).str(SGLOCALE));
	}
}

void Oracle_Statement::clearNull(char *addr)
{
	reinterpret_cast<sb2 *>(addr)[iteration] = 0;
}

date Oracle_Statement::date2native(void *x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(x);
	return date(dt->OCIDateYYYY, dt->OCIDateMM, dt->OCIDateDD);
}

ptime Oracle_Statement::time2native(void *x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(x);
	return ptime(dt->OCIDateTime.OCITimeHH, dt->OCIDateTime.OCITimeMI, dt->OCIDateTime.OCITimeSS);
}

datetime Oracle_Statement::datetime2native(void *x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(x);
	return datetime(dt->OCIDateYYYY, dt->OCIDateMM, dt->OCIDateDD,
		dt->OCIDateTime.OCITimeHH, dt->OCIDateTime.OCITimeMI, dt->OCIDateTime.OCITimeSS);
}

sql_datetime Oracle_Statement::sqldatetime2native(void *x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(x);
	return sql_datetime(dt->OCIDateYYYY, dt->OCIDateMM, dt->OCIDateDD,
		dt->OCIDateTime.OCITimeHH, dt->OCIDateTime.OCITimeMI, dt->OCIDateTime.OCITimeSS);
}

void Oracle_Statement::native2date(void *addr, const date& x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(addr);
	dt->OCIDateYYYY = x.year();
	dt->OCIDateMM = x.month();
	dt->OCIDateDD = x.day();
}

void Oracle_Statement::native2time(void *addr, const ptime& x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(addr);
	dt->OCIDateYYYY = 0;
	dt->OCIDateMM = 0;
	dt->OCIDateDD = 0;
	dt->OCIDateTime.OCITimeHH = x.hour();
	dt->OCIDateTime.OCITimeMI = x.minute();
	dt->OCIDateTime.OCITimeSS = x.second();
}

void Oracle_Statement::native2datetime(void *addr, const datetime& x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(addr);
	dt->OCIDateYYYY = x.year();
	dt->OCIDateMM = x.month();
	dt->OCIDateDD = x.day();
	dt->OCIDateTime.OCITimeHH = x.hour();
	dt->OCIDateTime.OCITimeMI = x.minute();
	dt->OCIDateTime.OCITimeSS = x.second();
}

void Oracle_Statement::native2sqldatetime(void *addr, const sql_datetime& x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(addr);
	dt->OCIDateYYYY = x.year();
	dt->OCIDateMM = x.month();
	dt->OCIDateDD = x.day();
	dt->OCIDateTime.OCITimeHH = x.hour();
	dt->OCIDateTime.OCITimeMI = x.minute();
	dt->OCIDateTime.OCITimeSS = x.second();
}

void Oracle_Statement::setRowOffset(ub4 rowOffset_)
{
	rowOffset = rowOffset_;
}

int Oracle_Statement::getRowsErrorCount() const
{
	return iRowsErrorCount;
}

ub4 * Oracle_Statement::getRowsError()
{
	return pRowsError;
}

sb4 * Oracle_Statement::getRowsErrorCode()
{
	return pRowsErrorCode;
}

void Oracle_Statement::describe()
{
	sb4 status;
	int ret_code;
	int param_count;
	OCIParam* hpCol;
	ub2 type;
	ub2 size;
	sb2 precision;
	ub1 scale;

	// 执行
	status = OCIStmtExecute(hpService, hpStmt, hpError, 0, 0, NULL, NULL, OCI_DESCRIBE_ONLY);

	// 处理的行数
	ret_code = OCIAttrGet(reinterpret_cast<dvoid *>(hpStmt), OCI_HTYPE_STMT,
		reinterpret_cast<dvoid *>(&param_count), NULL, OCI_ATTR_PARAM_COUNT, hpError);
	if (ret_code != OCI_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
	}

	for (int i = 0; i < param_count; i++) {
		ret_code = OCIParamGet(hpStmt, OCI_HTYPE_STMT, hpError, reinterpret_cast<void **>(&hpCol), i + 1);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
		}

		ret_code = OCIAttrGet(hpCol, OCI_DTYPE_PARAM, reinterpret_cast<dvoid *>(&type), 0, OCI_ATTR_DATA_TYPE, hpError);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
		}

		ret_code = OCIAttrGet(hpCol, OCI_DTYPE_PARAM, reinterpret_cast<dvoid *>(&size), 0, OCI_ATTR_DATA_SIZE, hpError);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
		}

		ret_code = OCIAttrGet(hpCol, OCI_DTYPE_PARAM, reinterpret_cast<dvoid *>(&precision), 0, OCI_ATTR_PRECISION, hpError);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
		}

		ret_code = OCIAttrGet(hpCol, OCI_DTYPE_PARAM, reinterpret_cast<dvoid *>(&scale), 0, OCI_ATTR_SCALE, hpError);
		if (ret_code != OCI_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
		}

		switch (type) {
		case SQLT_CHR: // varchar2 varchar
		case SQLT_AFC: // char
			select_fields[i].field_type = SQLTYPE_STRING;
			select_fields[i].field_length = size + 1;
			break;
		case SQLT_ODT:
		case SQLT_DAT: //date
			select_fields[i].field_type = SQLTYPE_DATETIME;
			select_fields[i].field_length = sizeof(OCIDate);
			break;
		case SQLT_NUM: // number
			if (scale == 0 && precision >=0 && precision <= 9) {
				select_fields[i].field_type = SQLTYPE_INT;
				select_fields[i].field_length = sizeof(int);
			} else {
				select_fields[i].field_type = SQLTYPE_DOUBLE;
				select_fields[i].field_length = sizeof(double);
			}
			break;
		default:
			throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: Unsupported data type given")).str(SGLOCALE));
		}
	}

	data->alloc_select();
}

Oracle_ResultSet::Oracle_ResultSet(Oracle_Statement *stmt_)
	: stmt(stmt_),
	  hpStmt(stmt_->hpStmt),
	  hpError(stmt_->hpError)
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	fetched_rows = 0;
	batch_status = FIRST_DATA_CALL;
	iteration = -1;
}

Oracle_ResultSet::~Oracle_ResultSet()
{
}

dbtype_enum Oracle_ResultSet::get_dbtype()
{
	return DBTYPE_ORACLE;
}

Oracle_ResultSet::Status Oracle_ResultSet::next()
{
	if (++iteration < fetched_rows)
		return DATA_AVAILABLE;

	if (batch_status) {
		batchNext();
		if (fetched_rows == 0) {
			return END_OF_FETCH;
		} else {
			iteration = 0;
			return DATA_AVAILABLE;
		}
	} else {
		return END_OF_FETCH;
	}
}

Oracle_ResultSet::Status Oracle_ResultSet::batchNext()
{
	int ret_code;
	int array_size = data->size();

	if (batch_status == END_OF_FETCH) { // 已到末尾
		fetched_rows = 0;
	} else if (batch_status == FIRST_DATA_CALL) { // 第一次提取，在执行时预提取了部分数据
		fetched_rows = stmt->getUpdateCount();
		total_rows = fetched_rows;
		if (fetched_rows == array_size)
			batch_status = DATA_AVAILABLE;
		else
			batch_status = END_OF_FETCH;
	} else { // 其它情况
		sb4 status = OCIStmtFetch(hpStmt, hpError, array_size, OCI_FETCH_NEXT, OCI_DEFAULT);
		if (status == OCI_NO_DATA) {
			ub4 rows;

			OCIErrorGet(reinterpret_cast<dvoid *>(hpError),
				static_cast<ub4>(1),
				reinterpret_cast<text *>(NULL),
				&iErrorCode,
				reinterpret_cast<text*>(szErrorBuf),
				static_cast<ub4>(sizeof(szErrorBuf)),
				OCI_HTYPE_ERROR);
			if (iErrorCode == 24345) { // A truncation or NULL fetch error occurred
				errorString = reinterpret_cast<char *>(szErrorBuf);
				if (data != NULL) {
					if (*errorString.rbegin() != '\n')
						errorString += "\n";
					errorString += "SQL is ";
					errorString += data->getSQL(index);
				}
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
			}

			ret_code = OCIAttrGet(reinterpret_cast<dvoid *>(hpStmt), OCI_HTYPE_STMT,
				reinterpret_cast<dvoid *>(&rows), NULL, OCI_ATTR_ROW_COUNT, hpError);
			if (ret_code != OCI_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
			}

			fetched_rows = rows - total_rows;
			total_rows = rows;
			batch_status = END_OF_FETCH;
		} else {
			fetched_rows = array_size;
			total_rows += fetched_rows;
			if (status != OCI_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, errorString);
			}
			batch_status = DATA_AVAILABLE;
		}
	}

	// 对null数据进行标准化
	for (int i = 0; i < select_count; i++) {
		if (select_fields[i].ind_offset == -1)
			continue;

		sb2 *inds = reinterpret_cast<sb2 *>(reinterpret_cast<char *>(data) + select_fields[i].ind_offset);
		for (int j = 0; j < fetched_rows; j++) {
			if (inds[j] == -1) { // null
				switch (select_fields[i].field_type) {
				case SQLTYPE_STRING:
				case SQLTYPE_VSTRING:
					(reinterpret_cast<char *>(data) + select_fields[i].field_offset + j * select_fields[i].field_length)[0] = '\0';
					break;
				case SQLTYPE_CHAR:
				case SQLTYPE_UCHAR:
					(reinterpret_cast<char *>(data) + select_fields[i].field_offset)[j] = '\0';
					break;
				case SQLTYPE_SHORT:
					reinterpret_cast<short *>(reinterpret_cast<char *>(data) + select_fields[i].field_offset)[j] = 0;
					break;
				case SQLTYPE_USHORT:
					reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(data) + select_fields[i].field_offset)[j] = 0;
					break;
				case SQLTYPE_INT:
					reinterpret_cast<int *>(reinterpret_cast<char *>(data) + select_fields[i].field_offset)[j] = 0;
					break;
				case SQLTYPE_UINT:
					reinterpret_cast<unsigned int *>(reinterpret_cast<char *>(data) + select_fields[i].field_offset)[j] = 0;
					break;
				case SQLTYPE_LONG:
					reinterpret_cast<long *>(reinterpret_cast<char *>(data) + select_fields[i].field_offset)[j] = 0L;
					break;
				case SQLTYPE_ULONG:
					reinterpret_cast<unsigned long *>(reinterpret_cast<char *>(data) + select_fields[i].field_offset)[j] = 0UL;
					break;
				case SQLTYPE_FLOAT:
					reinterpret_cast<float *>(reinterpret_cast<char *>(data) + select_fields[i].field_offset)[j] = 0.0;
					break;
				case SQLTYPE_DOUBLE:
					reinterpret_cast<double *>(reinterpret_cast<char *>(data) + select_fields[i].field_offset)[j] = 0.0;
					break;
				default:
					break;
				}
			}
		}
	}

	return batch_status;
}

int Oracle_ResultSet::getFetchRows() const
{
	return fetched_rows;
}

bool Oracle_ResultSet::isNull(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		sb2 *ind = reinterpret_cast<sb2 *>(reinterpret_cast<char *>(data) + field.ind_offset);
		if (ind[iteration] == -1)
			return true;
		else
			return false;
	}
}

bool Oracle_ResultSet::isTruncated(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		sb2 *ind = reinterpret_cast<sb2 *>(reinterpret_cast<char *>(data) + field.ind_offset);
		if (ind[iteration] > 0)
			return true;
		else
			return false;
	}
}

void Oracle_ResultSet::getErrorMsg()
{
	OCIErrorGet(reinterpret_cast<dvoid *>(hpError),
		static_cast<ub4>(1),
		reinterpret_cast<text *>(NULL),
		&iErrorCode,
		reinterpret_cast<text*>(szErrorBuf),
		static_cast<ub4>(sizeof(szErrorBuf)),
		OCI_HTYPE_ERROR);

	errorString = reinterpret_cast<char *>(szErrorBuf);
	if (data != NULL) {
		if (*errorString.rbegin() != '\n')
			errorString += "\n";
		errorString += "SQL is ";
		errorString += data->getSQL(index);
	}
}

date Oracle_ResultSet::date2native(void *x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(x);
	return date(dt->OCIDateYYYY, dt->OCIDateMM, dt->OCIDateDD);
}

ptime Oracle_ResultSet::time2native(void *x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(x);
	return ptime(dt->OCIDateTime.OCITimeHH, dt->OCIDateTime.OCITimeMI, dt->OCIDateTime.OCITimeSS);
}

datetime Oracle_ResultSet::datetime2native(void *x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(x);
	return datetime(dt->OCIDateYYYY, dt->OCIDateMM, dt->OCIDateDD,
		dt->OCIDateTime.OCITimeHH, dt->OCIDateTime.OCITimeMI, dt->OCIDateTime.OCITimeSS);
}

sql_datetime Oracle_ResultSet::sqldatetime2native(void *x) const
{
	OCIDate *dt = reinterpret_cast<OCIDate *>(x);
	return sql_datetime(dt->OCIDateYYYY, dt->OCIDateMM, dt->OCIDateDD,
		dt->OCIDateTime.OCITimeHH, dt->OCIDateTime.OCITimeMI, dt->OCIDateTime.OCITimeSS);
}

DECLARE_DATABASE("ORACLE", Oracle_Database)

}
}

