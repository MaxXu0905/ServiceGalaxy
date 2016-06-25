#include "sql_update.h"
#include "sql_tree.h"

namespace ai
{
namespace scci
{

extern string print_prefix;

sql_update::sql_update()
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	table_items = NULL;
	update_items = NULL;
	where_items = NULL;
}

sql_update::~sql_update()
{
	arg_list_t::iterator iter;
	
	delete where_items;
	if (update_items) {
		for (iter = update_items->begin(); iter != update_items->end(); ++iter)
			delete (*iter);
		delete update_items;
	}
	delete table_items;
}

void sql_update::set_table_items(composite_item_t *table_items_)
{
	table_items = table_items_;
}

void sql_update::set_update_items(arg_list_t *update_items_)
{
	update_items = update_items_;
}

void sql_update::set_where_items(relation_item_t *where_items_)
{
	where_items = where_items_;
}

bool sql_update::gen_data()
{
	if (!table_items || !update_items || update_items->size() == 0)
		return false;

	sql_tree tree;
	tree.set_root(update_items);
	tree.set_root(where_items);
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

string sql_update::gen_sql()
{
	arg_list_t::iterator iter;
	
	string sql = "UPDATE ";
	if (!comments.empty()) {
		sql += comments;
		sql += " ";
	}
	if (table_items) {
		// Collect table/pos pairs
		if (table_items->simple_item->schema.empty())
			table_pos_map[table_items->simple_item->value] = sql.length();
		else
			table_pos_map[table_items->simple_item->schema] = sql.length();
		
		table_items->gen_sql(sql, false);
	}

	sql += " SET ";
	if (update_items) {
		bool first = true;
		for (iter = update_items->begin(); iter != update_items->end(); ++iter) {
			if (first)
				first = false;
			else
				sql += ", ";
			
			(*iter)->gen_sql(sql, true);
		}
	}

	if (where_items) {
		sql += " WHERE ";
		where_items->gen_sql(sql, true);
	}

	return sql;
}

string sql_update::gen_table()
{
	string sql;
	if (table_items)
		table_items->simple_item->gen_sql(sql, true);
	return sql;
}

void sql_update::print() const
{
	int index;
	arg_list_t::iterator iter;

	cout << "{table_items\n";
	if (table_items) {
		print_prefix += "  ";
		table_items->print();
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}table_items\n";

	cout << "{update_items\n";
	if (update_items) {
		print_prefix += "  ";
		index = 0;
		for (iter = update_items->begin(); iter != update_items->end(); ++iter) {
			cout << print_prefix << "index = " << ++index << "\n";
			(*iter)->print();
		}
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}update_items\n";

	cout << "{where_items\n";
	if (where_items) {
		print_prefix += "  ";
		where_items->print();
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}where_items\n";
}

arg_list_t * sql_update::get_update_items()
{
	return update_items;
}

relation_item_t * sql_update::get_where_items()
{
	return where_items;
}

}
}

