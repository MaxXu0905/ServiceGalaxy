/*
 * GP_Database.cpp
 *
 *  Created on: 2012-8-28
 *      Author: Administrator
 */

#include "gp_control.h"
#include "gp_strconv.h"
const char TIMEFORMAT[] = "%H:%M:%S";
const char DATEFORMAT[] = "%Y-%m-%d";
const char TIMEDATEFORMAT[] = "%Y-%m-%d %H:%M:%S";
namespace ai {
namespace scci {

//绑定与选择域类型
typedef short gp_field_index;
GP_Database::GP_Database() :
		_conn(0), statmentId(0) {
	begined = false;

}
GP_Database::~GP_Database() {
	disconnect();
}
void GP_Database::begin() {
	if (begined)
		return;
	execCmd(gp_sql_begin);
	begined = true;
}
namespace {
void addConnStr(string & connstr, const map<string, string>& conn_info
		,const string & atr) {
	map<string, string>::const_iterator iter;
	iter = conn_info.find(atr);
	if (iter != conn_info.end()) {
		connstr += atr + "='";
		connstr += iter->second;
		connstr += "' ";
	}
}
}
void GP_Database::connect(const map<string, string>& conn_info) {
	if (_conn)
		return;
	string connstr = "";
	addConnStr(connstr, conn_info, "host");
	addConnStr(connstr, conn_info, "hostaddr");
	addConnStr(connstr, conn_info, "port");
	addConnStr(connstr, conn_info, "dbname");
	addConnStr(connstr, conn_info, "user");
	addConnStr(connstr, conn_info, "password");
	addConnStr(connstr, conn_info, "connect_timeout");
	_conn = PQconnectdb(connstr.c_str());
	if (PQstatus(_conn) != CONNECTION_OK) {
		const string msg(PQerrorMessage(_conn));
		PQfinish(_conn);
		_conn = 0;
		throw bad_param(__FILE__, __LINE__, 0, msg);
	}
	map<string, string>::const_iterator iter;
	iter = conn_info.find("encoding");
	if (iter != conn_info.end()) {
		if (PQsetClientEncoding(_conn, iter->second.c_str())) {
			throw bad_param(__FILE__, __LINE__, 0,
					"not support encoding" + iter->second);
		}
	}
	iter = conn_info.find("search_path");
	if (iter != conn_info.end()) {
		string search_pathCmd = gp_sql_search_path + iter->second;
		execCmd(search_pathCmd.c_str());
	}
	//设置时间格式
	execCmd(gp_sql_datestyle);
}
void GP_Database::disconnect() {
	if (!_conn) {
		return;
	}
	PQfinish(_conn);
	_conn = 0;
}
void GP_Database::commit() {
	execCmd(gp_sql_commit);
	begined = false;
}
void GP_Database::rollback() {
	execCmd(gp_sql_rollback);
	begined = false;
}

void GP_Database::execCmd(const string &command) {
	if (!_conn)
		throw bad_param(__FILE__, __LINE__, 0,
				(_("ERROR: database not connected!")).str(SGLOCALE));
	PGresult *res = PQexec(_conn, command.c_str());
	int recode = PQresultStatus(res);
	if (recode != PGRES_COMMAND_OK) {
		PQclear(res);
		const string msg(PQerrorMessage(_conn));
		throw bad_sql(__FILE__, __LINE__, 0, recode, msg);
	}
	PQclear(res);
}
Generic_Statement * GP_Database::create_statement() {
	if (!_conn)
		throw bad_param(__FILE__, __LINE__, 0,
				(_("ERROR: database not connected!")).str(SGLOCALE));
	statmentId++;
	return new GP_Statement(this, "statment_" + gp::to_string(statmentId));
}
void GP_Database::terminate_statement(Generic_Statement *& stmt) {
	delete stmt;
	stmt = 0;
}
dbtype_enum GP_Database::get_dbtype() {
	return DBTYPE_GP;
}

void GP_Database::prepare(const string &statementName, const string &sql) {
	if (!_conn)
		throw bad_param(__FILE__, __LINE__, 0,
				(_("ERROR: database not connected!")).str(SGLOCALE));
	PGresult *res = PQprepare(_conn, statementName.c_str(), sql.c_str(), 0, 0);
	int recode = PQresultStatus(res);
	if (recode != PGRES_COMMAND_OK) {
		PQclear(res);
		const string msg(PQerrorMessage(_conn));
		throw bad_sql(__FILE__, __LINE__, 0, recode, msg);
	}
	PQclear(res);
}

PGresult * GP_Database::prepare_exec(const string &statementName,
		const char * const params[], const int paramlengths[],
		const int binary[], int nparams) {
	if (!_conn)
		throw bad_param(__FILE__, __LINE__, 0,
				(_("ERROR: database not connected!")).str(SGLOCALE));
	PGresult *res = PQexecPrepared(_conn, statementName.c_str(), nparams,
			params, paramlengths, binary, 0);
	int recode = PQresultStatus(res);
	if (recode != PGRES_COMMAND_OK && recode != PGRES_TUPLES_OK) {
		const string msg(PQerrorMessage(_conn));
		PQclear(res);
		throw bad_sql(__FILE__, __LINE__, 0, recode, msg);
	}
	return res;
}

string GP_Database::get_seq_item(const string& seq_name, int seq_action) {
	string seq_item;
	switch (seq_action) {
	case SEQ_ACTION_CURRVAL:
		seq_item = "currval('";
		seq_item += seq_name;
		seq_item += "')";
		break;
	case SEQ_ACTION_NEXTVAL:
		seq_item = "nextval('";
		seq_item += seq_name;
		seq_item += "')";
		break;
	default:
		return "";
	}

	return seq_item;
}

GP_Statement::GP_Statement(GP_Database *db, const string &statmentName) :
		_db(db), _statmentName(statmentName) {
	pRowsError = 0;
	iRowsErrorCount = 0;
	pRowsErrorCode = 0;
	prepared = false;
}
GP_Statement::~GP_Statement() {
	try {
		if (prepared) {
			_db->execCmd(gp_sql_deallcate + _statmentName);
		}
		if (pRowsError) {
			delete[] pRowsError;
			pRowsError = 0;
		}
		if (pRowsErrorCode) {
			delete[] pRowsErrorCode;
			pRowsErrorCode = 0;
		}
	} catch (exception& ex) {
		// 忽略错误
	}
}

dbtype_enum GP_Statement::get_dbtype() {
	return DBTYPE_GP;
}

void GP_Statement::pushPrepareData(bind_field_t * bind_field_, int bind_count_,
		int index_) {
	_nonnull.clear();
	_values.clear();
	_binary.clear();
	for (int i = 0; i < bind_count_; i++) {
		if (bind_field_[i].ind_offset == -1) {
			_nonnull.push_back(true);
		} else {
			gp_field_index *inds =
					reinterpret_cast<gp_field_index *>(reinterpret_cast<char *>(data)
							+ bind_field_[i].ind_offset);
			if (inds[index_] == -1) {
				_nonnull.push_back(false);
			} else {
				_nonnull.push_back(true);
			}
		}
		_binary.push_back(false);
		switch (bind_field_[i].field_type) {
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			_values.push_back(
					gp::to_string(
							reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset
									+ index_ * bind_field_[i].field_length));
			break;
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR:
			_values.push_back(
					gp::to_string(
							(reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset)[index_]));
			break;
		case SQLTYPE_SHORT:
			_values.push_back(
					gp::to_string(
							reinterpret_cast<short *>(reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset)[index_]));
			break;
		case SQLTYPE_USHORT:
			_values.push_back(
					gp::to_string(
							reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset)[index_]));
			break;
		case SQLTYPE_INT:
			_values.push_back(
					gp::to_string(
							reinterpret_cast<int *>(reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset)[index_]));
			break;
		case SQLTYPE_UINT:
			_values.push_back(
					gp::to_string(
							reinterpret_cast<unsigned int *>(reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset)[index_]));
			break;
		case SQLTYPE_LONG:
			_values.push_back(
					gp::to_string(
							reinterpret_cast<long *>(reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset)[index_]));
			break;
		case SQLTYPE_ULONG:
			_values.push_back(
					gp::to_string(
							reinterpret_cast<unsigned long *>(reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset)[index_]));
			break;
		case SQLTYPE_FLOAT:
			_values.push_back(
					gp::to_string(
							reinterpret_cast<float *>(reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset)[index_]));
			break;
		case SQLTYPE_DOUBLE:
			_values.push_back(
					gp::to_string(
							reinterpret_cast<double *>(reinterpret_cast<char *>(data)
									+ bind_field_[i].field_offset)[index_]));
			break;
		case SQLTYPE_DATE:
			time_t datetim;
			datetim = reinterpret_cast<time_t *>(reinterpret_cast<char *>(data)
					+ bind_field_[i].field_offset)[index_];
			tm datetm;
			char datebuf[32];
			localtime_r(&datetim, &datetm);
			strftime(datebuf, sizeof(datebuf), TIMEFORMAT, &datetm);
			_values.push_back(datebuf);
			break;
		case SQLTYPE_TIME:
			time_t timetim;
			timetim = reinterpret_cast<time_t *>(reinterpret_cast<char *>(data)
					+ bind_field_[i].field_offset)[index_];
			struct tm timetm;
			char timebuf[32];
			localtime_r(&timetim, &timetm);
			strftime(timebuf, sizeof(timebuf), "%DATEFORMAT", &timetm);
			_values.push_back(timebuf);
			break;
		case SQLTYPE_DATETIME:
			time_t datetimetim;
			datetimetim =
					reinterpret_cast<time_t *>(reinterpret_cast<char *>(data)
							+ bind_field_[i].field_offset)[index_];
			tm datetimetm;
			char datetimebuf[32];
			localtime_r(&datetimetim, &datetimetm);
			strftime(datetimebuf, sizeof(datetimebuf), TIMEDATEFORMAT,
					&datetimetm);
			_values.push_back(datetimebuf);
			break;
		default:
			break;
		}
	}
}
//生成prepare数组
int GP_Statement::marshall(scoped_array<const char *> &values,
		scoped_array<int> &lengths, scoped_array<int> &binaries) {
	const size_t elements = _nonnull.size();
	values.reset(new const char *[elements + 1]);
	lengths.reset(new int[elements + 1]);
	binaries.reset(new int[elements + 1]);
	int v = 0;
	for (int i = 0; i < elements; ++i) {
		if (_nonnull[i]) {
			values[i] = _values[v].c_str();
			lengths[i] = int(_values[v].size());
			++v;
		} else {
			values[i] = 0;
			lengths[i] = 0;
		}
		binaries[i] = int(_binary[i]);
	}
	values[elements] = 0;
	lengths[elements] = 0;
	binaries[elements] = 0;
	return int(elements);
}
Generic_ResultSet * GP_Statement::executeQuery() {
	if (data == NULL)
		throw bad_system(__FILE__, __LINE__, 0, 0,
				(_("ERROR: bind() should be called before executeQuery()")).str(SGLOCALE));
	pushPrepareData(bind_fields, bind_count);
	scoped_array<const char *> params;
	scoped_array<int> lens;
	scoped_array<int> binaries;
	int nparams = marshall(params, lens, binaries);
	PGresult *res = _db->prepare_exec(_statmentName, params.get(), lens.get(),
			binaries.get(), nparams);
	GP_ResultSet * grset = new GP_ResultSet(res);
	grset->bind(data, index);
	return grset;

}
void GP_Statement::execute(int arrayLength) {
	_db->begin();
	_updateCount = 0;
	iRowsErrorCount = 0;
	for (int i = 0; i < arrayLength; i++) {
		pushPrepareData(bind_fields, bind_count, i);
		scoped_array<const char *> params;
		scoped_array<int> lens;
		scoped_array<int> binaries;
		int nparams = marshall(params, lens, binaries);
//		try {
		PGresult *res = _db->prepare_exec(_statmentName, params.get(),
				lens.get(), binaries.get(), nparams);
		int updated = 0;
		gp::from_string(PQcmdTuples(res), updated);
		_updateCount += updated;
		PQclear(res);
//		} catch (const bad_sql & e) {
//			pRowsError[iRowsErrorCount++] = i;
//			pRowsErrorCode[iRowsErrorCount++] = e.get_error_code();
//		}
	}

}
int GP_Statement::getUpdateCount() const {
	return _updateCount;
}
void GP_Statement::executeUpdate() {
	execute(iteration + 1);
	iteration = 0;

}
void GP_Statement::executeArrayUpdate(int arrayLength) {
	execute(arrayLength);
}
void GP_Statement::closeResultSet(Generic_ResultSet *& rset) {
	delete rset;
	rset = 0;
}
bool GP_Statement::isNull(int colIndex) {
	if (colIndex <= 0 || colIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		gp_field_index *ind =
				reinterpret_cast<gp_field_index *>(reinterpret_cast<char *>(data)
						+ field.ind_offset);
		if (ind[iteration] == -1)
			return true;
		else
			return false;
	}
}
bool GP_Statement::isTruncated(int colIndex) {
	return false;
}
void GP_Statement::setNull(int paramIndex) {
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset == -1) // No indicator
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Field indicator is not defined")).str(SGLOCALE));

	gp_field_index *ind = reinterpret_cast<gp_field_index *>(data)
			+ field.ind_offset + field.ind_length * iteration;
	*ind = -1;
}
void GP_Statement::setExecuteMode(execute_mode_enum execute_mode_) {

}
execute_mode_enum GP_Statement::getExecuteMode() const {
	return EM_DEFAULT;
}

// Not available for generic interface
sb4 GP_Statement::checkNonblockingResult() {
	throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: not support!")).str(SGLOCALE));
	return 0;
}

date GP_Statement::date2native(void *x) const {
	return date(*reinterpret_cast<time_t *>(x));
}

ptime GP_Statement::time2native(void *x) const {
	return ptime(*reinterpret_cast<time_t *>(x));
}

datetime GP_Statement::datetime2native(void *x) const {
	return datetime(*reinterpret_cast<time_t *>(x));
}

sql_datetime GP_Statement::sqldatetime2native(void *x) const {
	datetime dt(*reinterpret_cast<time_t *>(x));
	return sql_datetime(dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(),
			dt.second());
}

void GP_Statement::native2date(void *addr, const date& x) const {
	time_t *dt = reinterpret_cast<time_t *>(addr);
	*dt = x.duration();
}

void GP_Statement::native2time(void *addr, const ptime& x) const {
	time_t *dt = reinterpret_cast<time_t *>(addr);
	*dt = x.duration();
}

void GP_Statement::native2datetime(void *addr, const datetime& x) const {
	time_t *dt = reinterpret_cast<time_t *>(addr);
	*dt = x.duration();
}

void GP_Statement::native2sqldatetime(void *addr, const sql_datetime& x) const {
	time_t *dt = reinterpret_cast<time_t *>(addr);
	*dt = datetime(x.year(), x.month(), x.day(), x.hour(), x.minute(),
			x.second()).duration();
}

void GP_Statement::setRowOffset(ub4 rowOffset_) {
	rowOffset = rowOffset_;
}
//出错总行数
int GP_Statement::getRowsErrorCount() const {
	return iRowsErrorCount;
}
//行号
ub4 * GP_Statement::getRowsError() {
	return pRowsError;
}
//错误代码
sb4 * GP_Statement::getRowsErrorCode() {
	return pRowsErrorCode;
}

void GP_Statement::prepare() {
	if (!prepared) {
		if (pRowsError) {
			delete[] pRowsError;
		}
		pRowsError = new ub4[data->size()];
		if (pRowsErrorCode) {
			delete[] pRowsErrorCode;
		}
		pRowsErrorCode = new sb4[data->size()];
		_db->prepare(_statmentName, data->getSQL());
		prepared = true;
	}
}
void GP_Statement::clearNull(char *addr) {
	reinterpret_cast<gp_field_index *>(addr)[iteration] = 0;
}

GP_ResultSet::GP_ResultSet(PGresult * res) :
		_res(res) {
	_fetched_rows = 0;
	_status = FIRST_DATA_CALL;
	iteration = -1;
	_curRow = 0;
	_rowCount = PQntuples(_res);
}
GP_ResultSet::~GP_ResultSet() {
	PQclear(_res);
}
dbtype_enum GP_ResultSet::get_dbtype() {
	return DBTYPE_GP;
}

Generic_ResultSet::Status GP_ResultSet::next() {
	if (_status == FIRST_DATA_CALL)
		batchNext();
	if (++iteration < _fetched_rows)
		return DATA_AVAILABLE;
	if (_status) {
		batchNext();
		if (_fetched_rows == 0) {
			return END_OF_FETCH;
		} else {
			iteration = 0;
			return DATA_AVAILABLE;
		}
	} else {
		return END_OF_FETCH;
	}
}
namespace {
void pushData(PGresult *_res, struct_base *data,
		const bind_field_t &select_field, int row/*写入行*/, int pgrow,
		int index) {

	if (PQgetisnull(_res, pgrow, index)) {
		if (select_field.ind_offset != -1) {
			gp_field_index *inds =
					reinterpret_cast<gp_field_index *>(reinterpret_cast<char *>(data)
							+ select_field.ind_offset);
			inds[row] = -1;
		}
		switch (select_field.field_type) {
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			(reinterpret_cast<char *>(data) + select_field.field_offset
					+ row * select_field.field_length)[0] = '\0';
			break;
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR:
			(reinterpret_cast<char *>(data) + select_field.field_offset)[row] =
					'\0';
			break;
		case SQLTYPE_SHORT:
			reinterpret_cast<short *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = 0;
			break;
		case SQLTYPE_USHORT:
			reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = 0;
			break;
		case SQLTYPE_INT:
			reinterpret_cast<int *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = 0;
			break;
		case SQLTYPE_UINT:
			reinterpret_cast<unsigned int *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = 0;
			break;
		case SQLTYPE_LONG:
			reinterpret_cast<long *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = 0L;
			break;
		case SQLTYPE_ULONG:
			reinterpret_cast<unsigned long *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = 0UL;
			break;
		case SQLTYPE_FLOAT:
			reinterpret_cast<float *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = 0.0;
			break;
		case SQLTYPE_DOUBLE:
			reinterpret_cast<double *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = 0.0;
			break;
		default:
			break;
		}

	} else {
		if (select_field.ind_offset != -1) {
			gp_field_index *inds =
					reinterpret_cast<gp_field_index *>(reinterpret_cast<char *>(data)
							+ select_field.ind_offset);
			inds[row] = 0;
		}
		char *value = PQgetvalue(_res, pgrow, index);
		switch (select_field.field_type) {
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			strncpy(
					(reinterpret_cast<char *>(data) + select_field.field_offset
							+ row * select_field.field_length), value,
					select_field.field_length - 1);
			(reinterpret_cast<char *>(data) + select_field.field_offset
					+ row * select_field.field_length)[select_field.field_length
					- 1] = '\0';
			break;
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR: {
			char charv;
			gp::from_string(value, charv);
			(reinterpret_cast<char *>(data) + select_field.field_offset)[row] =
					charv;
			break;
		}

		case SQLTYPE_SHORT: {
			short shortv;
			gp::from_string(value, shortv);
			reinterpret_cast<short *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = shortv;
			break;
		}
		case SQLTYPE_USHORT: {
			unsigned short ushort;
			gp::from_string(value, ushort);
			reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = ushort;
			break;
		}
		case SQLTYPE_INT: {
			int intv;
			gp::from_string(value, intv);
			reinterpret_cast<int *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = intv;
			break;
		}
		case SQLTYPE_UINT: {
			unsigned int uintv;
			gp::from_string(value, uintv);
			reinterpret_cast<unsigned int *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = uintv;
			break;
		}
		case SQLTYPE_LONG: {
			long longv;
			gp::from_string(value, longv);
			reinterpret_cast<long *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = longv;
			break;
		}
		case SQLTYPE_ULONG: {
			unsigned long ulongv;
			gp::from_string(value, ulongv);
			reinterpret_cast<unsigned long *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = ulongv;
			break;
		}
		case SQLTYPE_FLOAT: {
			float floatv;
			gp::from_string(value, floatv);
			reinterpret_cast<float *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = floatv;
			break;
		}
		case SQLTYPE_DOUBLE: {
			double doublev;
			gp::from_string(value, doublev);
			reinterpret_cast<double *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = doublev;
			break;
		}

		case SQLTYPE_DATE: {
			struct tm datetm;
			strptime(value, DATEFORMAT, &datetm);
			reinterpret_cast<time_t *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = mktime(&datetm);
			break;
		}
		case SQLTYPE_TIME: {
			struct tm timetm;
			strptime(value, TIMEFORMAT, &timetm);
			reinterpret_cast<time_t *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = mktime(&timetm);
			break;
		}
		case SQLTYPE_DATETIME: {
			struct tm datetimetm;
			strptime(value, TIMEDATEFORMAT, &datetimetm);
			reinterpret_cast<time_t *>(reinterpret_cast<char *>(data)
					+ select_field.field_offset)[row] = mktime(&datetimetm);
			break;
		}
		default:
			break;

		}
	}
}
}
Generic_ResultSet::Status GP_ResultSet::batchNext() {
	_fetched_rows = 0;
	for (; _fetched_rows < data->size() && _curRow < _rowCount;
			_fetched_rows++, _curRow++) {
		for (int j = 0; j < select_count; j++) {
			pushData(_res, data, select_fields[j], _fetched_rows, _curRow, j);
		}
	}
	if (_fetched_rows) {
		_status = DATA_AVAILABLE;
	} else {
		_status = END_OF_FETCH;
	}
	return _status;

}
int GP_ResultSet::getFetchRows() const {
	return _fetched_rows;
}

bool GP_ResultSet::isNull(int colIndex) {
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	if (field.ind_offset == -1) { // No indicator
		return false;
	} else {
		gp_field_index *ind =
				reinterpret_cast<gp_field_index *>(reinterpret_cast<char *>(data)
						+ field.ind_offset);
		if (ind[iteration] == -1)
			return true;
		else
			return false;
	}
}
bool GP_ResultSet::isTruncated(int paramIndex) {
	return false;
}
date GP_ResultSet::date2native(void *x) const {
	return date(*reinterpret_cast<time_t *>(x));
}

ptime GP_ResultSet::time2native(void *x) const {
	return ptime(*reinterpret_cast<time_t *>(x));
}

datetime GP_ResultSet::datetime2native(void *x) const {
	return datetime(*reinterpret_cast<time_t *>(x));
}

sql_datetime GP_ResultSet::sqldatetime2native(void *x) const {
	datetime dt(*reinterpret_cast<time_t *>(x));
	return sql_datetime(dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(),
			dt.second());
}

DECLARE_DATABASE("GP", GP_Database)

} /* namespace scci */
} /* namespace ai */

