#include "sql_statement.h"
#include "sql_control.h"
#include "struct_dynamic.h"

namespace ai
{
namespace scci
{

bool sql_statement::dynamic_flag = false;
string sql_statement::h_path;
string sql_statement::cpp_path;
string sql_statement::file_prefix;
string sql_statement::filename_h;
string sql_statement::filename_cpp;
string sql_statement::class_name;
int sql_statement::array_size;
bool sql_statement::require_ind;
dbtype_enum sql_statement::dbtype;
string sql_statement::ind_type_str;
string sql_statement::date_type_str;
string sql_statement::time_type_str;
string sql_statement::datetime_type_str;
int sql_statement::ind_type_size;
int sql_statement::date_type_size;
int sql_statement::time_type_size;
int sql_statement::datetime_type_size;
ostringstream sql_statement::data_field_fmt;
map<string, combine_field_t> sql_statement::data_map;
vector<sql_statement *> sql_statement::stmt_vector;
int sql_statement::bind_pos = 0;

sql_statement::sql_statement()
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	require_ind = true;
	dbtype = DBTYPE_ORACLE;
	array_size = 100;
	root_flag = false;
}

sql_statement::~sql_statement()
{
}

void sql_statement::add_statement(sql_statement *stmt)
{
	stmt_vector.push_back(stmt);
}

void sql_statement:: set_h_path(const string& h_path_)
{
	h_path = h_path_;
}

void sql_statement:: set_cpp_path(const string& cpp_path_)
{
	cpp_path = cpp_path_;
}

void sql_statement:: set_file_prefix(const string& file_prefix_)
{
	file_prefix = file_prefix_;
	filename_h = file_prefix + ".h";
	filename_cpp = file_prefix + ".cpp";
}

void sql_statement:: set_class_name(const string& class_name_)
{
	class_name = class_name_;
}

void sql_statement::set_array_size(int array_size_)
{
	array_size = array_size_;
}

void sql_statement:: set_reqire_ind(bool require_ind_)
{
	require_ind = require_ind_;
}

void sql_statement:: set_dbtype(dbtype_enum dbtype_)
{
	dbtype = dbtype_;
	switch (dbtype) {
	case DBTYPE_ORACLE:
		ind_type_str = "sb2";
		date_type_str = "OCIDate";
		time_type_str = "OCIDate";
		datetime_type_str = "OCIDate";
		ind_type_size = 2;
		date_type_size = 8;
		time_type_size = 8;
		datetime_type_size = 8;
		break;
	case DBTYPE_ALTIBASE:
		ind_type_str = "SQLINTEGER";
		date_type_str = "tagDATE_STRUCT";
		time_type_str = "tagTIME_STRUCT";
		datetime_type_str = "tagTIMESTAMP_STRUCT";
		ind_type_size = 2;
		date_type_size = 8;
		time_type_size = 8;
		datetime_type_size = 8;
		break;
	case DBTYPE_DBC:
		ind_type_str = "short";
		date_type_str = "time_t";
		time_type_str = "time_t";
		datetime_type_str = "time_t";
		ind_type_size = 2;
		date_type_size = sizeof(time_t);
		time_type_size = sizeof(time_t);
		datetime_type_size = sizeof(time_t);
		break;
	case DBTYPE_GP:
		ind_type_str = "short";
		date_type_str = "time_t";
		time_type_str = "time_t";
		datetime_type_str = "time_t";
		ind_type_size = sizeof(short);
		date_type_size = sizeof(time_t);
		time_type_size = sizeof(time_t);
		datetime_type_size = sizeof(time_t);
		break;
	default:
		break;
	}
}

dbtype_enum sql_statement:: get_dbtype()
{
	return dbtype;
}

int sql_statement::get_bind_pos()
{
	return ++bind_pos;
}

void sql_statement::set_stmt_type(stmttype_enum stmt_type_)
{
	stmt_type = stmt_type_;
}

stmttype_enum sql_statement::get_stmt_type() const
{
	return stmt_type;
}

bool sql_statement::is_root() const
{
	return root_flag;
}

void sql_statement::set_root()
{
	root_flag = true;
}

void sql_statement::set_comments(char *comments_)
{
	if (comments_ == NULL)
		comments = "";
	else
		comments = comments_;
}

void sql_statement::print_data()
{
	for (vector<sql_statement *>::iterator iter = stmt_vector.begin(); iter != stmt_vector.end(); ++iter)
		(*iter)->print();
}

void sql_statement::clear()
{
	for (vector<sql_statement *>::iterator iter = stmt_vector.begin(); iter != stmt_vector.end(); ++iter)
		delete *iter;
	data_map.clear();
	stmt_vector.clear();
	bind_pos = 0;
}

sql_statement * sql_statement::first()
{
	return stmt_vector[0];
}

bool sql_statement::gen_data_static()
{
	dynamic_flag = false;
	for (vector<sql_statement *>::const_iterator iter = stmt_vector.begin(); iter != stmt_vector.end(); ++iter) {
		sql_statement *_this = *iter;
		_this->select_item_action = &sql_statement::gen_select_static;
		_this->bind_item_action = &sql_statement::gen_bind_static;
		_this->finish_action = &sql_statement::gen_static;
		if (!_this->gen_data())
			return false;
	}

	return true;
}

bool sql_statement::gen_data_dynamic(struct_dynamic *data_)
{
	dynamic_flag = true;
	sql_statement *_this = stmt_vector[0];
	_this->data = data_;
	_this->data->clear();
	_this->select_item_action = &sql_statement::gen_select_dynamic;
	_this->bind_item_action = &sql_statement::gen_bind_dynamic;
	_this->finish_action = &sql_statement::gen_dynamic;
	return _this->gen_data();
}

bool sql_statement::gen_select_static(const composite_item_t& composite_item)
{
	simple_item_t *simple_item = composite_item.simple_item;
	field_desc_t *field_desc = composite_item.field_desc;

	if (field_desc->alias.empty() && simple_item == NULL) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: alias name not given")).str(SGLOCALE));
		return false;
	}

	string field_name = (field_desc->alias.empty() ? simple_item->value : field_desc->alias);
	string field_len;
	ostringstream field_fmt;

	field_fmt << "\t";
	data_select_fmt << "\t{ ";
	switch (field_desc->type) {
	case SQLTYPE_CHAR:
		field_fmt << "char ";
		data_select_fmt << "SQLTYPE_CHAR";
		field_len = "sizeof(char)";
		break;
	case SQLTYPE_UCHAR:
		field_fmt << "unsigned char ";
		data_select_fmt << "SQLTYPE_UCHAR";
		field_len = "sizeof(unsigned char)";
		break;
	case SQLTYPE_SHORT:
		field_fmt << "short ";
		data_select_fmt << "SQLTYPE_SHORT";
		field_len = "sizeof(short)";
		break;
	case SQLTYPE_USHORT:
		field_fmt << "unsigned short ";
		data_select_fmt << "SQLTYPE_USHORT";
		field_len = "sizeof(unsigned short)";
		break;
	case SQLTYPE_INT:
		field_fmt << "int ";
		data_select_fmt << "SQLTYPE_INT";
		field_len = "sizeof(int)";
		break;
	case SQLTYPE_UINT:
		field_fmt << "unsigned int ";
		data_select_fmt << "SQLTYPE_UINT";
		field_len = "sizeof(unsigned int)";
		break;
	case SQLTYPE_LONG:
		field_fmt << "long ";
		data_select_fmt << "SQLTYPE_LONG";
		field_len = "sizeof(long)";
		break;
	case SQLTYPE_ULONG:
		field_fmt << "unsigned long ";
		data_select_fmt << "SQLTYPE_ULONG";
		field_len = "sizeof(unsigned long)";
		break;
	case SQLTYPE_FLOAT:
		field_fmt << "float ";
		data_select_fmt << "SQLTYPE_FLOAT";
		field_len = "sizeof(float)";
		break;
	case SQLTYPE_DOUBLE:
		field_fmt << "double ";
		data_select_fmt << "SQLTYPE_DOUBLE";
		field_len = "sizeof(double)";
		break;
	case SQLTYPE_STRING:
		if (field_desc->len <= 0) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: String length should be positive")).str(SGLOCALE));
			return false;
		}
		field_fmt << "char ";
		data_select_fmt << "SQLTYPE_STRING";
		fmt.str("");
		fmt << (field_desc->len + 1);
		field_len = fmt.str();
 		break;
	case SQLTYPE_VSTRING:
		if (field_desc->len <= 0) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: VString length should be positive")).str(SGLOCALE));
			return false;
		}
		field_fmt << "vchar ";
		data_select_fmt << "SQLTYPE_VSTRING";
		fmt.str("");
		fmt << (field_desc->len + 1);
		field_len = fmt.str();
 		break;
	case SQLTYPE_DATE:
		field_fmt << date_type_str << " ";
		data_select_fmt << "SQLTYPE_DATE";
		field_len = "sizeof(";
		field_len += date_type_str;
		field_len += ")";
 		break;
	case SQLTYPE_TIME:
		field_fmt << time_type_str << " ";
		data_select_fmt << "SQLTYPE_TIME";
		field_len = "sizeof(";
		field_len += time_type_str;
		field_len += ")";
 		break;
	case SQLTYPE_DATETIME:
		field_fmt << datetime_type_str << " ";
		data_select_fmt << "SQLTYPE_DATETIME";
		field_len = "sizeof(";
		field_len += datetime_type_str;
		field_len += ")";
 		break;
	default:
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown sql type")).str(SGLOCALE));
		return false;
	}

	field_fmt << field_name << "[" << array_size << "]";
	if (field_desc->type == SQLTYPE_STRING || field_desc->type == SQLTYPE_VSTRING)
		field_fmt << "[" << field_desc->len << " + 1]";
	field_fmt << ";\n";

	data_select_fmt << ", \"" << field_name << "\", " << field_len
		<< ", offsetof(" << class_name << ", " << field_name << "), sizeof("
		<< ind_type_str << "), ";

	if (require_ind) {
		field_fmt << "\t" << ind_type_str << " " << field_name << "_ind[" << array_size << "];\n";
		data_select_fmt << "offsetof(" << class_name << ", " << field_name << "_ind) },\n";
	} else {
		data_select_fmt << "-1 },\n";
	}

	map<string, combine_field_t>::const_iterator map_iter = data_map.find(field_name);
	if (map_iter == data_map.end()) {
		combine_field_t combine_field;
		combine_field.field_type = field_desc->type;
		combine_field.field_length = field_desc->len;
		data_map[field_name] = combine_field;
		data_field_fmt << field_fmt.str();
	} else {
		if (map_iter->second.field_type != field_desc->type || map_iter->second.field_length != field_desc->len) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: column name [{1}] redefined with different type or length") % field_name).str(SGLOCALE));
			return false;
		}
	}

	return true;
}

bool sql_statement::gen_bind_static(const composite_item_t& composite_item)
{
	simple_item_t *simple_item = composite_item.simple_item;
	field_desc_t *field_desc = composite_item.field_desc;

	if (field_desc->alias.empty() && simple_item == NULL) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: alias name not given")).str(SGLOCALE));
		return false;
	}

	string field_name = (field_desc->alias.empty() ? simple_item->value : field_desc->alias);
	string field_len;
	ostringstream field_fmt;

	field_fmt << "\t";
	data_bind_fmt << "\t{ ";
	switch (field_desc->type) {
	case SQLTYPE_CHAR:
		field_fmt << "char ";
		data_bind_fmt << "SQLTYPE_CHAR";
		field_len = "sizeof(char)";
		break;
	case SQLTYPE_UCHAR:
		field_fmt << "unsigned char ";
		data_bind_fmt << "SQLTYPE_UCHAR";
		field_len = "sizeof(unsigned char)";
		break;
	case SQLTYPE_SHORT:
		field_fmt << "short ";
		data_bind_fmt << "SQLTYPE_SHORT";
		field_len = "sizeof(short)";
		break;
	case SQLTYPE_USHORT:
		field_fmt << "unsigned short ";
		data_bind_fmt << "SQLTYPE_USHORT";
		field_len = "sizeof(unsigned short)";
		break;
	case SQLTYPE_INT:
		field_fmt << "int ";
		data_bind_fmt << "SQLTYPE_INT";
		field_len = "sizeof(int)";
		break;
	case SQLTYPE_UINT:
		field_fmt << "unsigned int ";
		data_bind_fmt << "SQLTYPE_UINT";
		field_len = "sizeof(unsigned int)";
		break;
	case SQLTYPE_LONG:
		field_fmt << "long ";
		data_bind_fmt << "SQLTYPE_LONG";
		field_len = "sizeof(long)";
		break;
	case SQLTYPE_ULONG:
		field_fmt << "unsigned long ";
		data_bind_fmt << "SQLTYPE_ULONG";
		field_len = "sizeof(unsigned long)";
		break;
	case SQLTYPE_FLOAT:
		field_fmt << "float ";
		data_bind_fmt << "SQLTYPE_FLOAT";
		field_len = "sizeof(float)";
		break;
	case SQLTYPE_DOUBLE:
		field_fmt << "double ";
		data_bind_fmt << "SQLTYPE_DOUBLE";
		field_len = "sizeof(double)";
		break;
	case SQLTYPE_STRING:
		if (field_desc->len <= 0) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: String length should be positive")).str(SGLOCALE));
			return false;
		}
		field_fmt << "char ";
		data_bind_fmt << "SQLTYPE_STRING";
		fmt.str("");
		fmt << (field_desc->len + 1);
		field_len = fmt.str();
		break;
	case SQLTYPE_VSTRING:
		if (field_desc->len <= 0) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: VString length should be positive")).str(SGLOCALE));
			return false;
		}
		field_fmt << "vchar ";
		data_bind_fmt << "SQLTYPE_VSTRING";
		fmt.str("");
		fmt << (field_desc->len + 1);
		field_len = fmt.str();
		break;
	case SQLTYPE_DATE:
		field_fmt << date_type_str << " ";
		data_bind_fmt << "SQLTYPE_DATE";
		field_len = "sizeof(";
		field_len += date_type_str;
		field_len += ")";
 		break;
	case SQLTYPE_TIME:
		field_fmt << time_type_str << " ";
		data_bind_fmt << "SQLTYPE_TIME";
		field_len = "sizeof(";
		field_len += time_type_str;
		field_len += ")";
 		break;
	case SQLTYPE_DATETIME:
		field_fmt << datetime_type_str << " ";
		data_bind_fmt << "SQLTYPE_DATETIME";
		field_len = "sizeof(";
		field_len += datetime_type_str;
		field_len += ")";
 		break;
	default:
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown sql type")).str(SGLOCALE));
		return false;
	}

	field_fmt << field_name << "[" << array_size << "]";
	if (field_desc->type == SQLTYPE_STRING || field_desc->type == SQLTYPE_VSTRING)
		field_fmt << "[" << field_desc->len << " + 1]";
	field_fmt << ";\n";

	data_bind_fmt << ", \":" << field_name << "\", " << field_len
		<< ", offsetof(" << class_name << ", " << field_name << "), sizeof("
		<< ind_type_str << "), ";

	if (require_ind) {
		field_fmt << "\t" << ind_type_str << " " << field_name << "_ind[" << array_size << "];\n";
		data_bind_fmt << "offsetof(" << class_name << ", " << field_name << "_ind) },\n";
	} else {
		data_bind_fmt << "-1 },\n";
	}

	map<string, combine_field_t>::const_iterator map_iter = data_map.find(field_name);
	if (map_iter == data_map.end()) {
		combine_field_t combine_field;
		combine_field.field_type = field_desc->type;
		combine_field.field_length = field_desc->len;
		data_map[field_name] = combine_field;
		data_field_fmt << field_fmt.str();
	} else {
		if (map_iter->second.field_type != field_desc->type
			|| ((field_desc->type == SQLTYPE_STRING || field_desc->type == SQLTYPE_VSTRING)
				&& map_iter->second.field_length != field_desc->len)) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: column name [{1}] redefined with different type or length")).str(SGLOCALE));
			return false;
		}
	}

	return true;
}

void sql_statement::gen_static()
{
	int i;
	string full_name;

	if (this != *stmt_vector.rbegin())
		return;

	full_name = h_path;
	if (*full_name.rbegin() != '/')
		full_name += "/";
	full_name += filename_h;
	ofstream ofs(full_name.c_str());
	if (!ofs) {
		GPT->_GPT_error_code = GPEOS;
		GPT->_GPT_native_code = UOPEN;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1}") % filename_h).str(SGLOCALE));
	}

	string upper_value = file_prefix;
	common::upper(upper_value);
	ofs << "#if !defined(__" << upper_value << "_H__)\n";
	ofs << "#define __" << upper_value << "_H__\n\n";
	switch (dbtype) {
	case DBTYPE_ORACLE:
		ofs << "#include \"oci.h\"\n";
		break;
	case DBTYPE_ALTIBASE:
		break;
	case DBTYPE_DBC:
		break;
	case DBTYPE_GP:
		ofs << "#include \"libpq-fe.h\"\n";
			break;
	default:
		break;
	}
	ofs << "#include \"struct_base.h\"\n\n";

	ofs << "namespace ai\n";
	ofs << "{\n";
	ofs << "namespace scci\n";
	ofs << "{\n\n";

	ofs << "class " << class_name << " : public struct_base\n";
	ofs << "{\n";
	ofs << "public:\n";
	ofs << "\tdbtype_enum get_dbtype();\n";
	ofs << "\tbind_field_t * getSelectFields(int index = 0);\n";
	ofs << "\tbind_field_t * getBindFields(int index = 0);\n";
	ofs << "\ttable_field_t * getTableFields(int index = 0);\n";
	ofs << "\tstring& getSQL(int index = 0);\n";
	ofs << "\tint size() const;\n";
	ofs << "\n";

	ofs << data_field_fmt.str();

	ofs << "\nprivate:\n";
	for (i = 0; i < stmt_vector.size(); i++) {
		ofs << "\tstatic bind_field_t select_fields" << i << "[];\n";
		ofs << "\tstatic bind_field_t bind_fields" << i << "[];\n";
		ofs << "\tstatic table_field_t table_fields" << i << "[];\n";
	}
	ofs << "\tstatic bind_field_t *select_fields[];\n";
	ofs << "\tstatic bind_field_t *bind_fields[];\n";
	ofs << "\tstatic string _sql[" << stmt_vector.size() << "];\n";
	ofs << "\tstatic table_field_t *table_fields[];\n";
	ofs << "};\n\n";

	ofs << "}\n";
	ofs << "}\n\n";
	ofs << "#endif\n\n";

	ofs.close();

	full_name = cpp_path;
	if (*full_name.rbegin() != '/')
		full_name += "/";
	full_name += filename_cpp;
	ofs.open(full_name.c_str());
	if (!ofs) {
		GPT->_GPT_error_code = GPEOS;
		GPT->_GPT_native_code = UOPEN;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1}") % filename_cpp).str(SGLOCALE));
	}

	ofs << "#include \"sql_base.h\"\n";
	switch (dbtype) {
	case DBTYPE_ORACLE:
		ofs << "#include \"oracle_control.h\"\n";
		break;
	case DBTYPE_ALTIBASE:
		ofs << "#include \"altibase_control.h\"\n";
		break;
	case DBTYPE_DBC:
		ofs << "#include \"dbc_control.h\"\n";
		break;
	case DBTYPE_GP:
		ofs << "#include \"gp_control.h\"\n";
		break;
	default:
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: unsupported dbtype")).str(SGLOCALE));
	}
	ofs << "#include \"" << filename_h << "\"\n\n";

	ofs << "namespace ai\n";
	ofs << "{\n";
	ofs << "namespace scci\n";
	ofs << "{\n\n";

	for (i = 0; i < stmt_vector.size(); i++) {
		ofs << "bind_field_t " << class_name << "::select_fields" << i << "[] = {\n";
		ofs << stmt_vector[i]->data_select_fmt.str();
		ofs << "\t{ SQLTYPE_UNKNOWN, \"\", 0, -1, 0, -1 }\n";
		ofs << "};\n\n";

		ofs << "bind_field_t " << class_name << "::bind_fields" << i << "[] = {\n";
		ofs << stmt_vector[i]->data_bind_fmt.str();
		ofs << "\t{ SQLTYPE_UNKNOWN, \"\", 0, -1, 0, -1 }\n";
		ofs << "};\n\n";
	}

	ofs << "string " << class_name << "::_sql[] = {\n";
	for (i = 0; i < stmt_vector.size(); i++) {
		if (i > 0)
			ofs << ",\n";
		ofs << "\t\"" << stmt_vector[i]->gen_sql() << "\"";
	}
	ofs << "\n};\n\n";

	for (i = 0; i < stmt_vector.size(); i++) {
		ofs << "table_field_t " << class_name << "::table_fields" << i << "[] = {\n";
		for (map<string, int>::const_iterator map_iter =  stmt_vector[i]->table_pos_map.begin(); map_iter !=  stmt_vector[i]->table_pos_map.end(); ++map_iter)
			ofs << "\t{ \"" << map_iter->first << "\", " << map_iter->second << " },\n";
		ofs << "\t{ \"\", -1 }\n";
		ofs << "};\n\n";
	}

	ofs << "bind_field_t *" << class_name << "::select_fields[] = {\n";
	for (i = 0; i < stmt_vector.size(); i++) {
		if (i > 0)
			ofs << ",\n";
		ofs << "\t" << class_name << "::select_fields" << i;
	}
	ofs << "\n};\n\n";

	ofs << "bind_field_t *" << class_name << "::bind_fields[] = {\n";
	for (i = 0; i < stmt_vector.size(); i++) {
		if (i > 0)
			ofs << ",\n";
		ofs << "\t" << class_name << "::bind_fields" << i;
	}
	ofs << "\n};\n\n";

	ofs << "table_field_t *" << class_name << "::table_fields[] = {\n";
	for (i = 0; i < stmt_vector.size(); i++) {
		if (i > 0)
			ofs << ",\n";
		ofs << "\t" << class_name << "::table_fields" << i;
	}
	ofs << "\n};\n\n";

	ofs << "dbtype_enum " << class_name << "::get_dbtype()\n";
	ofs << "{\n";
	switch (dbtype) {
	case DBTYPE_ORACLE:
		ofs << "\treturn DBTYPE_ORACLE;\n";
		break;
	case DBTYPE_ALTIBASE:
		ofs << "\treturn DBTYPE_ALTIBASE;\n";
		break;
	case DBTYPE_DBC:
		ofs << "\treturn DBTYPE_DBC;\n";
		break;
	case DBTYPE_GP:
			ofs << "\treturn DBTYPE_GP;\n";
			break;
	default:
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: unsupported dbtype")).str(SGLOCALE));
	}
	ofs << "}\n\n";

	ofs << "bind_field_t * " << class_name << "::getSelectFields(int index)\n";
	ofs << "{\n";
	ofs << "\treturn select_fields[index];\n";
	ofs << "}\n\n";

	ofs << "bind_field_t * " << class_name << "::getBindFields(int index)\n";
	ofs << "{\n";
	ofs << "\treturn bind_fields[index];\n";
	ofs << "}\n\n";

	ofs << "string& " << class_name << "::getSQL(int index)\n";
	ofs << "{\n";
	ofs << "\treturn _sql[index];\n";
	ofs << "}\n\n";

	ofs << "table_field_t * " << class_name << "::getTableFields(int index)\n";
	ofs << "{\n";
	ofs << "\treturn table_fields[index];\n";
	ofs << "}\n\n";

	ofs << "int " << class_name << "::size() const\n";
	ofs << "{\n";
	ofs << "\treturn " << array_size << ";\n";
	ofs << "}\n\n";

	ofs << "}\n";
	ofs << "}\n\n";
}

bool sql_statement::gen_select_dynamic(const composite_item_t& composite_item)
{
	bind_field_t field;
	simple_item_t *simple_item = composite_item.simple_item;
	field_desc_t *field_desc = composite_item.field_desc;

	if (field_desc->alias.empty()) {
		if (simple_item == NULL) {
			field.field_name = "";
			composite_item.gen_sql(field.field_name, true);
		} else {
			field.field_name = simple_item->value;
		}
	} else {
		field.field_name = field_desc->alias;
	}

	field.field_offset = -1;
	field.ind_length = ind_type_size;
	field.ind_offset = -1;

	switch (field_desc->type) {
	case SQLTYPE_CHAR:
		field.field_type = SQLTYPE_CHAR;
		field.field_length = sizeof(char);
		break;
	case SQLTYPE_UCHAR:
		field.field_type = SQLTYPE_UCHAR;
		field.field_length = sizeof(unsigned char);
		break;
	case SQLTYPE_SHORT:
		field.field_type = SQLTYPE_SHORT;
		field.field_length = sizeof(short);
		break;
	case SQLTYPE_USHORT:
		field.field_type = SQLTYPE_USHORT;
		field.field_length = sizeof(unsigned short);
		break;
	case SQLTYPE_INT:
		field.field_type = SQLTYPE_INT;
		field.field_length = sizeof(int);
		break;
	case SQLTYPE_UINT:
		field.field_type = SQLTYPE_UINT;
		field.field_length = sizeof(unsigned int);
		break;
	case SQLTYPE_LONG:
		field.field_type = SQLTYPE_LONG;
		field.field_length = sizeof(long);
		break;
	case SQLTYPE_ULONG:
		field.field_type = SQLTYPE_ULONG;
		field.field_length = sizeof(unsigned long);
		break;
	case SQLTYPE_FLOAT:
		field.field_type = SQLTYPE_FLOAT;
		field.field_length = sizeof(float);
		break;
	case SQLTYPE_DOUBLE:
		field.field_type = SQLTYPE_DOUBLE;
		field.field_length = sizeof(double);
		break;
	case SQLTYPE_STRING:
		if (field_desc->len <= 0) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: String length should be positive")).str(SGLOCALE));
			return false;
		}
		field.field_type = SQLTYPE_STRING;
		field.field_length = field_desc->len + 1;
		break;
	case SQLTYPE_VSTRING:
		if (field_desc->len <= 0) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: VString length should be positive")).str(SGLOCALE));
			return false;
		}
		field.field_type = SQLTYPE_VSTRING;
		field.field_length = field_desc->len + 1;
		break;
	case SQLTYPE_DATE:
		field.field_type = SQLTYPE_DATE;
		field.field_length = date_type_size;
 		break;
	case SQLTYPE_TIME:
		field.field_type = SQLTYPE_TIME;
		field.field_length = time_type_size;
 		break;
	case SQLTYPE_DATETIME:
		field.field_type = SQLTYPE_DATETIME;
		field.field_length = datetime_type_size;
 		break;
	case SQLTYPE_ANY:
		field.field_type = SQLTYPE_ANY;
		field.field_length = -1;
		break;
	default:
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown sql type")).str(SGLOCALE));
		return false;
	}

	data->select_fields.push_back(field);
	return true;
}

bool sql_statement::gen_bind_dynamic(const composite_item_t& composite_item)
{
	bind_field_t field;
	simple_item_t *simple_item = composite_item.simple_item;
	field_desc_t *field_desc = composite_item.field_desc;

	if (field_desc->alias.empty()) {
		if (simple_item == NULL)
			field.field_name = "";
		else
			field.field_name = simple_item->value;
	} else {
		field.field_name = field_desc->alias;
	}

	field.field_offset = -1;
	field.ind_length = ind_type_size;
	field.ind_offset = -1;

	switch (field_desc->type) {
	case SQLTYPE_CHAR:
		field.field_type = SQLTYPE_CHAR;
		field.field_length = sizeof(char);
		break;
	case SQLTYPE_UCHAR:
		field.field_type = SQLTYPE_UCHAR;
		field.field_length = sizeof(unsigned char);
		break;
	case SQLTYPE_SHORT:
		field.field_type = SQLTYPE_SHORT;
		field.field_length = sizeof(short);
		break;
	case SQLTYPE_USHORT:
		field.field_type = SQLTYPE_USHORT;
		field.field_length = sizeof(unsigned short);
		break;
	case SQLTYPE_INT:
		field.field_type = SQLTYPE_INT;
		field.field_length = sizeof(int);
		break;
	case SQLTYPE_UINT:
		field.field_type = SQLTYPE_UINT;
		field.field_length = sizeof(unsigned int);
		break;
	case SQLTYPE_LONG:
		field.field_type = SQLTYPE_LONG;
		field.field_length = sizeof(long);
		break;
	case SQLTYPE_ULONG:
		field.field_type = SQLTYPE_ULONG;
		field.field_length = sizeof(unsigned long);
		break;
	case SQLTYPE_FLOAT:
		field.field_type = SQLTYPE_FLOAT;
		field.field_length = sizeof(float);
		break;
	case SQLTYPE_DOUBLE:
		field.field_type = SQLTYPE_DOUBLE;
		field.field_length = sizeof(double);
		break;
	case SQLTYPE_STRING:
		if (field_desc->len <= 0) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: String length should be positive")).str(SGLOCALE));
			return false;
		}
		field.field_type = SQLTYPE_STRING;
		field.field_length = field_desc->len + 1;
		break;
	case SQLTYPE_VSTRING:
		if (field_desc->len <= 0) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: VString length should be positive")).str(SGLOCALE));
			return false;
		}
		field.field_type = SQLTYPE_VSTRING;
		field.field_length = field_desc->len + 1;
		break;
	case SQLTYPE_DATE:
		field.field_type = SQLTYPE_DATE;
		field.field_length = date_type_size;
 		break;
	case SQLTYPE_TIME:
		field.field_type = SQLTYPE_TIME;
		field.field_length = time_type_size;
 		break;
	case SQLTYPE_DATETIME:
		field.field_type = SQLTYPE_DATETIME;
		field.field_length = datetime_type_size;
 		break;
	case SQLTYPE_ANY:
		field.field_type = SQLTYPE_ANY;
		field.field_length = -1;
		break;
	default:
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown sql type")).str(SGLOCALE));
		return false;
	}

	data->bind_fields.push_back(field);
	return true;
}

void sql_statement::gen_dynamic()
{
	bind_field_t field;
	table_field_t table_field;

	data->_sql = gen_sql();

	field.field_type = SQLTYPE_UNKNOWN;
	field.field_name = "";
	field.field_length = -1;
	field.field_offset = -1;
	field.ind_length = ind_type_size;
	field.ind_offset = -1;

	data->select_fields.push_back(field);
	data->bind_fields.push_back(field);

	for (map<string, int>::const_iterator iter = table_pos_map.begin(); iter != table_pos_map.end(); ++iter) {
		table_field.table_name = iter->first;
		table_field.pos = iter->second;
		data->table_fields.push_back(table_field);
	}

	table_field.table_name = "";
	table_field.pos = -1;
	data->table_fields.push_back(table_field);
}

}
}

