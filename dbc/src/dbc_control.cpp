#include "dbc_internal.h"
#include "dbc_control.h"

extern void yyrestart(FILE *input_file);

namespace ai
{
namespace scci
{

extern int inquote;
extern int incomments;
extern string tminline;
extern int line_no;
extern int column;

using namespace ai::sg;

group_data_t::group_data_t()
{
}

group_data_t::~group_data_t()
{
}

bool group_data_t::operator<(const group_data_t& rhs) const
{
	dbct_ctx *DBCT = dbct_ctx::instance();

	const char *in[1] = { rhs.data };
	char *out[1] = { data };

	return ((*DBCT->_DBCT_group_func)(NULL, NULL, in, out) < 0);
}

group_order_compare::group_order_compare(compiler::func_type order_func_)
	: order_func(order_func_)
{
}

bool group_order_compare::operator()(char *left, char *right) const
{
	const char *in[1] = { right };
	char *out[1] = { left };

	return ((*order_func)(NULL, NULL, in, out) < 0);
}

Dbc_Database::Dbc_Database()
	: DBCT(dbct_ctx::instance()),
	  api_mgr(dbc_api::instance(DBCT))
{
}

Dbc_Database::~Dbc_Database()
{
	disconnect();
}

dbtype_enum Dbc_Database::get_dbtype()
{
	return DBTYPE_DBC;
}

void Dbc_Database::connect(const map<string, string>& conn_info)
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

	if (!api_mgr.login())
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: login to DBC failed.")).str(SGLOCALE));

	if (!api_mgr.connect(user_name, password))
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: connect to DBC failed.")).str(SGLOCALE));
}

void Dbc_Database::disconnect()
{
	api_mgr.disconnect();
	api_mgr.logout();
}

void Dbc_Database::commit()
{
	if (!api_mgr.commit())
		throw bad_sql(__FILE__, __LINE__, 0, DBCT->_DBCT_error_code, (_("ERROR: commit() failed, {1}") % DBCT->strerror()).str(SGLOCALE));
}

void Dbc_Database::rollback()
{
	if (!api_mgr.rollback())
		throw bad_sql(__FILE__, __LINE__, 0, DBCT->_DBCT_error_code, (_("ERROR: rollback() failed, {1}") % DBCT->strerror()).str(SGLOCALE));
}

Generic_Statement * Dbc_Database::create_statement()
{
	return dynamic_cast<Generic_Statement *>(new Dbc_Statement(this));
}

void Dbc_Database::terminate_statement(Generic_Statement *& stmt)
{
	if (stmt) {
		delete stmt;
		stmt = NULL;
	}
}

Dbc_Statement::Dbc_Statement(Dbc_Database *db_)
	: db(db_),
	  api_mgr(db_->api_mgr)
{
	DBCP = dbcp_ctx::instance();
	DBCT = dbct_ctx::instance();
	cmpl = NULL;
	update_count = 0;

	pRowsError = NULL;
	iRowsErrorCount = 0;
	pRowsErrorCode = NULL;

	rowOffset = 0;
	update_count = 0;
	execute_mode = EM_DEFAULT;
	prepared = false;
}

Dbc_Statement::~Dbc_Statement()
{
	delete []pRowsErrorCode;
	delete []pRowsError;
	delete cmpl;
}

dbtype_enum Dbc_Statement::get_dbtype()
{
	return DBTYPE_DBC;
}

int Dbc_Statement::getUpdateCount() const
{
	return update_count;
}

Generic_ResultSet * Dbc_Statement::executeQuery()
{
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0, (_("ERROR: bind() should be called before executeQuery()")).str(SGLOCALE));

	execute(data->size());
	Dbc_ResultSet *grset = new Dbc_ResultSet(this);
	grset->bind(data, index);
	return dynamic_cast<Generic_ResultSet *>(grset);
}

void Dbc_Statement::executeUpdate()
{
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0, (_("ERROR: bind() should be called before executeUpdate()")).str(SGLOCALE));

	execute(iteration + 1);
	iteration = 0;
}

void Dbc_Statement::executeArrayUpdate(int arrayLength)
{
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0, (_("ERROR: bind() should be called before executeArrayUpdate()")).str(SGLOCALE));

	execute(arrayLength);
}

void Dbc_Statement::closeResultSet(Generic_ResultSet *& rset)
{
	if (rset) {
		Dbc_ResultSet *grset = dynamic_cast<Dbc_ResultSet *>(rset);
		delete grset;
		rset = NULL;
	}
}

bool Dbc_Statement::isNull(int colIndex)
{
	return false;
}

bool Dbc_Statement::isTruncated(int colIndex)
{
	return false;
}

void Dbc_Statement::setNull(int paramIndex)
{
	throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unsupported function.")).str(SGLOCALE));
}

void Dbc_Statement::prepare()
{
	int ret;

	if (prepared)
		return;

	if (cmpl)
		delete cmpl;
	cmpl = new compiler();
	cmpl->set_cplusplus(true);

	string sql = data->getSQL();
	int array_size = data->size();

	char temp_name[4096];
	const char *tmpdir = ::getenv("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = P_tmpdir;

	sprintf(temp_name, "%s/scciXXXXXX", tmpdir);

	int fd = ::mkstemp(temp_name);
	if (fd == -1)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: mkstemp failure, {1}") % ::strerror(errno)).str(SGLOCALE));

	yyin = ::fdopen(fd, "w+");
	if (yyin == NULL) {
		::close(fd);
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: open temp file [{1}] failure, {2}") % temp_name % ::strerror(errno)).str(SGLOCALE));
	}

	if (::fwrite(sql.c_str(), sql.length(), 1, yyin) != 1) {
		::fclose(yyin);
		::unlink(temp_name);
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: write sql to file [{1}] failure, {2}") % temp_name % ::strerror(errno)).str(SGLOCALE));
	}

	::fflush(yyin);
	::fseek(yyin, 0, SEEK_SET);

	yyout = stdout;		// default
	errcnt = 0;

	inquote = 0;
	incomments = 0;
	tminline = "";
	line_no = 0;
	column = 0;

	sql_statement::clear();

	// call yyparse() to parse temp file
	yyrestart(yyin);
	if ((ret = yyparse()) < 0) {
		::fclose(yyin);
		::unlink(temp_name);
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Parse failed.")).str(SGLOCALE));
	}

	if (ret == 1) {
		::fclose(yyin);
		::unlink(temp_name);
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Severe error found. Stop syntax checking.")).str(SGLOCALE));
	}

	if (errcnt > 0) {
		::fclose(yyin);
		::unlink(temp_name);
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Above errors found during syntax checking.")).str(SGLOCALE));
	}

	::fclose(yyin);
	::unlink(temp_name);

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	sql_statement *stmt = sql_statement::first();
	string table_name = stmt->gen_table();

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	prepared_te = te_mgr.find_te(table_name);
	if (prepared_te == NULL)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find TE for table {1}") % table_name).str(SGLOCALE));
	te_mgr.set_ctx(prepared_te);

	// 获取表中字段及其偏移量、字节数之间的关系
	get_field_map();

	// 获取SQL定义
	if (select_count > 0 && select_fields[0].field_type == SQLTYPE_ANY)
		describe();

	// 分配内存
	if (pRowsError)
		delete []pRowsError;
	pRowsError = new ub4[array_size];

	if (pRowsErrorCode)
		delete []pRowsErrorCode;
	pRowsErrorCode = new sb4[array_size];

	stmt_type = stmt->get_stmt_type();
	switch (stmt_type) {
	case STMTTYPE_SELECT:
		prepare_select(dynamic_cast<sql_select *>(stmt));
		break;
	case STMTTYPE_INSERT:
		prepare_insert(dynamic_cast<sql_insert *>(stmt));
		break;
	case STMTTYPE_UPDATE:
		prepare_update(dynamic_cast<sql_update *>(stmt));
		break;
	case STMTTYPE_DELETE:
		prepare_delete(dynamic_cast<sql_delete *>(stmt));
		break;
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unsupported statement type given.")).str(SGLOCALE));
	}

	prepared = true;
}

// 在调用之前需要设置提交模式
// DBC_DEFAULT
// DBC_DESCRIBE_ONLY
// DBC_COMMIT_ON_SUCCESS
// DBC_EXACT_FETCH
// DBC_BATCH_ERRORS
// 对no-select语句，实际执行次数为iRows-rowOffset
void Dbc_Statement::execute(int arrayLength)
{
	int i;
	int j;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	te_mgr.set_ctx(prepared_te);

	DBCT->enter_execute();
	update_count = 0;

	switch (stmt_type) {
	case STMTTYPE_SELECT:
		DBCT->set_where_func(where_func);

		if (index_id == -1) {
			cursor = api_mgr.find(prepared_te->table_id, NULL);
		} else if (index_id == -2) {
			long rowid;
			char *out[1] = { reinterpret_cast<char *>(&rowid) };
			(*wkey_func)(NULL, NULL, const_cast<const char **>(DBCT->_DBCT_input_binds), out);
			cursor = api_mgr.find_by_rowid(prepared_te->table_id, rowid);
		} else {
			dbc_data_t& te_meta = prepared_te->te_meta;
			boost::shared_array<char> auto_record(new char[te_meta.struct_size]);
			char *record = auto_record.get();

			(*wkey_func)(NULL, NULL, const_cast<const char **>(DBCT->_DBCT_input_binds), &record);
			cursor = api_mgr.find(prepared_te->table_id, index_id, record);
		}

		DBCT->set_where_func(NULL);
		break;
	case STMTTYPE_INSERT:
		{
			dbc_data_t& te_meta = prepared_te->te_meta;
			boost::shared_array<char> auto_record(new char[te_meta.struct_size]);
			char *record = auto_record.get();

			if (rowOffset > 0) {
				for (j = 0; j < bind_count; j++)
					DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
			}

			char *global[3];
			global[GI_ROW_DATA] = record;
			global[GI_DATA_START] = DBCT->_DBCT_data_start;
			global[GI_TIMESTAMP] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);

			iRowsErrorCount = 0;
			for (i = rowOffset; i < arrayLength; i++) {
				if (i > rowOffset) {
					for (j = 0; j < bind_count; j++)
						DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
				}

				memset(record, 0, te_meta.struct_size);
				(*set_func)(NULL, global, const_cast<const char **>(DBCT->_DBCT_input_binds), NULL);
				if (!api_mgr.insert(prepared_te->table_id, record)) {
					if (execute_mode == EM_BATCH_ERRORS) {
						pRowsError[iRowsErrorCount] = i;
						pRowsErrorCode[iRowsErrorCount] = DBCT->_DBCT_error_code;
						iRowsErrorCount++;
						continue;
					}

					if (i > 0) {
						for (j = 0; j < bind_count; j++)
							DBCT->_DBCT_input_binds[j] -= i * bind_fields[j].field_length;
					}

					if (DBCT->_DBCT_error_code == DBCETIME)
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Transaction timeout, please rollback transaction first.")).str(SGLOCALE));
					else
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: insert execution for table {1} failed, {2}") % te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				}

				update_count++;
			}

			for (j = 0; j < bind_count; j++)
				DBCT->_DBCT_input_binds[j] -= (arrayLength - 1) * bind_fields[j].field_length;
		}
		break;
	case STMTTYPE_UPDATE:
		DBCT->set_where_func(where_func);
		DBCT->_DBCT_update_func_internal = set_func;

		if (index_id == -1) {
			if (rowOffset > 0) {
				for (j = 0; j < bind_count; j++)
					DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
			}

			iRowsErrorCount = 0;
			for (i = rowOffset; i < arrayLength; i++) {
				if (i > rowOffset) {
					for (j = 0; j < bind_count; j++)
						DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
				}

				int affected_rows = api_mgr.update(prepared_te->table_id, NULL, NULL);
				if (affected_rows < 0) {
					if (execute_mode == EM_BATCH_ERRORS) {
						pRowsError[iRowsErrorCount] = i;
						pRowsErrorCode[iRowsErrorCount] = DBCT->_DBCT_error_code;
						iRowsErrorCount++;
						continue;
					}

					if (i > 0) {
						for (j = 0; j < bind_count; j++)
							DBCT->_DBCT_input_binds[j] -= i * bind_fields[j].field_length;
					}

					DBCT->_DBCT_update_func_internal = NULL;
					DBCT->set_where_func(NULL);

					if (DBCT->_DBCT_error_code == DBCETIME)
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Transaction timeout, please rollback transaction first.")).str(SGLOCALE));
					else
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: update execution for table {1} failed, {2}") % prepared_te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				}

				update_count += affected_rows;
			}
		} else if (index_id == -2) {
			// 获得输入条件结构
			long rowid;
			char *out[1] = { reinterpret_cast<char *>(&rowid) };

			if (rowOffset > 0) {
				for (j = 0; j < bind_count; j++)
					DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
			}

			iRowsErrorCount = 0;
			for (i = rowOffset; i < arrayLength; i++) {
				if (i > rowOffset) {
					for (j = 0; j < bind_count; j++)
						DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
				}

				(*wkey_func)(NULL, NULL, const_cast<const char **>(DBCT->_DBCT_input_binds), out);

				int affected_rows = api_mgr.update_by_rowid(prepared_te->table_id, rowid, NULL);
				if (affected_rows < 0) {
					if (execute_mode == EM_BATCH_ERRORS) {
						pRowsError[iRowsErrorCount] = i;
						pRowsErrorCode[iRowsErrorCount] = DBCT->_DBCT_error_code;
						iRowsErrorCount++;
						continue;
					}

					if (i > 0) {
						for (j = 0; j < bind_count; j++)
							DBCT->_DBCT_input_binds[j] -= i * bind_fields[j].field_length;
					}

					DBCT->_DBCT_update_func_internal = NULL;
					DBCT->set_where_func(NULL);

					if (DBCT->_DBCT_error_code == DBCETIME)
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Transaction timeout, please rollback transaction first.")).str(SGLOCALE));
					else
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: update execution for table {1} failed, {2}") % prepared_te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				}

				update_count += affected_rows;
			}
		} else {
			// 获得输入条件结构
			dbc_data_t& te_meta = prepared_te->te_meta;
			boost::shared_array<char> auto_record(new char[te_meta.struct_size]);
			char *record = auto_record.get();

			DBCT->_DBCT_update_func_internal = set_func;

			if (rowOffset > 0) {
				for (j = 0; j < bind_count; j++)
					DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
			}

			iRowsErrorCount = 0;
			for (i = rowOffset; i < arrayLength; i++) {
				if (i > rowOffset) {
					for (j = 0; j < bind_count; j++)
						DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
				}

				(*wkey_func)(NULL, NULL, const_cast<const char **>(DBCT->_DBCT_input_binds), &record);

				int affected_rows = api_mgr.update(prepared_te->table_id, index_id, record, NULL);
				if (affected_rows < 0) {
					if (execute_mode == EM_BATCH_ERRORS) {
						pRowsError[iRowsErrorCount] = i;
						pRowsErrorCode[iRowsErrorCount] = DBCT->_DBCT_error_code;
						iRowsErrorCount++;
						continue;
					}

					if (i > 0) {
						for (j = 0; j < bind_count; j++)
							DBCT->_DBCT_input_binds[j] -= i * bind_fields[j].field_length;
					}

					DBCT->_DBCT_update_func_internal = NULL;
					DBCT->set_where_func(NULL);

					if (DBCT->_DBCT_error_code == DBCETIME)
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Transaction timeout, please rollback transaction first.")).str(SGLOCALE));
					else
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: update execution for table {1} failed, {2}") % prepared_te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				}

				update_count += affected_rows;
			}
		}

		for (j = 0; j < bind_count; j++)
			DBCT->_DBCT_input_binds[j] -= (arrayLength - 1) * bind_fields[j].field_length;

		DBCT->_DBCT_update_func_internal = NULL;
		DBCT->set_where_func(NULL);
		break;
	case STMTTYPE_DELETE:
		DBCT->set_where_func(where_func);

		if (index_id == -1) {
			if (rowOffset > 0) {
				for (j = 0; j < bind_count; j++)
					DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
			}

			iRowsErrorCount = 0;
			for (i = rowOffset; i < arrayLength; i++) {
				if (i > rowOffset) {
					for (j = 0; j < bind_count; j++)
						DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
				}

				int affected_rows = api_mgr.erase(prepared_te->table_id, NULL);
				if (affected_rows < 0) {
					if (execute_mode == EM_BATCH_ERRORS) {
						pRowsError[iRowsErrorCount] = i;
						pRowsErrorCode[iRowsErrorCount] = DBCT->_DBCT_error_code;
						iRowsErrorCount++;
						continue;
					}

					if (i > 0) {
						for (j = 0; j < bind_count; j++)
							DBCT->_DBCT_input_binds[j] -= i * bind_fields[j].field_length;
					}

					DBCT->set_where_func(NULL);

					if (DBCT->_DBCT_error_code == DBCETIME)
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Transaction timeout, please rollback transaction first.")).str(SGLOCALE));
					else
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: delete execution for table {1} failed, {2}") % prepared_te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				}

				update_count += affected_rows;
			}
		} else if (index_id == -2) {
			long rowid;
			char *out[1] = { reinterpret_cast<char *>(&rowid) };

			if (rowOffset > 0) {
				for (j = 0; j < bind_count; j++)
					DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
			}

			iRowsErrorCount = 0;
			for (i = rowOffset; i < arrayLength; i++) {
				if (i > rowOffset) {
					for (j = 0; j < bind_count; j++)
						DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
				}

				(*wkey_func)(NULL, NULL, const_cast<const char **>(DBCT->_DBCT_input_binds), out);

				int affected_rows = api_mgr.erase_by_rowid(prepared_te->table_id, rowid);
				if (affected_rows < 0) {
					if (execute_mode == EM_BATCH_ERRORS) {
						pRowsError[iRowsErrorCount] = i;
						pRowsErrorCode[iRowsErrorCount] = DBCT->_DBCT_error_code;
						iRowsErrorCount++;
						continue;
					}

					if (i > 0) {
						for (j = 0; j < bind_count; j++)
							DBCT->_DBCT_input_binds[j] -= i * bind_fields[j].field_length;
					}

					DBCT->set_where_func(NULL);

					if (DBCT->_DBCT_error_code == DBCETIME)
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Transaction timeout, please rollback transaction first.")).str(SGLOCALE));
					else
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: delete execution for table {1} failed, {2}") % prepared_te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				}

				update_count += affected_rows;
			}
		} else {
			dbc_data_t& te_meta = prepared_te->te_meta;
			boost::shared_array<char> auto_record(new char[te_meta.struct_size]);
			char *record = auto_record.get();

			if (rowOffset > 0) {
				for (j = 0; j < bind_count; j++)
					DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
			}

			iRowsErrorCount = 0;
			for (i = rowOffset; i < arrayLength; i++) {
				if (i > rowOffset) {
					for (j = 0; j < bind_count; j++)
						DBCT->_DBCT_input_binds[j] += bind_fields[j].field_length;
				}

				(*wkey_func)(NULL, NULL, const_cast<const char **>(DBCT->_DBCT_input_binds), &record);

				int affected_rows = api_mgr.erase(prepared_te->table_id, index_id, record);
				if (affected_rows < 0) {
					if (execute_mode == EM_BATCH_ERRORS) {
						pRowsError[iRowsErrorCount] = i;
						pRowsErrorCode[iRowsErrorCount] = DBCT->_DBCT_error_code;
						iRowsErrorCount++;
						continue;
					}

					if (i > 0) {
						for (j = 0; j < bind_count; j++)
							DBCT->_DBCT_input_binds[j] -= i * bind_fields[j].field_length;
					}

					DBCT->set_where_func(NULL);

					if (DBCT->_DBCT_error_code == DBCETIME)
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Transaction timeout, please rollback transaction first.")).str(SGLOCALE));
					else
						throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: delete execution for table {1} failed, {2}") % prepared_te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				}

				update_count += affected_rows;
			}
		}

		for (j = 0; j < bind_count; j++)
			DBCT->_DBCT_input_binds[j] -= (arrayLength - 1) * bind_fields[j].field_length;

		DBCT->set_where_func(NULL);
		break;
	case STMTTYPE_CREATE:
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unsupported SQL statemenet.")).str(SGLOCALE));
	}
}

// 默认模式为DBC_DEFAULT
void Dbc_Statement::setExecuteMode(execute_mode_enum execute_mode_)
{
	execute_mode = execute_mode_;
}

execute_mode_enum Dbc_Statement::getExecuteMode() const
{
	return execute_mode;
}

date Dbc_Statement::date2native(void *x) const
{
	return date(*reinterpret_cast<time_t *>(x));
}

ptime Dbc_Statement::time2native(void *x) const
{
	return ptime(*reinterpret_cast<time_t *>(x));
}

datetime Dbc_Statement::datetime2native(void *x) const
{
	return datetime(*reinterpret_cast<time_t *>(x));
}

sql_datetime Dbc_Statement::sqldatetime2native(void *x) const
{
	datetime dt(*reinterpret_cast<time_t *>(x));
	return sql_datetime(dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
}

void Dbc_Statement::native2date(void *addr, const date& x) const
{
	time_t *dt = reinterpret_cast<time_t *>(addr);
	*dt = x.duration();
}

void Dbc_Statement::native2time(void *addr, const ptime& x) const
{
	time_t *dt = reinterpret_cast<time_t *>(addr);
	*dt = x.duration();
}

void Dbc_Statement::native2datetime(void *addr, const datetime& x) const
{
	time_t *dt = reinterpret_cast<time_t *>(addr);
	*dt = x.duration();
}

void Dbc_Statement::native2sqldatetime(void *addr, const sql_datetime& x) const
{
	time_t *dt = reinterpret_cast<time_t *>(addr);
	*dt = datetime(x.year(), x.month(), x.day(), x.hour(), x.minute(), x.second()).duration();
}

void Dbc_Statement::clearNull(char *addr)
{
	throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unsupported function.")).str(SGLOCALE));
}

int Dbc_Statement::prepare_select(sql_select *stmt)
{
	bind_field_t *current_field;
	int field_id;

	order_code = "";
	group_code = "";
	agg_code = "";
	gcomp_code = "";
	having_code = "";

	// 分析条件语句
	relation_item_t *rela_item = stmt->get_where_items();
	parse_where(stmt, rela_item);

	// 分析group语句
	bool grouped = is_grouped(stmt);

	// 分析选择语句
	if (!grouped)
		gen_direct_select(stmt);
	else
		gen_group_select(stmt);

	// 设置选择语句的绑定变量
	for (current_field = select_fields, field_id = 0; current_field->field_type != SQLTYPE_UNKNOWN; ++current_field, field_id++) {
		if (field_id >= DBCT->_DBCT_set_bind_count) {
			if (DBCT->_DBCT_set_bind_count == 0) {
				DBCT->_DBCT_set_bind_count = 32;
				DBCT->_DBCT_set_binds = new char *[DBCT->_DBCT_set_bind_count];
			} else {
				char **tmp = new char *[DBCT->_DBCT_set_bind_count * 2];
				memcpy(tmp, DBCT->_DBCT_set_binds, sizeof(char *) * DBCT->_DBCT_set_bind_count);
				delete []DBCT->_DBCT_set_binds;
				DBCT->_DBCT_set_binds = tmp;
				DBCT->_DBCT_set_bind_count <<= 1;
			}
		}

		DBCT->_DBCT_set_binds[field_id] = reinterpret_cast<char *>(data) + current_field->field_offset;
	}

	int wkey_func_idx;
	int where_func_idx;
	int set_idx;
	int order_idx;
	int group_idx;
	int agg_idx;
	int gcomp_idx;
	int having_idx;

	if (!wkey_code.empty())
		wkey_func_idx = cmpl->create_function(wkey_code, map<string, int>(), map<string, int>(), map<string, int>());
	else
		wkey_func_idx = -1;

	if (!where_code.empty())
		where_func_idx = cmpl->create_function(where_code, map<string, int>(), map<string, int>(), map<string, int>());
	else
		where_func_idx = -1;

	set_idx = cmpl->create_function(set_code, map<string, int>(), map<string, int>(), map<string, int>());

	if (!order_code.empty())
		order_idx = cmpl->create_function(order_code, map<string, int>(), map<string, int>(), map<string, int>());
	else
		order_idx = -1;

	if (grouped) {
		group_idx = cmpl->create_function(group_code, map<string, int>(), map<string, int>(), map<string, int>());
		agg_idx = cmpl->create_function(agg_code, map<string, int>(), map<string, int>(), map<string, int>());
		gcomp_idx = cmpl->create_function(gcomp_code, map<string, int>(), map<string, int>(), map<string, int>());
	} else {
		group_idx = -1;
		agg_idx = -1;
		gcomp_idx = -1;
	}

	if (!having_code.empty())
		having_idx = cmpl->create_function(having_code, map<string, int>(), map<string, int>(), map<string, int>());
	else
		having_idx = -1;

	cmpl->build();

	if (wkey_func_idx != -1)
		wkey_func = cmpl->get_function(wkey_func_idx);
	else
		wkey_func = NULL;

	if (where_func_idx != -1)
		where_func = cmpl->get_function(where_func_idx);
	else
		where_func = NULL;

	set_func = cmpl->get_function(set_idx);

	if (order_idx != -1)
		order_func = cmpl->get_function(order_idx);
	else
		order_func = NULL;

	if (group_idx != -1)
		group_func = cmpl->get_function(group_idx);
	else
		group_func = NULL;

	if (agg_idx != -1)
		agg_func = cmpl->get_function(agg_idx);
	else
		agg_func = NULL;

	if (gcomp_idx != -1)
		gcomp_func = cmpl->get_function(gcomp_idx);
	else
		gcomp_func = NULL;

	if (having_idx != -1)
		having_func = cmpl->get_function(having_idx);
	else
		having_func = NULL;

	return 0;
}

int Dbc_Statement::prepare_insert(sql_insert *stmt)
{
	arg_list_t *ident_items = stmt->get_ident_items();
	arg_list_t *insert_items = stmt->get_insert_items();

	bind_field_t *current_field;
	int field_id = 0;

	fmt.str("");
	fmt <<"\tdbc_sqlfun_init();\n\n";

	arg_list_t::const_iterator ident_iter;
	if (ident_items)
		ident_iter = ident_items->begin();

	dbc_data_t& te_meta = prepared_te->te_meta;
	for (arg_list_t::const_iterator iter = insert_items->begin(); iter != insert_items->end(); ++iter, field_id++) {
		gen_code_t para(&field_map, bind_fields);
		string& code = para.code;

		(*iter)->gen_code(para);

		string field_name;
		dbc_data_field_t *field = NULL;
		if (ident_items) {
			field_name = (*ident_iter)->simple_item->value;
			for (int i = 0; i < te_meta.field_size; i++) {
				field = &te_meta.fields[i];
				if (strcasecmp(field_name.c_str(), field->field_name) == 0)
					break;
			}
			ident_iter++;
		} else {
			field = &te_meta.fields[field_id];
		}

		string str1;
		string str2;
		string str3;
		gen_format(field->field_type, str1, str2, str3);

		if (field->field_type != SQLTYPE_STRING && field->field_type != SQLTYPE_VSTRING) {
			fmt << "\t" << str3 << str1 << "global[0] + " << field->field_offset
				<< "), " << code << ");\n";
		} else {
			fmt << "\t" << str3 << "global[0] + " << field->field_offset
				<< ", " << code << ", " << (field->field_size - 1) << ");\n";
		}
	}

	set_code = fmt.str();
	dout << "set_code = [\n" << set_code << "\n]" << endl;

	// 设置条件语句的绑定变量
	for (current_field = bind_fields, field_id = 0; current_field->field_type != SQLTYPE_UNKNOWN; ++current_field, field_id++) {
		if (field_id >= DBCT->_DBCT_input_bind_count) {
			if (DBCT->_DBCT_input_bind_count == 0) {
				DBCT->_DBCT_input_bind_count = 32;
				DBCT->_DBCT_input_binds = new char *[DBCT->_DBCT_input_bind_count];
			} else {
				char **tmp = new char *[DBCT->_DBCT_input_bind_count * 2];
				memcpy(tmp, DBCT->_DBCT_input_binds, sizeof(char *) * DBCT->_DBCT_input_bind_count);
				delete []DBCT->_DBCT_input_binds;
				DBCT->_DBCT_input_binds = tmp;
				DBCT->_DBCT_input_bind_count <<= 1;
			}
		}

		DBCT->_DBCT_input_binds[field_id] = reinterpret_cast<char *>(data) + current_field->field_offset;
	}

	int set_idx = cmpl->create_function(set_code, map<string, int>(), map<string, int>(), map<string, int>());

	cmpl->build();

	set_func = cmpl->get_function(set_idx);

	return 0;
}

int Dbc_Statement::prepare_update(sql_update *stmt)
{
	// 分析条件语句
	relation_item_t *rela_item = stmt->get_where_items();
	parse_where(stmt, rela_item);

	// 分析设置语句
	arg_list_t *update_items = stmt->get_update_items();

	fmt.str("");
	fmt <<"\tdbc_sqlfun_init();\n\n";

	for (arg_list_t::const_iterator iter = update_items->begin(); iter != update_items->end(); ++iter) {
		composite_item_t *composite_item = *iter;
		if (composite_item->rela_item == NULL) {
			DBCT->_DBCT_error_code = DBCEINVAL;
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Wrong SQL string given.")).str(SGLOCALE));
		}

		relation_item_t *relation_item = composite_item->rela_item;
		composite_item_t *left = relation_item->left;
		composite_item_t *right = relation_item->right;

		simple_item_t *simple_item = left->simple_item;
		if (simple_item == NULL) {
			DBCT->_DBCT_error_code = DBCEINVAL;
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Wrong SQL string given.")).str(SGLOCALE));
		}

		gen_code_t para(&field_map, bind_fields);
		string& code = para.code;

		right->gen_code(para);

		dbc_data_field_t *field = NULL;
		dbc_data_t& te_meta = prepared_te->te_meta;
		for (int i = 0; i < te_meta.field_size; i++) {
			field = &te_meta.fields[i];
			if (strcasecmp(simple_item->value.c_str(), field->field_name) == 0)
				break;
		}

		string str1;
		string str2;
		string str3;
		gen_format(field->field_type, str1, str2, str3);

		if (field->field_type != SQLTYPE_STRING && field->field_type != SQLTYPE_VSTRING) {
			fmt << "\t" << str3 << str1 << "#0 + " << field->field_offset
				<< "), " << code << ");\n";
		} else {
			fmt << "\t" << str3 << "#0 + " << field->field_offset
				<< ", " << code << ", " << (field->field_size - 1) << ");\n";
		}
	}

	set_code = fmt.str();
	dout << "set_code = [\n" << set_code << "\n]" << endl;

	int wkey_func_idx;
	int where_func_idx;
	int set_idx;

	if (!wkey_code.empty())
		wkey_func_idx = cmpl->create_function(wkey_code, map<string, int>(), map<string, int>(), map<string, int>());
	else
		wkey_func_idx = -1;

	if (!where_code.empty())
		where_func_idx = cmpl->create_function(where_code, map<string, int>(), map<string, int>(), map<string, int>());
	else
		where_func_idx = -1;

	set_idx = cmpl->create_function(set_code, map<string, int>(), map<string, int>(), map<string, int>());

	cmpl->build();

	if (wkey_func_idx != -1)
		wkey_func = cmpl->get_function(wkey_func_idx);
	else
		wkey_func = NULL;

	if (where_func_idx != -1)
		where_func = cmpl->get_function(where_func_idx);
	else
		where_func = NULL;

	set_func = cmpl->get_function(set_idx);

	return 0;
}

int Dbc_Statement::prepare_delete(sql_delete *stmt)
{
	// 分析条件语句
	relation_item_t *rela_item = stmt->get_where_items();
	parse_where(stmt, rela_item);

	int wkey_func_idx;
	int where_func_idx;

	if (!wkey_code.empty())
		wkey_func_idx = cmpl->create_function(wkey_code, map<string, int>(), map<string, int>(), map<string, int>());
	else
		wkey_func_idx = -1;

	if (!where_code.empty())
		where_func_idx = cmpl->create_function(where_code, map<string, int>(), map<string, int>(), map<string, int>());
	else
		where_func_idx = -1;

	cmpl->build();

	if (wkey_func_idx != -1)
		wkey_func = cmpl->get_function(wkey_func_idx);
	else
		wkey_func = NULL;

	if (where_func_idx != -1)
		where_func = cmpl->get_function(where_func_idx);
	else
		where_func = NULL;

	return 0;
}

void Dbc_Statement::parse_where(sql_statement *stmt, relation_item_t *rela_item)
{
	match_index(rela_item);

	if (index_id == -2)
		gen_wkey_by_rowid(stmt);
	else if (index_id >= 0)
		gen_wkey_by_index(stmt);

	gen_where();

	bind_field_t *current_field;
	int field_id;

	// 设置条件语句的绑定变量
	for (current_field = bind_fields, field_id = 0; current_field->field_type != SQLTYPE_UNKNOWN; ++current_field, field_id++) {
		if (field_id >= DBCT->_DBCT_input_bind_count) {
			if (DBCT->_DBCT_input_bind_count == 0) {
				DBCT->_DBCT_input_bind_count = 32;
				DBCT->_DBCT_input_binds = new char *[DBCT->_DBCT_input_bind_count];
			} else {
				char **tmp = new char *[DBCT->_DBCT_input_bind_count * 2];
				memcpy(tmp, DBCT->_DBCT_input_binds, sizeof(char *) * DBCT->_DBCT_input_bind_count);
				delete []DBCT->_DBCT_input_binds;
				DBCT->_DBCT_input_binds = tmp;
				DBCT->_DBCT_input_bind_count <<= 1;
			}
		}

		DBCT->_DBCT_input_binds[field_id] = reinterpret_cast<char *>(data) + current_field->field_offset;
	}
}

void Dbc_Statement::gen_where()
{
	if (!cond_codes.empty()) {
		where_code = "\tdbc_sqlfun_init();\n\n";
		for (vector<string>::const_iterator iter = cond_codes.begin(); iter != cond_codes.end(); ++iter) {
			where_code += "\tif (!(";
			where_code += *iter;
			where_code += "))\n";
			where_code += "\t\treturn -1;\n\n";
		}
		where_code += "\treturn 0;\n";
	} else {
		if (index_id == -1)
			where_code = "\treturn 0;\n";
		else
			where_code = "";
	}

	dout << "where_code = [\n" << where_code << "\n]" << endl;
}

void Dbc_Statement::gen_wkey_by_rowid(sql_statement *stmt)
{
	fmt.str("");
	fmt << "\tdbc_sqlfun_init();\n\n"
		<< "\tdbc_assign(*reinterpret_cast<long *>(#0), " << cond_map.begin()->second << ");\n";
	wkey_code = fmt.str();

	dout << "wkey_code = [\n" << wkey_code << "\n]" << endl;
}

void Dbc_Statement::gen_wkey_by_index(sql_statement *stmt)
{
	fmt.str("");
	fmt <<"\tdbc_sqlfun_init();\n\n";

	dbc_data_t& te_meta = prepared_te->te_meta;
	int field_id = 0;
	for (map<string, string>::const_iterator iter = cond_map.begin(); iter != cond_map.end(); ++iter, field_id++) {
		const dbc_data_field_t *field = NULL;

		for (int i = 0; i < te_meta.field_size; i++) {
			field = &te_meta.fields[i];
			if (iter->first == field->field_name)
				break;
		}

		string str1;
		string str2;
		string str3;
		gen_format(field->field_type, str1, str2, str3);

		if (field->field_type != SQLTYPE_STRING && field->field_type != SQLTYPE_VSTRING) {
			fmt << "\t" << str3 << str1 << "#0 + " << field->field_offset
				<< "), " << iter->second << ");\n";
		} else {
			fmt << "\t" << str3 << "#0 + " << field->field_offset
				<< ", " << iter->second << ", " << (field->field_size - 1) << ");\n";
		}
	}

	wkey_code = fmt.str();

	dout << "wkey_code = [\n" << wkey_code << "\n]" << endl;

	// lock index so we can make sure the index is available all the time.
	if (prepared_te->table_id < TE_MIN_RESERVED) {
		t_sys_index_lock item;
		item.pid = DBCP->_DBCP_current_pid;
		item.table_id = prepared_te->table_id;
		item.index_id = index_id;
		item.lock_date = DBCT->_DBCT_timestamp;

		// 先删除原有记录，忽略错误
		api_mgr.erase(TE_SYS_INDEX_LOCK, 0, &item);
		api_mgr.insert(TE_SYS_INDEX_LOCK, &item);
	}
}

void Dbc_Statement::match_index(relation_item_t *rela_item)
{
	cond_map.clear();
	cond_codes.clear();

	if (rela_item == NULL) {
		index_id = -1;
		return;
	}

	map<string, string>::const_iterator iter;
	dbc_te& te_mgr = dbc_te::instance(DBCT);

	if (cond_is_index_style(rela_item)) {
		if (cond_map.size() == 1 && cond_map.begin()->first == "rowid") { // 按照rowid方式查询
			index_id = -2;
		} else {
			int best_index_id = -1;
			int best_index_priority = 0;
			for (int i = 0; i <= prepared_te->max_index; i++) {
				dbc_ie_t *ie = te_mgr.get_index(i);
				if (ie == NULL)
					continue;

				dbc_index_t& ie_meta = ie->ie_meta;

				int found = 1;
				for (int j = 0; j < ie_meta.field_size; j++) {
					dbc_index_field_t& field = ie_meta.fields[j];

					// 如果是范围字段，忽略结束字段
					// range_group < 0为范围开始字段，> 0为范围结束字段，= 0为普通字段
					if (field.range_group > 0)
						continue;

					if (cond_map.find(field.field_name) == cond_map.end()) {
						// 如果字段找不到，则需要看是否hash键
						if (field.is_hash || !(ie_meta.method_type & METHOD_TYPE_HASH)) {
							found = -1;
							break;
						} else {
							found = 0;
						}
					}
				}

				int current_index_priority = 0;
				if (found == 0) { // 找到可以用作hash查找的索引
					// 对于遍历hash节点上记录的查找，顺序查找的效率较高
					if (ie_meta.method_type & METHOD_TYPE_SEQ)
						current_index_priority |= 0x1;
				} else if (found == 1) { // 索引完全符合，结束查询
					// 对于完全使用索引的查找，主键和唯一所以优先级最高
					// 其次是二分查找
					current_index_priority |= 0x2;
					if (ie_meta.method_type & METHOD_TYPE_BINARY)
						current_index_priority |= 0x4;
					if (ie_meta.index_type & (INDEX_TYPE_PRIMARY | INDEX_TYPE_UNIQUE | INDEX_TYPE_RANGE))
						current_index_priority |= 0x8;
				}

				if (current_index_priority > best_index_priority) {
					best_index_id = i;
					best_index_priority = current_index_priority;
				}
			}

			index_id = best_index_id;
			use_index_hash = !(best_index_priority & 0x4);
		}
	} else {
		index_id = -1;
	}

	dout << "index_id = " << index_id << ", use_index_hash = " << use_index_hash << endl;
}

// 检查条件语句，检查其列表是否为以下形式:
// field_name1 = statement1 [AND field_name2 = statement2 ...]
// 只有这种形式，才能通过索引来查找记录
// 返回true表示需要进行下一步操作再确定是否能使用索引
// 返回false表示不能使用索引
bool Dbc_Statement::cond_is_index_style(relation_item_t *rela_item)
{
	stack<relation_item_t *> cond_stack;
	composite_item_t *left;
	composite_item_t *right;
	simple_item_t *simple_item;
	gen_code_t para(&field_map, bind_fields);
	string& code = para.code;

	while (1) {
		left = rela_item->left;
		right = rela_item->right;

		if (rela_item->op == OPTYPE_AND) {
			cond_stack.push(right->rela_item);
			rela_item = left->rela_item;
			continue;
		} else if (rela_item->op == OPTYPE_EQ) {
			// 作为索引的表达式必须具备以下条件:
			// 1. 表达式的一边为简单类型，而且为字段
			// 2. 表达式的另一边不能有字段出现，也就是可以直接求值的表达式
			// 3. 所有表达式都必须以OPTYPE_AND连接
			simple_item = left->simple_item;
			if (simple_item == NULL || simple_item->word_type != WORDTYPE_IDENT) {
				simple_item = right->simple_item;
				if (simple_item != NULL && simple_item->word_type == WORDTYPE_IDENT) {
					if (cond_map.find(simple_item->value) == cond_map.end()) {
						// 不存在该字段，则作为where条件记录
						code = "";
						left->gen_code(para);
						if (para.can_use_index())
							cond_map[simple_item->value] = code;
					}
				}
			} else {
				if (cond_map.find(simple_item->value) == cond_map.end()) {
					// 不存在该字段，则作为where条件记录
					code = "";
					right->gen_code(para);
					if (para.can_use_index())
						cond_map[simple_item->value] = code;
				}
			}
		}

		// 生成where条件记录
		code = "";
		para.set_use_internal();
		rela_item->gen_code(para);
		para.clear_use_internal();
		cond_codes.push_back(code);

		if (cond_stack.empty())
			return true;

		rela_item = cond_stack.top();
		cond_stack.pop();
	}

	return true;
}

void Dbc_Statement::parse_order(sql_select *stmt)
{
	arg_list_t *order_items = stmt->get_order_items();
	if (order_items == NULL)
		return;

	fmt.str("");
	fmt << "\tint retval;\n\n"
		<<"\tdbc_sqlfun_init();\n\n";
	for (arg_list_t::const_iterator iter = order_items->begin(); iter != order_items->end(); ++iter) {
		composite_item_t *item = *iter;

		gen_code_t para(&field_map, NULL);
		string& code = para.code;
		item->gen_code(para);

		code += ", ";

		para.row_index = GI_ROW_DATA2;
		item->gen_code(para);

		fmt << "\tretval = dbc_compare(" << code << ");\n";

		// 默认是升序
		if (item->order_type == ORDERTYPE_DESC) {
			fmt << "\tif (retval != 0)\n"
				<< "\t\treturn -retval;\n\n";
		} else {
			fmt << "\tif (retval != 0)\n"
				<< "\t\treturn retval;\n\n";
		}
	}

	order_code = fmt.str();
	dout << "order_code = [\n" << order_code << "\n]" << endl;
}

bool Dbc_Statement::is_grouped(sql_select *stmt)
{
	group_codes.clear();
	get_select_map(stmt);

	arg_list_t *group_items = stmt->get_group_items();
	if (group_items == NULL) {
		// 如果没有group by字句，则需要查询选择列表中是否为聚合字段
		bool grouped = false;
		bool grouped_set = false;

		arg_list_t *select_items = stmt->get_select_items();
		for (arg_list_t::const_iterator iter = select_items->begin(); iter != select_items->end(); ++iter) {
			gen_code_t para(&field_map, bind_fields);

			(*iter)->gen_code(para);

			if (!grouped_set) {
				grouped = para.has_agg_func();
				grouped_set = true;
			} else if (grouped != para.has_agg_func()) {
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: some items are not provided in group by sub-statement.")).str(SGLOCALE));
			}
		}

		return grouped;
	}

	for (arg_list_t::const_iterator iter = group_items->begin(); iter != group_items->end(); ++iter) {
		composite_item_t *composite_item = *iter;

		string sql;
		composite_item->gen_sql(sql, true);

		map<string, select_data_t>::const_iterator sel_iter = select_map.find(sql);
		if (sel_iter == select_map.end()) {
			gen_code_t para(&field_map, bind_fields);
			string& code = para.code;

			composite_item->gen_code(para);

			// 保存group列表的代码以与select列表比较
			group_codes.insert(code);
		} else {
			group_codes.insert(sel_iter->second.code);
		}
	}

	return true;
}

void Dbc_Statement::gen_direct_select(sql_select *stmt)
{
	bind_field_t *current_field = select_fields;
	int field_id = 0;

	// 分析order语句
	parse_order(stmt);

	fmt.str("");
	fmt <<"\tdbc_sqlfun_init();\n\n";

	arg_list_t *select_items = stmt->get_select_items();
	for (arg_list_t::const_iterator iter = select_items->begin(); iter != select_items->end(); ++iter, field_id++, current_field++) {
		gen_code_t para(&field_map, bind_fields);
		string& code = para.code;

		(*iter)->gen_code(para);

		string str1;
		string str2;
		string str3;
		gen_format(current_field->field_type, str1, str2, str3);

		if (current_field->field_type != SQLTYPE_STRING && current_field->field_type != SQLTYPE_VSTRING) {
			fmt << "\t" << str3 << str1 << "#" << field_id
				<< "), " << code << ");\n";
		} else {
			fmt << "\t" << str3 << "#" << field_id
				<< ", " << code << ", " << (current_field->field_length - 1) << ");\n";
		}
	}

	set_code = fmt.str();
	dout << "set_code = [\n" << set_code << "\n]" << endl;
}

void Dbc_Statement::gen_group_select(sql_select *stmt)
{
	ostringstream fmt_set;
	ostringstream fmt_agg;
	ostringstream fmt_group;
	ostringstream fmt_gcomp;
	ostringstream fmt_order;

	bind_field_t *current_field = select_fields;
	int field_id = 0;

	get_order_fields(stmt);

	for (; current_field->field_type != SQLTYPE_UNKNOWN; ++current_field, field_id++) {
		map<string, select_data_t>::const_iterator sel_iter = select_map.find(current_field->field_name);
		if (sel_iter == select_map.end())
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: field_name {1} is not in select_map.") % current_field->field_name).str(SGLOCALE));

		const string& code = sel_iter->second.code;
		const string& agg_func_name = sel_iter->second.agg_func_name;
		const int& field_offset = sel_iter->second.field_offset;

		string str1;
		string str2;
		string str3;
		gen_format(current_field->field_type, str1, str2, str3);

		if (current_field->field_type != SQLTYPE_STRING && current_field->field_type != SQLTYPE_VSTRING) {
			if (!agg_func_name.empty()) { // 聚合字段
				fmt_agg << "\tdbc_" << agg_func_name << "("
					<< str1 << "#0 + " << field_offset
					<< "), " << str2 << "$0 + " << field_offset << "));\n";
			} else {
				fmt_gcomp << "\tretval = dbc_compare("
					<< str1 << "#0 + " << field_offset
					<< "), " << str2 << "$0 + " << field_offset << "));\n"
					<< "\tif(retval != 0)\n\t\treturn retval;\n\n";
			}

			fmt_group << "\t" << str3 << str1 << "#0 + " << field_offset
				<< "), " << code << ");\n";
			fmt_set << "\t" << str3 << str1 << "#" << field_id
				<< "), " << str2 << "$0 + " << field_offset << "));\n";
		} else {
			if (!agg_func_name.empty()) { // 聚合字段
				fmt_agg << "\tdbc_" << agg_func_name << "("
					<< "#0 + " << field_offset
					<< ", " << "$0 + " << field_offset << ");\n";
			} else {
				fmt_gcomp << "\tretval = dbc_compare("
					<< "#0 + " << field_offset
					<< ", " << "$0 + " << field_offset << ");\n"
					<< "\tif(retval != 0)\n\t\treturn retval;\n\n";
			}

			fmt_group << str3 << "#0 + " << field_offset
				<< ", " << code << ", " << (current_field->field_length - 1) << ");\n";
			fmt_set << "\t" << str3 << "#" << field_id
				<< ", " << "$0 + " << field_offset << ", " << current_field->field_length << ");\n";
		}
	}

	// 生成排序函数
	for (vector<order_data_t>::const_iterator order_iter = order_fields.begin(); order_iter != order_fields.end(); ++order_iter) {
		map<string, select_data_t>::const_iterator sel_iter = select_map.find(order_iter->field_name);
		if (sel_iter == select_map.end())
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: order field {1} is not in select list.")).str(SGLOCALE));

		const int& field_offset = sel_iter->second.field_offset;
		if (sel_iter->second.field_type != SQLTYPE_STRING && sel_iter->second.field_type != SQLTYPE_VSTRING) {
			string str1;
			string str2;
			string str3;
			gen_format(sel_iter->second.field_type, str1, str2, str3);

			fmt_order << "\tretval = dbc_compare("
				<< str1 << "#0 + " << field_offset
				<< "), " << str2 << "$0 + " << field_offset << "));\n";
		} else {
			fmt_order << "\tretval = dbc_compare("
				<< "#0 + " << field_offset
				<< ", " << "$0 + " << field_offset << ");\n";
		}

		// 默认是升序
		if (order_iter->field_type == ORDERTYPE_DESC) {
			fmt_order << "\tif (retval != 0)\n"
				<< "\t\treturn -retval;\n\n";
		} else {
			fmt_order << "\tif (retval != 0)\n"
				<< "\t\treturn retval;\n\n";
		}
	}

	parse_having(stmt);

	string str = fmt_set.str();
	if (!str.empty()) {
		set_code = "\tdbc_sqlfun_init();\n\n";
		set_code += str;
	}

	str = fmt_agg.str();
	if (!str.empty()) {
		agg_code = "\tdbc_sqlfun_init();\n\n";
		agg_code += str;
	}

	str = fmt_group.str();
	if (!str.empty()) {
		group_code = "\tdbc_sqlfun_init();\n\n";
		group_code += str;
	}

	str = fmt_gcomp.str();
	if (!str.empty()) {
		gcomp_code = "\tint retval;\n\n";
		gcomp_code += "\tdbc_sqlfun_init();\n\n";
		gcomp_code += str;
	}

	str = fmt_order.str();
	if (!str.empty()) {
		order_code = "\tint retval;\n\n";
		order_code += "\tdbc_sqlfun_init();\n\n";
		order_code += str;
	}

	dout << "set_code = [\n" << set_code << "\n]" << endl;
	dout << "agg_code = [\n" << agg_code << "\n]" << endl;
	dout << "group_code = [\n" << group_code << "\n]" << endl;
	dout << "gcomp_code = [\n" << gcomp_code << "\n]" << endl;
	dout << "order_code = [\n" << order_code << "\n]" << endl;
}

void Dbc_Statement::get_order_fields(sql_select *stmt)
{
	order_fields.clear();

	arg_list_t *order_items = stmt->get_order_items();
	if (order_items == NULL)
		return;

	for (arg_list_t::const_iterator iter = order_items->begin(); iter != order_items->end(); ++iter) {
		string sql;
		composite_item_t *composite_item = *iter;
		composite_item->gen_sql(sql, true);

		// 保存order列表的代码以与select列表比较
		order_data_t item;
		if (composite_item->order_type == ORDERTYPE_ASC)
			item.field_name = sql.substr(0, sql.length() - 4); // 去掉结尾的ASC
		else if (composite_item->order_type == ORDERTYPE_DESC)
			item.field_name = sql.substr(0, sql.length() - 5); // 去掉结尾的DESC
		else
			item.field_name = sql;
		item.field_type = composite_item->order_type;
		order_fields.push_back(item);
	}
}

void Dbc_Statement::parse_having(sql_select *stmt)
{
	relation_item_t *having_items = stmt->get_having_items();
	if (having_items == NULL) {
		having_code = "";
		return;
	}

	map<string, sql_field_t> select_field_map;
	for (map<string, select_data_t>::const_iterator iter = select_map.begin(); iter != select_map.end(); ++iter) {
		sql_field_t item;
		item.field_type = iter->second.field_type;
		item.field_offset = iter->second.field_offset;
		item.internal_offset = -1;
		select_field_map[iter->first] = item;
	}

	gen_code_t para(&select_field_map, NULL);
	para.row_index = -1;
	string& code = para.code;
	having_items->gen_code(para);
	if (!code.empty()) {
		having_code = "\tif (";
		having_code += code;
		having_code += ")\n";
		having_code += "\t\treturn 0;\n";
		having_code += "\telse\n";
		having_code += "\t\treturn -1;\n";
	} else {
		having_code = "";
	}

	dout << "having_code = [\n" << having_code << "\n]" << endl;
}

void Dbc_Statement::setRowOffset(ub4 rowOffset_)
{
	rowOffset = rowOffset_;
}

int Dbc_Statement::getRowsErrorCount() const
{
	return iRowsErrorCount;
}

ub4 * Dbc_Statement::getRowsError()
{
	return pRowsError;
}

sb4 * Dbc_Statement::getRowsErrorCode()
{
	return pRowsErrorCode;
}

void Dbc_Statement::get_field_map()
{
	dbc_data_t& te_meta = prepared_te->te_meta;

	field_map.clear();
	for (int i = 0; i < te_meta.field_size; i++) {
		dbc_data_field_t& field = te_meta.fields[i];

		sql_field_t sql_field;
		sql_field.field_type = field.field_type;
		sql_field.field_offset = field.field_offset;
		sql_field.internal_offset = field.internal_offset;
		field_map[field.field_name] = sql_field;
	}
}

void Dbc_Statement::get_select_map(sql_select *stmt)
{
	int field_id = 0;
	int field_offset = 0;
	int align = CHAR_ALIGN;

	select_map.clear();

	arg_list_t *select_items = stmt->get_select_items();
	bind_field_t *current_field = select_fields;
	for (arg_list_t::const_iterator iter = select_items->begin(); iter != select_items->end(); ++iter, field_id++, current_field++) {
		gen_code_t para(&field_map, bind_fields);
		string& code = para.code;

		composite_item_t *composite_item = *iter;
		composite_item->gen_code(para);

		// 对于聚合字段，需要调整其类型，并获得其聚合函数
		string agg_func_name;
		if (para.has_agg_func() && composite_item->func_item) {
			sql_func_t *func_item = composite_item->func_item;
			agg_func_name = func_item->func_name;

			if (current_field->field_type == BIND_DEFAULT_SQLTYPE && current_field->field_length == BIND_DEFAULT_LENGTH) {
				if (agg_func_name == "count" || agg_func_name == "sum" || agg_func_name == "avg") {
					current_field->field_type = SQLTYPE_DOUBLE;
					current_field->field_length = sizeof(double);
				}
			}
		}

		// 根据类型，分析出来字段偏移量
		switch (current_field->field_type) {
		case SQLTYPE_CHAR:
			break;
		case SQLTYPE_UCHAR:
			break;
		case SQLTYPE_SHORT:
		case SQLTYPE_USHORT:
			align = std::max(align, SHORT_ALIGN);
			field_offset = common::round_up(field_offset, SHORT_ALIGN);
			break;
		case SQLTYPE_INT:
		case SQLTYPE_UINT:
			align = std::max(align, INT_ALIGN);
			field_offset = common::round_up(field_offset, INT_ALIGN);
			break;
		case SQLTYPE_LONG:
		case SQLTYPE_ULONG:
			align = std::max(align, LONG_ALIGN);
			field_offset = common::round_up(field_offset, LONG_ALIGN);
			break;
		case SQLTYPE_FLOAT:
			align = std::max(align, FLOAT_ALIGN);
			field_offset = common::round_up(field_offset, FLOAT_ALIGN);
			break;
		case SQLTYPE_DOUBLE:
			align = std::max(align, DOUBLE_ALIGN);
			field_offset = common::round_up(field_offset, DOUBLE_ALIGN);
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			align = std::max(align, TIME_T_ALIGN);
			field_offset = common::round_up(field_offset, TIME_T_ALIGN);
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unknown type given.")).str(SGLOCALE));
		}

		string sql;
		composite_item->gen_sql(sql, true);

		select_data_t item;
		item.code = code;
		item.agg_func_name = agg_func_name;
		item.field_id = field_id;
		item.field_type = current_field->field_type;
		item.field_length = current_field->field_length;
		item.field_offset = field_offset;

		if (composite_item->field_desc) {
			field_desc_t *field_desc = composite_item->field_desc;
			if (!field_desc->alias.empty()) {
				select_map[field_desc->alias] = item;
				// 去掉空格+别名
				sql.resize(sql.length() - field_desc->alias.length() - 1);
			}
		}

		select_map[sql] = item;

		field_offset += current_field->field_length;
	}

	row_size = common::round_up(field_offset, align);
}

void Dbc_Statement::describe()
{
	dbc_data_t& te_meta = prepared_te->te_meta;

	for (bind_field_t *current_field = data->getSelectFields(); current_field->field_type != SQLTYPE_UNKNOWN; current_field++) {
		if (current_field->field_type != SQLTYPE_ANY)
			continue;

		int i;
		for (i = 0; i < te_meta.field_size; i++) {
			const dbc_data_field_t& field = te_meta.fields[i];

			if (strcasecmp(current_field->field_name.c_str(), field.field_name) != 0)
				continue;

			current_field->field_type = field.field_type;
			current_field->field_length = field.field_size;
			break;
		}

		// 默认类型
		if (i == te_meta.field_size) {
			current_field->field_type = BIND_DEFAULT_SQLTYPE;
			current_field->field_length = BIND_DEFAULT_LENGTH;
		}
	}

	data->alloc_select();
}

void Dbc_Statement::gen_format(sqltype_enum field_type, string& str1, string& str2, string& str3)
{
	// 根据类型，分析出来字段偏移量
	switch (field_type) {
	case SQLTYPE_CHAR:
		str1 = "*reinterpret_cast<char *>(";
		str2 = "*reinterpret_cast<const char *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_UCHAR:
		str1 = "*reinterpret_cast<unsigned char *>(";
		str2 = "*reinterpret_cast<const unsigned char *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_SHORT:
		str1 = "*reinterpret_cast<short *>(";
		str2 = "*reinterpret_cast<const short *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_USHORT:
		str1 = "*reinterpret_cast<unsigned short *>(";
		str2 = "*reinterpret_cast<const unsigned short *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_INT:
		str1 = "*reinterpret_cast<int *>(";
		str2 = "*reinterpret_cast<const int *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_UINT:
		str1 = "*reinterpret_cast<unsigned int *>(";
		str2 = "*reinterpret_cast<const unsigned int *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_LONG:
		str1 = "*reinterpret_cast<long *>(";
		str2 = "*reinterpret_cast<const long *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_ULONG:
		str1 = "*reinterpret_cast<unsigned long *>(";
		str2 = "*reinterpret_cast<const unsigned long *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_FLOAT:
		str1 = "*reinterpret_cast<float *>(";
		str2 = "*reinterpret_cast<const float *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_DOUBLE:
		str1 = "*reinterpret_cast<double *>(";
		str2 = "*reinterpret_cast<const double *>(";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		str1 = "";
		str2 = "";
		str3 = "dbc_assign(";
		break;
	case SQLTYPE_DATE:
		str1 = "*reinterpret_cast<time_t *>(";
		str2 = "*reinterpret_cast<const time_t *>(";
		str3 = "dbc_date_assign(";
	case SQLTYPE_TIME:
		str1 = "*reinterpret_cast<time_t *>(";
		str2 = "*reinterpret_cast<const time_t *>(";
		str3 = "dbc_time_assign(";
	case SQLTYPE_DATETIME:
		str1 = "*reinterpret_cast<time_t *>(";
		str2 = "*reinterpret_cast<const time_t *>(";
		str3 = "dbc_datetime_assign(";
		break;
	case SQLTYPE_UNKNOWN:
	case SQLTYPE_ANY:
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unknown type given.")).str(SGLOCALE));
	}
}

Dbc_ResultSet::Dbc_ResultSet(Dbc_Statement *stmt_)
	: stmt(stmt_),
	  cursor(stmt->cursor)
{
	DBCT = dbct_ctx::instance();

	fetched_rows = 0;
	batch_status = FIRST_DATA_CALL;
	iteration = -1;

	if (stmt->group_func != NULL) {
		group();
		use_group = true;
	} else if (stmt->order_func != NULL) {
		cursor->order(stmt->order_func);
		use_group = false;
	} else {
		use_group = false;
	}
}

Dbc_ResultSet::~Dbc_ResultSet()
{
	for (vector<char *>::iterator iter = data_vector.begin(); iter != data_vector.end(); ++iter)
		delete [](*iter);
}

dbtype_enum Dbc_ResultSet::get_dbtype()
{
	return DBTYPE_DBC;
}

Dbc_ResultSet::Status Dbc_ResultSet::next()
{
	if (batch_status == FIRST_DATA_CALL)
		batchNext();

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

Dbc_ResultSet::Status Dbc_ResultSet::batchNext()
{
	int j;
	int array_size = data->size();

	if (batch_status == END_OF_FETCH) { // 已到末尾
		fetched_rows = 0;
	} else if (use_group) { // group by的情况
		for (int i = 0; i < array_size; i++, ++curr_data) {
			if (curr_data == data_vector.end()) {
				if (i > 0) {
					for (j = 0; j < select_count; j++)
						DBCT->_DBCT_set_binds[j] -= i * select_fields[j].field_length;
				}

				batch_status = END_OF_FETCH;
				fetched_rows = i;
				return batch_status;
			}

			if (i > 0) {
				for (j = 0; j < select_count; j++)
					DBCT->_DBCT_set_binds[j] += select_fields[j].field_length;
			}

			const char *in[1] = { *curr_data };
			(*stmt->set_func)(NULL, NULL, in, DBCT->_DBCT_set_binds);
		}

		for (j = 0; j < select_count; j++)
			DBCT->_DBCT_set_binds[j] -= (array_size - 1) * select_fields[j].field_length;

		batch_status = DATA_AVAILABLE;
		fetched_rows = array_size;
	} else { // 其它情况
		char *global[3];
		global[GI_DATA_START] = DBCT->_DBCT_data_start;
		global[GI_TIMESTAMP] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);
		for (int i = 0; i < array_size; i++) {
			if (!cursor->next()) {
				if (i > 0) {
					for (j = 0; j < select_count; j++)
						DBCT->_DBCT_set_binds[j] -= i * select_fields[j].field_length;
				}

				batch_status = END_OF_FETCH;
				fetched_rows = i;
				return batch_status;
			}

			if (i > 0) {
				for (j = 0; j < select_count; j++)
					DBCT->_DBCT_set_binds[j] += select_fields[j].field_length;
			}

			DBCT->enter_execute();
			global[GI_ROW_DATA] = const_cast<char *>(cursor->data());
			(*stmt->set_func)(NULL, global, NULL, DBCT->_DBCT_set_binds);
		}

		for (j = 0; j < select_count; j++)
			DBCT->_DBCT_set_binds[j] -= (array_size - 1) * select_fields[j].field_length;

		batch_status = DATA_AVAILABLE;
		fetched_rows = array_size;
	}

	return batch_status;
}

int Dbc_ResultSet::getFetchRows() const
{
	return fetched_rows;
}

bool Dbc_ResultSet::isNull(int colIndex)
{
	return false;
}

bool Dbc_ResultSet::isTruncated(int colIndex)
{
	return false;
}

date Dbc_ResultSet::date2native(void *x) const
{
	return date(*reinterpret_cast<time_t *>(x));
}

ptime Dbc_ResultSet::time2native(void *x) const
{
	return ptime(*reinterpret_cast<time_t *>(x));
}

datetime Dbc_ResultSet::datetime2native(void *x) const
{
	return datetime(*reinterpret_cast<time_t *>(x));
}

sql_datetime Dbc_ResultSet::sqldatetime2native(void *x) const
{
	datetime dt(*reinterpret_cast<time_t *>(x));
	return sql_datetime(dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
}

void Dbc_ResultSet::group()
{
	set<group_data_t>::iterator iter;
	group_data_t item;
	char *global[3];
	global[GI_DATA_START] = DBCT->_DBCT_data_start;
	global[GI_TIMESTAMP] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);

	DBCT->_DBCT_group_func = stmt->gcomp_func;

	while (cursor->next()) {
		DBCT->enter_execute();
		global[GI_ROW_DATA] = const_cast<char *>(cursor->data());

		item.data = new char[stmt->row_size];
		item.rows = 1;
		(*stmt->group_func)(NULL, global, NULL, &item.data);

		if (stmt->having_func != NULL && (*stmt->having_func)(NULL, NULL, NULL, const_cast<char **>(&item.data)) == 0) {
			delete []item.data;
			continue;
		}

		set<group_data_t>::iterator iter = group_set.find(item);
		if (iter == group_set.end()) {
			group_set.insert(item);
		} else {
			group_data_t& current = const_cast<group_data_t&>(*iter);
			DBCT->_DBCT_result_set_size = ++current.rows;
			const char *in[1] = { item.data };
			(*stmt->agg_func)(NULL, NULL, in, &current.data);
		}
	}

	DBCT->_DBCT_group_func = NULL;

	if (stmt->group_codes.empty() && group_set.empty()) {
		char *data = new char[stmt->row_size];
		memset(data, '\0', stmt->row_size);
		data_vector.push_back(data);
	} else {
		for (iter = group_set.begin(); iter != group_set.end(); ++iter)
			data_vector.push_back(iter->data);
		group_set.clear();

		// 组内排序
		if (stmt->order_func)
			std::sort(data_vector.begin(), data_vector.end(), group_order_compare(stmt->order_func));
	}

	curr_data = data_vector.begin();
}

DECLARE_DATABASE("DBC", Dbc_Database)

}
}

