#include "sql_create.h"
#include "sql_tree.h"

namespace ai
{
namespace scci
{

extern string print_prefix;

sql_create::sql_create()
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	table_items = NULL;
	field_items = NULL;
}

sql_create::~sql_create()
{
	delete field_items;
	delete table_items;
}

void sql_create::set_table_items(composite_item_t *table_items_)
{
	table_items = table_items_;
}

void sql_create::set_field_items(arg_list_t *field_items_)
{
	field_items = field_items_;
}

string sql_create::gen_sql()
{
	string sql = "CREATE ";
	if (!comments.empty()) {
		sql += comments;
		sql += " ";
	}
	sql += "TABLE ";
	if (table_items)
		table_items->gen_sql(sql, false);

	if (field_items) {
		sql += " ( ";
		for (arg_list_t::const_iterator iter = field_items->begin(); iter != field_items->end(); ++iter) {
			simple_item_t *simple_item = (*iter)->simple_item;
			field_desc_t *field_desc = (*iter)->field_desc;

			if (iter != field_items->begin())
				sql += ",\n";

			sql += "\t";
			simple_item->gen_sql(sql, true);
			switch (field_desc->type) {
			case SQLTYPE_CHAR:
				sql += "char";
				break;
			case SQLTYPE_UCHAR:
				sql += "uchar";
				break;
			case SQLTYPE_SHORT:
				sql += "short";
				break;
			case SQLTYPE_USHORT:
				sql += "ushort";
				break;
			case SQLTYPE_INT:
				sql += "int";
				break;
			case SQLTYPE_UINT:
				sql += "uint";
				break;
			case SQLTYPE_LONG:
				sql += "long";
				break;
			case SQLTYPE_ULONG:
				sql += "ulong";
				break;
			case SQLTYPE_FLOAT:
				sql += "float";
				break;
			case SQLTYPE_DOUBLE:
				sql += "double";
				break;
			case SQLTYPE_STRING:
				{
					fmt.str("");
					fmt << field_desc->len;
					sql += "char(";
					sql += fmt.str();
					sql += ")";
					break;
				}
			case SQLTYPE_VSTRING:
				{
					fmt.str("");
					fmt << field_desc->len;
					sql += "vchar(";
					sql += fmt.str();
					sql += ")";
					break;
				}
			case SQLTYPE_DATE:
				sql += "date";
				break;
			case SQLTYPE_TIME:
				sql += "time";
				break;
			case SQLTYPE_DATETIME:
				sql += "datetime";
				break;
			case SQLTYPE_UNKNOWN:
			default:
				break;
			}
		}
		sql += ")";
	}

	return sql;
}

string sql_create::gen_code(const map<string, sql_field_t>& field_map, const bind_field_t *bind_fields)
{
	gen_code_t para(&field_map, bind_fields);
	string& code = para.code;

	if (field_items) {
		for (arg_list_t::iterator iter = field_items->begin(); iter != field_items->end(); ++iter) {
			if (iter != field_items->begin())
				code += ",\n";
			(*iter)->gen_code(para);
		}
	}

	return code;
}

string sql_create::gen_table()
{
	string sql;
	if (table_items)
		table_items->simple_item->gen_sql(sql, true);
	return sql;
}

bool sql_create::gen_data()
{
	(this->*finish_action)();
	return true;
}

void sql_create::print() const
{
	cout << "{table_items\n";
	if (table_items) {
		print_prefix += "  ";
		table_items->print();
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}table_items\n";

	cout << "{field_items\n";
	if (field_items) {
		print_prefix += "  ";
		for (arg_list_t::const_iterator iter = field_items->begin(); iter != field_items->end(); ++iter)
			(*iter)->print();
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}field_items\n";
}

arg_list_t * sql_create::get_field_items()
{
	return field_items;
}

}
}

