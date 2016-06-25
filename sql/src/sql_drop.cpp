#include "sql_drop.h"
#include "sql_tree.h"

namespace ai
{
namespace scci
{

extern string print_prefix;

sql_drop::sql_drop()
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	table_items = NULL;
}

sql_drop::~sql_drop()
{
	delete table_items;
}

void sql_drop::set_table_items(composite_item_t *table_items_)
{
	table_items = table_items_;
}

string sql_drop::gen_sql()
{
	string sql = "DROP TABLE ";

	if (table_items)
		table_items->gen_sql(sql, false);

	return sql;
}

string sql_drop::gen_code(const map<string, sql_field_t>& field_map, const bind_field_t *bind_fields)
{
	gen_code_t para(&field_map, bind_fields);
	string& code = para.code;

	return code;
}

string sql_drop::gen_table()
{
	string sql;
	if (table_items)
		table_items->simple_item->gen_sql(sql, true);
	return sql;
}

bool sql_drop::gen_data()
{
	(this->*finish_action)();
	return true;
}

void sql_drop::print() const
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

