#include "altibase_control.h"
#include "user_exception.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;

Altibase_Database::Altibase_Database()
{
	GPP = gpp_ctx::self();
	GPT = gpt_ctx::self();

	hpEnv = NULL;
	hpDbc = NULL;
	handleOpened = false;
	alloc();
}

Altibase_Database::~Altibase_Database()
{
	disconnect();
}

dbtype_enum Altibase_Database::get_dbtype()
{
	return DBTYPE_ALTIBASE;
}

void Altibase_Database::connect(const map<string, string>& conn_info)
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

	iter = conn_info.find("server");
	if (iter == conn_info.end())
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: missing process")).str(SGLOCALE));
	string server = iter->second;

	iter = conn_info.find("port_no");
	if (iter == conn_info.end())
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: missing port")).str(SGLOCALE));
	string port_no = iter->second;

	string conn_type;
	iter = conn_info.find("conn_type");
	if (iter == conn_info.end())
		conn_type = "1";
	else
		conn_type = iter->second;

	string character_set;
	iter = conn_info.find("character_set");
	if (iter == conn_info.end())
		character_set = "US7ASCII";
	else
		character_set = iter->second;

	alloc();

	string connect_string = "DSN=" + server
		+ ";UID=" + user_name
		+ ";PWD=" + password
		+ ";CONNTYPE=" + conn_type
		+ ";PORT_NO=" + port_no
		+ ";NLS_USE=" + character_set;

	int ret_code = SQLDriverConnect(hpDbc, NULL, const_cast<char*>(connect_string.c_str()), SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	ret_code = SQLSetConnectAttr(hpDbc, SQL_ATTR_BATCH, reinterpret_cast<void *>(SQL_BATCH_ON), 0);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

void Altibase_Database::disconnect()
{
	int ret_code = SQLDisconnect(hpDbc);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	free();
}

void Altibase_Database::commit()
{
	int ret_code = SQLEndTran(SQL_HANDLE_DBC, hpDbc, SQL_COMMIT);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

void Altibase_Database::rollback()
{
	int ret_code = SQLEndTran(SQL_HANDLE_DBC, hpDbc, SQL_ROLLBACK);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

Generic_Statement * Altibase_Database::create_statement()
{
	return dynamic_cast<Generic_Statement *>(new Altibase_Statement(this));

}

void Altibase_Database::terminate_statement(Generic_Statement *&stmt)
{
	if (stmt) {
		delete stmt;
		stmt = NULL;
	}
}

void Altibase_Database::alloc()
{
	if (!handleOpened) {
		int ret_code;

		// allocate Environment handle
		ret_code = SQLAllocEnv(&hpEnv);
		if (ret_code != SQL_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		// allocate Connection handle
		ret_code = SQLAllocConnect(hpEnv, &hpDbc);
		if (ret_code != SQL_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		handleOpened = true;
	}
}

void Altibase_Database::free()
{
	if (handleOpened) {
		int ret_code;

		ret_code = SQLFreeConnect(hpDbc);
		if (ret_code != SQL_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		ret_code = SQLFreeEnv(hpEnv);
		if (ret_code != SQL_SUCCESS) {
			getErrorMsg();
			throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
		}

		handleOpened = false;
	}
}

void Altibase_Database::getErrorMsg()
{
	SQLError(SQL_NULL_HENV, hpDbc, SQL_NULL_HSTMT, NULL, &iErrorCode, szErrorBuf, SQL_MAX_MESSAGE_LENGTH, &msgLength);
}

Altibase_Statement::Altibase_Statement(Altibase_Database *db_)
	: db(db_),
	  hpEnv(db_->hpEnv),
	  hpDbc(db_->hpDbc)

{
	GPP = gpp_ctx::self();
	GPT = gpt_ctx::self();

	hpStmt = NULL;

	pRowsError = NULL;
	iRowsErrorCount = 0;
	pRowsErrorCode = NULL;

	update_count = 0;
	prepared = false;

    //ret_code = connect();

	int ret_code = SQLAllocStmt(hpDbc, hpStmt);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	// Bind column-wize
	ret_code = SQLSetStmtAttr(hpStmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, SQL_NTS);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	// Bind status array
	ret_code = SQLSetStmtAttr(hpStmt, SQL_ATTR_PARAM_STATUS_PTR, pRowsErrorCode, SQL_NTS);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	// Fetch count in one batch
	ret_code = SQLSetStmtAttr(hpStmt, SQL_ATTR_ROW_ARRAY_SIZE, reinterpret_cast<void *>(data->size()), SQL_NTS);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

Altibase_Statement::~Altibase_Statement()
{
	delete []pRowsErrorCode;
	delete []pRowsError;

	int ret_code = SQLFreeStmt(hpStmt, SQL_DROP);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

dbtype_enum Altibase_Statement::get_dbtype()
{
	return DBTYPE_ALTIBASE;
}

int Altibase_Statement::getUpdateCount() const
{
	return update_count;
}

Generic_ResultSet * Altibase_Statement::executeQuery()
{
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0, ((_("ERROR: bind() should be called before executeQuery()")).str(SGLOCALE));

	execute(select_count);
	Altibase_ResultSet *grset = new Altibase_ResultSet(this);
	grset->bind(data, index);
	return dynamic_cast<Generic_ResultSet *>(grset);
}

void Altibase_Statement::executeUpdate()
{
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0, ((_("ERROR: bind() should be called before executeUpdate()")).str(SGLOCALE));

	execute(iteration + 1);
	iteration = 0;
}

void Altibase_Statement::executeArrayUpdate(int arrayLength)
{
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0, ((_("ERROR: bind() should be called before executeArrayUpdate()")).str(SGLOCALE));

	execute(arrayLength);
}

void Altibase_Statement::closeResultSet(Generic_ResultSet *&rset)
{
	if (rset) {
		Altibase_ResultSet *grset = dynamic_cast<Altibase_ResultSet *>(rset);
		delete grset;
		rset = NULL;
	}
}

bool Altibase_Statement::isNull(int colIndex)
{
	if (colIndex <= 0 || colIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	bind_field_t& field = bind_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		SQLINTEGER *ind = reinterpret_cast<SQLINTEGER *>(reinterpret_cast<char *>(data) + field.ind_offset);
		if (ind[iteration] == SQL_NULL_DATA)
			return true;
		else
			return false;
	}
}

bool Altibase_Statement::isTruncated(int colIndex)
{
	if (colIndex <= 0 || colIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	bind_field_t& field = bind_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		SQLINTEGER *ind = reinterpret_cast<SQLINTEGER *>(reinterpret_cast<char *>(data) + field.ind_offset);
		if (ind[iteration] > 0)
			return true;
		else
			return false;
	}
}

void Altibase_Statement::setNull(int paramIndex)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset == -1)// No indicator
		throw bad_param(__FILE__, __LINE__, 0, ((_("ERROR: Field indicator is not defined")).str(SGLOCALE));

	reinterpret_cast<SQLINTEGER *>(reinterpret_cast<char *>(data) + field.ind_offset)[iteration] = SQL_NULL_DATA;
}

void Altibase_Statement::setExecuteMode(execute_mode_enum execute_mode_)
{
	int ret_code;

	execute_mode = execute_mode_;
	if (execute_mode == EM_BATCH_ERRORS)
		ret_code = SQLSetConnectAttr(hpDbc, SQL_ATTR_BATCH, reinterpret_cast<void *>(SQL_BATCH_ON), 0);
	else
		ret_code = SQLSetConnectAttr(hpDbc, SQL_ATTR_BATCH, reinterpret_cast<void *>(SQL_BATCH_OFF), 0);

	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

execute_mode_enum Altibase_Statement::getExecuteMode() const
{
	return execute_mode;
}

void Altibase_Statement::getErrorMsg()
{
	SQLError(SQL_NULL_HENV, hpDbc, SQL_NULL_HSTMT, NULL, &iErrorCode, szErrorBuf, SQL_MAX_MESSAGE_LENGTH, &msgLength);
}

SQLSMALLINT Altibase_Statement::genericToCType(int field_type)
{
	switch (field_type) {
	case SQLTYPE_CHAR:
		return SQL_C_STINYINT;
	case SQLTYPE_UCHAR:
		return SQL_C_UTINYINT;
	case SQLTYPE_SHORT:
		return SQL_C_SSHORT;
	case SQLTYPE_USHORT:
		return SQL_C_USHORT;
	case SQLTYPE_LONG:
		return SQL_C_SLONG;
	case SQLTYPE_ULONG:
		return SQL_C_ULONG;
	case SQLTYPE_FLOAT:
		return SQL_C_FLOAT;
	case SQLTYPE_DOUBLE:
		return SQL_C_DOUBLE;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return SQL_C_CHAR;
	case SQLTYPE_DATE:
		return SQL_C_TYPE_DATE;
	case SQLTYPE_TIME:
		return SQL_C_TYPE_TIME;
	case SQLTYPE_DATETIME:
		return SQL_C_TYPE_TIMESTAMP;
	case SQLTYPE_INT:
	case SQLTYPE_UINT:
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field_type {1}") % field_type).str(SGLOCALE));
	}
}

SQLSMALLINT Altibase_Statement::genericToSqlType(int field_type)
{
	switch (field_type) {
	case SQLTYPE_CHAR:
	case SQLTYPE_UCHAR:
		return SQL_CHAR;
	case SQLTYPE_SHORT:
	case SQLTYPE_USHORT:
		return SQL_SMALLINT;
	case SQLTYPE_INT:
	case SQLTYPE_UINT:
		return SQL_INTEGER;
	case SQLTYPE_LONG:
	case SQLTYPE_ULONG:
		return SQL_BIGINT;
	case SQLTYPE_FLOAT:
		return SQL_FLOAT;
	case SQLTYPE_DOUBLE:
		return SQL_DOUBLE;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return SQL_VARCHAR;
	case SQLTYPE_DATE:
		return SQL_TYPE_DATE;
	case SQLTYPE_TIME:
		return SQL_TYPE_TIME;
	case SQLTYPE_DATETIME:
		return SQL_TYPE_TIMESTAMP;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field_type {1}") % field_type).str(SGLOCALE));
	}
}

date Altibase_Statement::date2native(void *x) const
{
	tagDATE_STRUCT *dt = reinterpret_cast<tagDATE_STRUCT *>(x);
	return date(dt->year, dt->month, dt->day);
}

ptime Altibase_Statement::time2native(void *x) const
{
	tagTIME_STRUCT *tm = reinterpret_cast<tagTIME_STRUCT *>(x);
	return ptime(tm->hour, tm->minute, tm->second);
}

datetime Altibase_Statement::datetime2native(void *x) const
{
	tagTIMESTAMP_STRUCT *dt = reinterpret_cast<tagTIMESTAMP_STRUCT *>(x);
	return datetime(dt->year, dt->month, dt->day, dt->hour, dt->minute, dt->second);
}

sql_datetime Altibase_Statement::sqldatetime2native(void *x) const
{
	tagTIMESTAMP_STRUCT *dt = reinterpret_cast<tagTIMESTAMP_STRUCT *>(x);
	return sql_datetime(dt->year, dt->month, dt->day,dt->hour, dt->minute, dt->second);
}

void Altibase_Statement::native2date(void *addr, const date& x) const
{
	tagDATE_STRUCT *dt = reinterpret_cast<tagDATE_STRUCT *>(addr);
	dt->year = x.year();
	dt->month = x.month();
	dt->day = x.day();
}

void Altibase_Statement::native2time(void *addr, const ptime& x) const
{
	tagTIME_STRUCT *tm = reinterpret_cast<tagTIME_STRUCT *>(addr);
	tm->hour = x.hour();
	tm->minute = x.minute();
	tm->second = x.second();
}

void Altibase_Statement::native2datetime(void *addr, const datetime& x) const
{
	tagTIMESTAMP_STRUCT *dt = reinterpret_cast<tagTIMESTAMP_STRUCT *>(addr);
	dt->year = x.year();
	dt->month = x.month();
	dt->day = x.day();
	dt->hour = x.hour();
	dt->minute = x.minute();
	dt->second = x.second();
	dt->fraction = 0;
}

void Altibase_Statement::native2sqldatetime(void *addr, const sql_datetime& x) const
{
	tagTIMESTAMP_STRUCT *dt = reinterpret_cast<tagTIMESTAMP_STRUCT *>(addr);
	dt->year = x.year();
	dt->month = x.month();
	dt->day = x.day();
	dt->hour = x.hour();
	dt->minute = x.minute();
	dt->second = x.second();
	dt->fraction = 0;
}

void Altibase_Statement::setRowOffset(ub4 rowOffset_)
{
	throw bad_sql(__FILE__, __LINE__, 0, 0, ((_("ERROR: unsupported function")).str(SGLOCALE));
}

int Altibase_Statement::getRowsErrorCount() const
{
	return iRowsErrorCount;
}

ub4 * Altibase_Statement::getRowsError()
{
	return pRowsError;
}

sb4 * Altibase_Statement::getRowsErrorCode()
{
	throw bad_sql(__FILE__, __LINE__, 0, 0, ((_("ERROR: unsupported function")).str(SGLOCALE));
}

void Altibase_Statement::prepare()
{
	if (prepared)
		return;

	int ret_code;
	string sql_str = data->getSQL();
	int array_size = data->size();

	ret_code = SQLPrepare(hpStmt, const_cast<char *>(sql_str.c_str()), SQL_NTS);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	if (pRowsError)
		delete []pRowsError;
	pRowsError = new int[array_size];

	if (pRowsErrorCode)
		delete []pRowsErrorCode;
	pRowsErrorCode = new SQLUSMALLINT[array_size];

	if (select_count) { // 是选择语句
		for (int i = 0; i < select_count; i++) {
			ret_code = SQLBindCol(hpStmt,
				static_cast<SQLSMALLINT>(i + 1),
				genericToCType(bind_fields[i].field_type),
				reinterpret_cast<char *>(data) + bind_fields[i].field_offset,
				data->size(),
				reinterpret_cast<SQLINTEGER *>(reinterpret_cast<char *>(data) + select_fields[i].ind_offset));
			if (ret_code != SQL_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
			}
		}
	}

	if (bind_count) { // 需要绑定变量
		for (int i = 0; i < bind_count; i++) {
			// Altibase不支持按名称绑定变量
			ret_code = SQLBindParameter(hpStmt,
				static_cast<SQLSMALLINT>(i + 1),
				SQL_PARAM_INPUT,
				genericToCType(bind_fields[i].field_type),
				genericToSqlType(bind_fields[i].field_type),
				static_cast<SQLUINTEGER>(bind_fields[i].field_length),
				0,
				reinterpret_cast<char *>(data) + bind_fields[i].field_offset,
				data->size(),
				reinterpret_cast<SQLINTEGER *>(reinterpret_cast<char *>(data) + bind_fields[i].ind_offset));
			if (ret_code != SQL_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
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
void Altibase_Statement::execute(int arrayLength)
{
	SQLRETURN status;
	int ret_code;

	// Bind array size
	ret_code = SQLSetStmtAttr(hpStmt, SQL_ATTR_PARAMSET_SIZE, reinterpret_cast<void *>(arrayLength), 0);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	// 执行
	status = SQLExecute(hpStmt);

	// 处理的行数
	ret_code = SQLRowCount(hpStmt, &update_count);
	if (ret_code != SQL_SUCCESS) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}

	iRowsErrorCount = arrayLength - update_count;
	for (int i = 0, j = 0; i < arrayLength; i++) {
		if (pRowsErrorCode[i] != SQL_SUCCESS && pRowsErrorCode[i] != SQL_SUCCESS_WITH_INFO) {
			pRowsErrorCode[j] = pRowsErrorCode[i];
			pRowsError[j] = i;
			j++;
		}
	}

	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO && status != SQL_NO_DATA) {
		getErrorMsg();
		throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
	}
}

void Altibase_Statement::clearNull(char *addr)
{
	reinterpret_cast<SQLINTEGER *>(addr)[iteration] = 0;
}

Altibase_ResultSet::Altibase_ResultSet(Altibase_Statement *stmt_)
	: stmt(stmt_),
	  hpStmt(stmt_->hpStmt)
{
	GPP = gpp_ctx::self();
	GPT = gpt_ctx::self();

	fetched_rows = 0;
	batch_status = FIRST_DATA_CALL;
	iteration = -1;
}

Altibase_ResultSet::~Altibase_ResultSet()
{
}

dbtype_enum Altibase_ResultSet::get_dbtype()
{
	return DBTYPE_ALTIBASE;
}

Altibase_ResultSet::Status Altibase_ResultSet::next()
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

Altibase_ResultSet::Status Altibase_ResultSet::batchNext()
{
	int ret_code;
	int array_size = data->size();

	if (batch_status == END_OF_FETCH) { // 已到末尾
		fetched_rows = 0;
	} else if (batch_status == FIRST_DATA_CALL) { // 第一次提取，在执行时预提取了部分数据
		fetched_rows = stmt->getUpdateCount();
		if (fetched_rows == array_size)
			batch_status = DATA_AVAILABLE;
		else
			batch_status = END_OF_FETCH;
	} else { // 其它情况
		SQLRETURN status = SQLFetch(hpStmt);
		if (status == SQL_NO_DATA) {
			fetched_rows = 0;
			batch_status = END_OF_FETCH;
		} else {
			fetched_rows = array_size;
			total_rows += fetched_rows;
			if (status != SQL_SUCCESS) {
				getErrorMsg();
				throw bad_sql(__FILE__, __LINE__, 0, iErrorCode, reinterpret_cast<char *>(szErrorBuf));
			}
			batch_status = DATA_AVAILABLE;
		}
	}

	// 对null数据进行标准化
	for (int i = 0; i < select_count; i++) {
		if (select_fields[i].ind_offset == -1)
			continue;

		SQLINTEGER *inds = reinterpret_cast<SQLINTEGER *>(reinterpret_cast<char *>(data) + select_fields[i].ind_offset);
		for (int j = 0; j < fetched_rows; j++) {
			if (inds[j] == SQL_NULL_DATA) { // null
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
				}
			}
		}
	}

	return batch_status;
}

int Altibase_ResultSet::getFetchRows() const
{
	return fetched_rows;
}

bool Altibase_ResultSet::isNull(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		SQLINTEGER *ind = reinterpret_cast<SQLINTEGER *>(reinterpret_cast<char *>(data) + field.ind_offset);
		if (ind[iteration] == SQL_NULL_DATA)
			return true;
		else
			return false;
	}
}

bool Altibase_ResultSet::isTruncated(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		SQLINTEGER *ind = reinterpret_cast<SQLINTEGER *>(reinterpret_cast<char *>(data) + field.ind_offset);
		if (ind[iteration] > 0)
			return true;
		else
			return false;
	}
}

void Altibase_ResultSet::getErrorMsg()
{
	SQLError(SQL_NULL_HENV, hpDbc, SQL_NULL_HSTMT, NULL, &iErrorCode, szErrorBuf, SQL_MAX_MESSAGE_LENGTH, &msgLength);
}

date Altibase_ResultSet::date2native(void *x) const
{
	tagDATE_STRUCT *dt = reinterpret_cast<tagDATE_STRUCT *>(x);
	return date(dt->year, dt->month, dt->day);
}

ptime Altibase_ResultSet::time2native(void *x) const
{
	tagTIME_STRUCT *tm = reinterpret_cast<tagTIME_STRUCT *>(x);
	return ptime(tm->hour, tm->minute, tm->second);
}

datetime Altibase_ResultSet::datetime2native(void *x) const
{
	tagTIMESTAMP_STRUCT *dt = reinterpret_cast<tagTIMESTAMP_STRUCT *>(x);
	return datetime(dt->year, dt->month, dt->day, dt->hour, dt->minute, dt->second);
}

sql_datetime Altibase_ResultSet::sqldatetime2native(void *x) const
{
	tagTIMESTAMP_STRUCT *dt = reinterpret_cast<tagTIMESTAMP_STRUCT *>(x);
	return sql_datetime(dt->year, dt->month, dt->day,dt->hour, dt->minute, dt->second);
}

DECLARE_DATABASE("ALTIBASE", Altibase_Database)

}
}

