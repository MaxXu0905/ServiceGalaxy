#include "dbc_dump.h"

namespace ai
{
namespace sg
{

dbc_dump::dbc_dump(int table_id_, const string& table_name_, int parallel_, long commit_size_,
	const string& user_name, const string& password)
	: api_mgr(dbc_api::instance(DBCT)),
	  table_id(table_id_),
	  table_name(table_name_),
	  parallel(parallel_),
	  commit_size(commit_size_)
{
	if (!api_mgr.login()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: dbc_dump: Can't login on BB.")).str(SGLOCALE));
		exit(1);
	}

	if (!api_mgr.connect(user_name, password)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: dbc_dump: Can't connect on BB.")).str(SGLOCALE));
		exit(1);
	}
}

dbc_dump::~dbc_dump()
{
	api_mgr.disconnect();
	api_mgr.logout();
}

long dbc_dump::run()
{
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t* ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_SELECT))
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: permissiion denied")).str(SGLOCALE));

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = NULL;

	// Set table context.
	if (table_id != -1) {
		dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
		te = ue_mgr.get_table(table_id);
		if (te == NULL)
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find table definition, table_id = {1}") % table_id).str(SGLOCALE));
	} else {
		te = te_mgr.find_te(table_name);
		if (te == NULL)
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find table definition, table_id = {1}") % table_id).str(SGLOCALE));
		table_id = te->table_id;
	}
	te_mgr.set_ctx(te);

	int i;
	const dbc_data_t& te_meta = te->te_meta;

	// Create SQL
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

	bi::sharable_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

	// Check rows.
	if (te->row_used == -1)
		return 0;

	// Create threads.
	boost::thread_group thread_group;
	vector<int> retvals;

	for (i = 0; i < parallel; i++)
		retvals.push_back(-1);

	for (i = 0; i < parallel; i++)
		thread_group.create_thread(boost::bind(&dbc_dump::execute, this, i, retvals[i]));

	thread_group.join_all();

	for (i = 0; i < parallel; i++) {
		if (retvals[i] != 0) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't load dump data for table {1}") % te_meta.table_name).str(SGLOCALE));
			return -1;
		}
	}

	return te->cur_count;
}

void dbc_dump::execute(int parallel_slot, int& retval)
{
	map<string, string> conn_info;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();
	dbc_te_t *te = ue_mgr.get_table(table_id);
	const dbc_data_t& te_meta = te->te_meta;
	te_mgr.set_ctx(te);

	database_factory& factory_mgr = database_factory::instance();
	factory_mgr.parse(ue->openinfo, conn_info);
	auto_ptr<Generic_Database> auto_db(factory_mgr.create(ue->dbname));
	Generic_Database *db = auto_db.get();
	db->connect(conn_info);

	auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();
	auto_ptr<struct_dynamic> auto_data(db->create_data());
	struct_dynamic *data = auto_data.get();

	data->setSQL(sql_str, binds);
	stmt->bind(data);

	long rowid = te->row_used;
	bool end_flag = false;

	// Skip over parallel_slot records
	for (int i = 0; i < parallel_slot; i++) {
		row_link_t *link = DBCT->rowid2link(rowid);
		if (link->next == -1) {
			end_flag = true;
			break;
		}

		rowid = link->next;
	}

	long batch_size;
	if (commit_size <= 0)
		batch_size = numeric_limits<long>::max();
	else
		batch_size = (commit_size -1)  / data->size() + 1;
	long batch_count = 0;
	int execute_count = 0;
	while (!end_flag) {
		char *row_data = DBCT->rowid2data(rowid);

		if (execute_count > 0)
			stmt->addIteration();

		for (int i = 0; i < te_meta.field_size; i++)
			set_data(te_meta.fields[i], i + 1, row_data, stmt);

		if (++execute_count == data->size()) {
			stmt->executeUpdate();
			execute_count = 0;
			if (++batch_count == batch_size) {
				batch_count = 0;
				db->commit();
			}
		}

		// Skip over parallelrecords
		for (int i = 0; i < parallel; i++) {
			row_link_t *link = DBCT->rowid2link(rowid);
			if (link->next == -1) {
				end_flag = true;
				break;
			}

			rowid = link->next;
		}
	}

	if (execute_count > 0) {
		stmt->executeUpdate();
		db->commit();
	}

	retval = 0;
}

void dbc_dump::add_bind(const dbc_data_field_t& data_field, vector<column_desc_t>& binds)
{
	column_desc_t column_desc;

	column_desc.field_type = data_field.field_type;
	if (data_field.field_type == SQLTYPE_STRING || data_field.field_type == SQLTYPE_VSTRING)
		column_desc.field_length = data_field.field_size - 1;
	else
		column_desc.field_length = data_field.field_size;

	binds.push_back(column_desc);
}

void dbc_dump::set_data(const dbc_data_field_t& data_field, int param_index, const char *data, Generic_Statement *stmt)
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

