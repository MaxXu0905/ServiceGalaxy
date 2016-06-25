#include "sql_truncate.h"
#include "sql_tree.h"

namespace ai
{
namespace scci
{

extern string print_prefix;

sql_truncate::sql_truncate()
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	table_items = NULL;
}

sql_truncate::~sql_truncate()
{
	delete table_items;
}

void sql_truncate::set_table_items(composite_item_t *table_items_)
{
	table_items = table_items_;
}

string sql_truncate::gen_sql()
{
	string sql = "TRUNCATE TABLE ";

	if (table_items)
		table_items->gen_sql(sql, false);

	return sql;
}

string sql_truncate::gen_code(const map<string, sql_field_t>& field_map, const bind_field_t *bind_fields)
{
	gen_code_t para(&field_map, bind_fields);
	string& code = para.code;

	return code;
}

string sql_truncate::gen_table()
{
	string sql;
	if (table_items)
		table_items->simple_item->gen_sql(sql, true);
	return sql;
}

bool sql_truncate::gen_data()
{
	(this->*finish_action)();
	return true;
}

void sql_truncate::print() const
{
	cout << "{table_items\n";
	if (table_items) {
		print_prefix += "  ";
		table_items->print();
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}table_items\n";
}

}
}

