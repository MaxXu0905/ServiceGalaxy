#include "dbc_internal.h"
#include "dbc_sync.h"

namespace ai
{
namespace sg
{

dbc_sync::dbc_sync()
	: api_mgr(dbc_api::instance(DBCT))
{
	DBCP->_DBCP_is_sync = true;

	if (!api_mgr.login()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: dbc_sync: Can't login on BB.")).str(SGLOCALE));
		exit(1);
	}

	db = NULL;

	for (int i = 0; i < SQL_CTX_TOTAL; i++) {
		sql_ctx[i].stmt = NULL;
		sql_ctx[i].dynamic_data = NULL;
		sql_ctx[i].execute_count = 0;
	}
}

dbc_sync::~dbc_sync()
{
	if (db != NULL) {
		for (int i = 0; i < SQL_CTX_TOTAL; i++) {
			db->terminate_statement(sql_ctx[i].stmt);
			delete sql_ctx[i].dynamic_data;
		}
		delete db;
	}

	api_mgr.logout();
}

void dbc_sync::run()
{
	if (!DBCP->_DBCP_is_sync)
		return;

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	while (1) {
		time_t end_timestamp = time(0) + bbparms.scan_unit;

		if (!run_once())
			return;

		while (end_timestamp > time(0)) {
			sleep(end_timestamp - time(0));
		}
	}
}

bool dbc_sync::run_once()
{
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = NULL;

	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, "", &retval);
#endif
	dbc_sqlite& sqlite_mgr = dbc_sqlite::instance(DBCT);

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	string db_name = (boost::format("%1%sqlite_dbc.sync") % bbparms.sync_dir).str();

	if (!sqlite_mgr.open(db_name, sqlite_db))
		return retval;

	// 关闭打开的数据库；如果处理结果成功，则删除创建的数据库
	BOOST_SCOPE_EXIT((&sqlite_db)(&db_name)(&retval)) {
		sqlite3_close(sqlite_db);
		if (retval)
			unlink(db_name.c_str());
	} BOOST_SCOPE_EXIT_END

	if (!sqlite_mgr.move2sync(sqlite_db, bbmeters.tran_id))
		return retval;

	if (!sqlite_mgr.prepare(sqlite_db, sqlite_stmt, DBC_SQLITE_SYNC))
		return retval;

	// 关闭select语句
	BOOST_SCOPE_EXIT((&sqlite_stmt)) {
		sqlite3_finalize(sqlite_stmt);
	} BOOST_SCOPE_EXIT_END

	int orig_user_idx = -1;
	int orig_table_id = -1;
	int type;
	int user_idx;
	int table_id;
	int row_size;
	int old_row_size;
	const char *row_data;
	const char *old_row_data;

	// 重置源表
	if (sqlite3_reset(sqlite_stmt) != SQLITE_OK)
		return retval;

	while (1) {
		switch (sqlite3_step(sqlite_stmt)) {
		case SQLITE_ROW:
			type = sqlite3_column_int(sqlite_stmt, 0);
			user_idx = sqlite3_column_int(sqlite_stmt, 1);
			table_id = sqlite3_column_int(sqlite_stmt, 2);
			row_size = sqlite3_column_bytes(sqlite_stmt, 3);
			old_row_size = sqlite3_column_bytes(sqlite_stmt, 4);
			row_data = reinterpret_cast<const char *>(sqlite3_column_text(sqlite_stmt, 3));
			old_row_data = reinterpret_cast<const char *>(sqlite3_column_text(sqlite_stmt, 4));

			if (user_idx != orig_user_idx) {
				ue_mgr.set_ctx(user_idx);
				orig_user_idx = user_idx;
				orig_table_id = -1;

				executeUpdate();
				create_db(ue_mgr.get_ue());
			}

			if (table_id != orig_table_id) {
				te = ue_mgr.get_table(table_id);
				te_mgr.set_ctx(te);
				orig_table_id = table_id;

				executeUpdate();
				create_stmt(te, UPDATE_TYPE_INSERT);
				create_stmt(te, UPDATE_TYPE_UPDATE);
				create_stmt(te, UPDATE_TYPE_DELETE);
			}

			switch (type) {
			case UPDATE_TYPE_INSERT:
				execute_insert(te, row_data);
				break;
			case UPDATE_TYPE_UPDATE:
				execute_update(te, row_data, old_row_data);
				break;
			case UPDATE_TYPE_DELETE:
				execute_delete(te, row_data);
				break;
			default:
				DBCT->_DBCT_error_code = DBCEDIAGNOSTIC;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error")).str(SGLOCALE));
				return false;
			}
			break;
		case SQLITE_BUSY:
			boost::this_thread::yield();
			break;
		case SQLITE_DONE:
			retval = true;
			return retval;
		default:
			return retval;
		}
	}

	executeUpdate();

	retval = true;
	return retval;
}

void dbc_sync::create_db(dbc_ue_t *ue)
{
	map<string, string> conn_info;

	database_factory& factory_mgr = database_factory::instance();
	factory_mgr.parse(ue->openinfo, conn_info);

	if (db == NULL)
		db = factory_mgr.create(ue->dbname);
	else
		db->disconnect();

	db->connect(conn_info);
}

void dbc_sync::create_stmt(dbc_te_t *te, int type)
{
	const dbc_data_t& te_meta = te->te_meta;
	string sql_str;
	bool first;
	int i;
	vector<column_desc_t> binds;
	sql_struct_t *sql_struct;

	switch (type) {
	case UPDATE_TYPE_INSERT:
		sql_str = "insert into ";
		sql_str += te_meta.table_name;
		sql_str += "(";
		for (i = 0; i < te_meta.field_size; i++) {
			if (i != 0)
				sql_str += ", ";
			sql_str += te_meta.fields[i].column_name;
		}
		sql_str += ") values(";

		for (i = 0; i < te_meta.field_size; i++) {
			if (i != 0)
				sql_str += ", ";
			sql_str += te_meta.fields[i].update_name;
			add_bind(te_meta.fields[i], binds);
		}
		sql_str += ")";
		sql_struct = &sql_ctx[SQL_CTX_INSERT];
		break;
	case UPDATE_TYPE_UPDATE:
		sql_str = "update ";
		sql_str += te_meta.table_name;
		sql_str += " set ";
		for (i = 0; i < te_meta.field_size; i++) {
			if (i != 0)
				sql_str += ", ";
			sql_str += te_meta.fields[i].column_name;
			sql_str += " = ";
			sql_str += te_meta.fields[i].update_name;
			add_bind(te_meta.fields[i], binds);
		}
		sql_str += " where ";

		first = true;
		for (i = 0; i < te_meta.field_size; i++) {
			if (!te_meta.fields[i].is_primary)
				continue;

			if (!first)
				sql_str += " and ";
			else
				first = false;

			sql_str += te_meta.fields[i].column_name;
			sql_str += " = ";
			sql_str += te_meta.fields[i].update_name;
			add_bind(te_meta.fields[i], binds);
		}
		sql_struct = &sql_ctx[SQL_CTX_UPDATE];
		break;
	case UPDATE_TYPE_DELETE:
		sql_str = "delete from ";
		sql_str += te_meta.table_name;
		sql_str += " where ";

		first = true;
		for (i = 0; i < te_meta.field_size; i++) {
			if (!te_meta.fields[i].is_primary)
				continue;

			if (!first)
				sql_str += " and ";
			else
				first = false;

			sql_str += te_meta.fields[i].column_name;
			sql_str += " = ";
			sql_str += te_meta.fields[i].update_name;
			add_bind(te_meta.fields[i], binds);
		}
		sql_struct = &sql_ctx[SQL_CTX_DELETE];
		break;
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: internal error")).str(SGLOCALE));
	}

	if (sql_struct->stmt != NULL) {
		db->terminate_statement(sql_struct->stmt);
		delete sql_struct->dynamic_data;
	}

	sql_struct->stmt = db->create_statement();
	sql_struct->dynamic_data = db->create_data();
	sql_struct->dynamic_data->setSQL(sql_str, binds);
	sql_struct->stmt->bind(sql_struct->dynamic_data);
	sql_struct->execute_count = 0;
}

void dbc_sync::add_bind(const dbc_data_field_t& data_field, vector<column_desc_t>& binds)
{
	column_desc_t column_desc;

	column_desc.field_type = data_field.field_type;
	if (data_field.field_type == SQLTYPE_STRING || data_field.field_type == SQLTYPE_VSTRING)
		column_desc.field_length = data_field.field_size - 1;
	else
		column_desc.field_length = data_field.field_size;

	binds.push_back(column_desc);
}

void dbc_sync::execute_insert(dbc_te_t *te, const char *row_data)
{
	const dbc_data_t& te_meta = te->te_meta;
	sql_struct_t& sql_struct = sql_ctx[SQL_CTX_INSERT];
	Generic_Statement *& stmt = sql_struct.stmt;
	struct_dynamic *& dynamic_data = sql_struct.dynamic_data;
	int& execute_count = sql_struct.execute_count;

	if (execute_count > 0)
		stmt->addIteration();

	for (int i = 0; i < te_meta.field_size; i++)
		set_data(te_meta.fields[i], i + 1, row_data, stmt);

	if (++execute_count == dynamic_data->size()) {
		stmt->executeUpdate();
		execute_count = 0;
	}
}

void dbc_sync::execute_update(dbc_te_t *te, const char *row_data, const char *old_row_data)
{
	int i;
	const dbc_data_t& te_meta = te->te_meta;
	sql_struct_t& sql_struct = sql_ctx[SQL_CTX_INSERT];
	Generic_Statement *& stmt = sql_struct.stmt;
	struct_dynamic *& dynamic_data = sql_struct.dynamic_data;
	int& execute_count = sql_struct.execute_count;

	if (execute_count > 0)
		stmt->addIteration();

	for (i = 0; i < te_meta.field_size; i++)
		set_data(te_meta.fields[i], i + 1, row_data, stmt);

	int param_index = te_meta.field_size;
	for (i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& data_field = te_meta.fields[i];
		if (!data_field.is_primary)
			continue;

		param_index++;
		set_data(te_meta.fields[i], param_index, old_row_data, stmt);
	}

	if (++execute_count == dynamic_data->size()) {
		stmt->executeUpdate();
		execute_count = 0;
	}
}

void dbc_sync::execute_delete(dbc_te_t *te, const char *row_data)
{
	const dbc_data_t& te_meta = te->te_meta;
	sql_struct_t& sql_struct = sql_ctx[SQL_CTX_INSERT];
	Generic_Statement *& stmt = sql_struct.stmt;
	struct_dynamic *& dynamic_data = sql_struct.dynamic_data;
	int& execute_count = sql_struct.execute_count;

	if (execute_count > 0)
		stmt->addIteration();

	for (int i = 0; i < te_meta.field_size; i++)
		set_data(te_meta.fields[i], i + 1, row_data, stmt);

	int param_index = 0;
	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& data_field = te_meta.fields[i];
		if (!data_field.is_primary)
			continue;

		param_index++;
		set_data(te_meta.fields[i], param_index, row_data, stmt);
	}

	if (++execute_count == dynamic_data->size()) {
		stmt->executeUpdate();
		execute_count = 0;
	}
}

void dbc_sync::executeUpdate()
{
	for (int i = 0; i < SQL_CTX_TOTAL; i++) {
		if (sql_ctx[i].execute_count > 0) {
			sql_ctx[i].stmt->executeUpdate();
			sql_ctx[i].execute_count = 0;
		}
	}

	db->commit();
}

void dbc_sync::set_data(const dbc_data_field_t& data_field, int param_index, const char *data, Generic_Statement *stmt)
{
	switch (data_field.field_type) {
	case SQLTYPE_CHAR:
		stmt->setChar(param_index, *(data + data_field.field_offset));
		break;
	case SQLTYPE_UCHAR:
		stmt->setUChar(param_index, *(data + data_field.field_offset));
		break;
	case SQLTYPE_SHORT:
		stmt->setShort(param_index, *reinterpret_cast<const short *>(data + data_field.field_offset));
		break;
	case SQLTYPE_USHORT:
		stmt->setUShort(param_index, *reinterpret_cast<const short *>(data + data_field.field_offset));
		break;
	case SQLTYPE_INT:
		stmt->setInt(param_index, *reinterpret_cast<const int *>(data + data_field.field_offset));
		break;
	case SQLTYPE_UINT:
		stmt->setUInt(param_index, *reinterpret_cast<const int *>(data + data_field.field_offset));
		break;
	case SQLTYPE_LONG:
		stmt->setLong(param_index, *reinterpret_cast<const long *>(data + data_field.field_offset));
		break;
	case SQLTYPE_ULONG:
		stmt->setLong(param_index, *reinterpret_cast<const long *>(data + data_field.field_offset));
		break;
	case SQLTYPE_FLOAT:
		stmt->setFloat(param_index, *reinterpret_cast<const float *>(data + data_field.field_offset));
		break;
	case SQLTYPE_DOUBLE:
		stmt->setDouble(param_index, *reinterpret_cast<const double *>(data + data_field.field_offset));
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		stmt->setString(param_index, data + data_field.field_offset);
 		break;
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		if (data_field.field_size == sizeof(int))
			stmt->setInt(param_index, *reinterpret_cast<const int *>(data + data_field.field_offset));
		else
			stmt->setLong(param_index, *reinterpret_cast<const long *>(data + data_field.field_offset));
		break;
	case SQLTYPE_UNKNOWN:
	case SQLTYPE_ANY:
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Unknown field_type {1}") % data_field.field_type).str(SGLOCALE));
	}
}

}
}

