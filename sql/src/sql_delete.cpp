#include "sql_delete.h"
#include "sql_tree.h"

namespace ai
{
namespace scci
{

extern string print_prefix;

sql_delete::sql_delete()
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	table_items = NULL;
	where_items = NULL;
}

sql_delete::~sql_delete()
{
	delete where_items;
	delete table_items;
}

void sql_delete::set_table_items(composite_item_t *table_items_)
{
	table_items = table_items_;
}

void sql_delete::set_where_items(relation_item_t *where_items_)
{
	where_items = where_items_;
}

bool sql_delete::gen_data()
{
	if (!table_items)
		return false;

	sql_tree tree(where_items);
	for (sql_tree::const_iterator tree_iter = tree.begin(); tree_iter != tree.end(); ++tree_iter) {
		switch (tree_iter->type) {
		case ITEMTYPE_SIMPLE:
		case ITEMTYPE_FUNC:
		case ITEMTYPE_RELA:
			break;
		case ITEMTYPE_COMPOSIT:
			{
				composite_item_t *item = reinterpret_cast<composite_item_t *>(tree_iter->data);
				if (item->field_desc) {
					if (!(this->*bind_item_action)(*item))
						return false;
				}
			}
			break;
		}
	}

	(this->*finish_action)();
	return true;
}

string sql_delete::gen_sql()
{
	string sql = "DELETE ";
	if (!comments.empty()) {
		sql += comments;
		sql += " ";
	}
	sql += "FROM ";
	if (table_items) {
		// Collect table/pos pairs
		if (table_items->simple_item->schema.empty())
			table_pos_map[table_items->simple_item->value] = sql.length();
		else
			table_pos_map[table_items->simple_item->schema] = sql.length();
		
		table_items->gen_sql(sql, false);
	}

	if (where_items) {
		sql += " WHERE ";
		where_items->gen_sql(sql, true);
	}

	return sql;
}

string sql_delete::gen_table()
{
	string sql;
	if (table_items)
		table_items->simple_item->gen_sql(sql, true);
	return sql;
}

void sql_delete::print() const
{
	cout << "{table_items\n";
	if (table_items) {
		print_prefix += "  ";
		table_items->print();
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}table_items\n";

	cout << "{where_items\n";
	if (where_items) {
		print_prefix += "  ";
		where_items->print();
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}where_items\n";
}

relation_item_t * sql_delete::get_where_items()
{
	return where_items;
}

}
}

