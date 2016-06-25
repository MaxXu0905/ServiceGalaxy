#include "sql_select.h"
#include "sql_tree.h"

namespace ai
{
namespace scci
{

extern string print_prefix;

sql_select::sql_select()
{
	distinct_flag = false;
	select_items = NULL;
	table_items = NULL;
	where_items = NULL;
	group_items = NULL;
	order_items = NULL;
	having_items = NULL;
	lock_flag = false;
}

sql_select::~sql_select()
{
	arg_list_t::iterator iter;

	delete having_items;
	if (order_items) {
		for (iter = order_items->begin(); iter != order_items->end(); ++iter)
			delete (*iter);
		delete order_items;
	}
	if (group_items) {
		for (iter = group_items->begin(); iter != group_items->end(); ++iter)
			delete (*iter);
		delete group_items;
	}
	delete where_items;
	if (table_items) {
		for (iter = table_items->begin(); iter != table_items->end(); ++iter)
			delete (*iter);
		delete table_items;
	}
	if (select_items) {
		for (iter = select_items->begin(); iter != select_items->end(); ++iter)
			delete (*iter);
		delete select_items;
	}
}

void sql_select::set_distinct_flag(bool distinct_flag_)
{
	distinct_flag = distinct_flag_;
}

void sql_select::set_select_items(arg_list_t *select_items_)
{
	select_items = select_items_;
}

void sql_select::set_table_items(arg_list_t *table_items_)
{
	table_items = table_items_;
}

void sql_select::set_where_items(relation_item_t *where_items_)
{
	where_items = where_items_;
}

void sql_select::set_group_items(arg_list_t *group_items_)
{
	group_items = group_items_;
}

void sql_select::set_order_items(arg_list_t *order_items_)
{
	order_items = order_items_;
}

void sql_select::set_having_items(relation_item_t *having_items_)
{
	having_items = having_items_;
}

bool sql_select::gen_data()
{
	arg_list_t::iterator iter;

	if (!select_items || select_items->size() == 0)
		return false;

	for (iter = select_items->begin(); iter != select_items->end(); ++iter) {
		simple_item_t *simple_item = (*iter)->simple_item;
		field_desc_t *field_desc = (*iter)->field_desc;
		if (!dynamic_flag && !field_desc) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Select item {1} missing field definition") % (iter - select_items->begin() + 1)).str(SGLOCALE));
			return false;
		}

		if (!dynamic_flag && field_desc->alias.empty() && simple_item == NULL) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Only simple item can ignore alias name")).str(SGLOCALE));
			return false;
		}

		if (!(this->*select_item_action)(*(*iter)))
			return false;
	}

	sql_tree tree;
	tree.set_root(where_items);
	tree.set_root(having_items);
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

void sql_select::set_lock_flag(bool lock_flag_)
{
	lock_flag = lock_flag_;
}

string sql_select::gen_sql()
{
	arg_list_t::iterator iter;

	string sql = "SELECT ";
	if (!comments.empty()) {
		sql += comments;
		sql += " ";
	}
	if (distinct_flag)
		sql += "DISTINCT ";
	if (select_items) {
		bool first = true;
		for (iter = select_items->begin(); iter != select_items->end(); ++iter) {
			if (first)
				first = false;
			else
				sql += ", ";
			(*iter)->gen_sql(sql, true);
		}
	}

	sql += " FROM ";
	if (table_items) {
		bool first = true;
		for (iter = table_items->begin(); iter != table_items->end(); ++iter) {
			if (first)
				first = false;
			else
				sql += ", ";

			// Collect table/pos pairs
			composite_item_t *item = *iter;
			if (item->simple_item->schema.empty())
				table_pos_map[item->simple_item->value] = sql.length();
			else
				table_pos_map[item->simple_item->schema] = sql.length();

			(*iter)->gen_sql(sql, false);
		}
	}

	if (where_items) {
		sql += " WHERE ";
		where_items->gen_sql(sql, true);
	}

	if (group_items) {
		sql += " GROUP BY ";
		bool first = true;
		for (iter = group_items->begin(); iter != group_items->end(); ++iter) {
			if (first)
				first = false;
			else
				sql += ", ";
			(*iter)->gen_sql(sql, true);
		}
	}

	if (order_items) {
		sql += " ORDER BY ";
		bool first = true;
		for (iter = order_items->begin(); iter != order_items->end(); ++iter) {
			if (first)
				first = false;
			else
				sql += ", ";
			(*iter)->gen_sql(sql, true);
		}
	}

	if (having_items) {
		sql += " HAVING ";
		having_items->gen_sql(sql, true);
	}

	if (lock_flag)
		sql += " FOR UPDATE";

	return sql;
}

string sql_select::gen_table()
{
	string sql;
	if (table_items) {
		composite_item_t *item = *table_items->begin();
		item->simple_item->gen_sql(sql, true);
	}
	return sql;
}

void sql_select::print() const
{
	int index;
	arg_list_t::iterator iter;

	cout << "{select_items\n";
	if (distinct_flag) {
		print_prefix += "  ";
		cout << print_prefix << "distinct\n";
		print_prefix.resize(print_prefix.size() - 2);
	}
	if (select_items) {
		print_prefix += "  ";
		index = 0;
		for (iter = select_items->begin(); iter != select_items->end(); ++iter) {
			cout << print_prefix << "index = " << ++index << "\n";
			(*iter)->print();
		}
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}select_items\n";

	cout << "{table_items\n";
	if (table_items) {
		print_prefix += "  ";
		index = 0;
		for (iter = table_items->begin(); iter != table_items->end(); ++iter) {
			cout << print_prefix << "index = " << ++index << "\n";
			(*iter)->print();
		}
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

	cout << "{group_items\n";
	if (group_items) {
		print_prefix += "  ";
		index = 0;
		for (iter = group_items->begin(); iter != group_items->end(); ++iter) {
			cout << print_prefix << "index = " << ++index << "\n";
			(*iter)->print();
		}
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}group_items\n";

	cout << "{order_items\n";
	if (order_items) {
		print_prefix += "  ";
		index = 0;
		for (iter = order_items->begin(); iter != order_items->end(); ++iter) {
			cout << print_prefix << "index = " << ++index << "\n";
			(*iter)->print();
		}
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}order_items\n";

	cout << "{having_items\n";
	if (having_items) {
		print_prefix += "  ";
		having_items->print();
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}having_items\n";
}

void sql_select::push(vector<sql_item_t>& vector_root)
{
	sql_tree tree;
	tree.set_root(where_items);
	tree.set_root(having_items);

	for (sql_tree::const_iterator tree_iter = tree.begin(); tree_iter != tree.end(); ++tree_iter)
		vector_root.push_back(*tree_iter);
}

arg_list_t * sql_select::get_select_items()
{
	return select_items;
}

relation_item_t * sql_select::get_where_items()
{
	return where_items;
}

arg_list_t * sql_select::get_group_items()
{
	return group_items;
}

arg_list_t * sql_select::get_order_items()
{
	return order_items;
}

relation_item_t * sql_select::get_having_items()
{
	return having_items;
}

}
}

