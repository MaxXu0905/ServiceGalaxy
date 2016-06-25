#include "sql_insert.h"
#include "sql_tree.h"
#include "sql_select.h"

namespace ai
{
namespace scci
{

extern string print_prefix;

sql_insert::sql_insert()
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();

	table_items = NULL;
	ident_items = NULL;
	insert_items = NULL;
	select_items = NULL;
}

sql_insert::~sql_insert()
{
	arg_list_t::iterator iter;
	
	delete select_items;
	if (insert_items) {
		for (iter = insert_items->begin(); iter != insert_items->end(); ++iter)
			delete (*iter);
		delete insert_items;
	}
	if (ident_items) {
		for (iter = ident_items->begin(); iter != ident_items->end(); ++iter)
			delete (*iter);
		delete ident_items;
	}
	delete table_items;
}

void sql_insert::set_table_items(composite_item_t *table_items_)
{
	table_items = table_items_;
}

void sql_insert::set_ident_items(arg_list_t *ident_items_)
{
	ident_items = ident_items_;
}
	
void sql_insert::set_insert_items(arg_list_t *insert_items_)
{
	insert_items = insert_items_;
}

void sql_insert::set_select_items(sql_select *select_items_)
{
	select_items = select_items_;
}

bool sql_insert::gen_data()
{
	string full_name;

	if (!table_items || ((!insert_items || insert_items->size() == 0) && !select_items))
		return false;

	if (insert_items) {
		sql_tree tree(insert_items);

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
	} else {
		vector<sql_item_t> vector_root;
		select_items->push(vector_root);

		for (vector<sql_item_t>::const_iterator iter = vector_root.begin(); iter != vector_root.end(); ++iter) {
			switch (iter->type) {
			case ITEMTYPE_SIMPLE:
			case ITEMTYPE_FUNC:
			case ITEMTYPE_RELA:
				break;
			case ITEMTYPE_COMPOSIT:
				{
					composite_item_t *item = reinterpret_cast<composite_item_t *>(iter->data);
					if (item->field_desc) {
						if (!(this->*bind_item_action)(*item))
							return false;
					}
				}
				break;
			}
		}
	}

	(this->*finish_action)();
	return true;
}

string sql_insert::gen_sql()
{
	arg_list_t::iterator iter;
	
	string sql = "INSERT ";
	if (!comments.empty()) {
		sql += comments;
		sql += " ";
	}
	sql += "INTO ";
	if (table_items) {
		// Collect table/pos pairs
		if (table_items->simple_item->schema.empty())
			table_pos_map[table_items->simple_item->value] = sql.length();
		else
			table_pos_map[table_items->simple_item->schema] = sql.length();
		
		table_items->gen_sql(sql, false);
	}

	if (ident_items) {
		bool first = true;
		sql += "(";
		for (iter = ident_items->begin(); iter != ident_items->end(); ++iter) {
			if (first)
				first = false;
			else
				sql += ", ";
			(*iter)->gen_sql(sql, true);
		}
		sql += ")";
	}

	if (insert_items) {
		sql += " VALUES(";
		if (insert_items) {
			bool first = true;
			for (iter = insert_items->begin(); iter != insert_items->end(); ++iter) {
				if (first)
					first = false;
				else
					sql += ", ";
				(*iter)->gen_sql(sql, true);
			}
		}
		sql += ")";
	} else {
		sql += " ";
		sql += select_items->gen_sql();
	}

	return sql;
}

string sql_insert::gen_table()
{
	string sql;
	if (table_items)
		table_items->simple_item->gen_sql(sql, true);
	return sql;
}

void sql_insert::print() const
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

	cout << "{ident_items\n";
	if (ident_items) {
		print_prefix += "  ";
		index = 0;
		for (iter = ident_items->begin(); iter != ident_items->end(); ++iter) {
			cout << print_prefix << "index = " << ++index << "\n";
			(*iter)->print();
		}
		print_prefix.resize(print_prefix.size() - 2);
	}
	cout << "}ident_items\n";

	cout << "{select_items\n";
	if (insert_items) {
		print_prefix += "  ";
		index = 0;
		for (iter = insert_items->begin(); iter != insert_items->end(); ++iter) {
			cout << print_prefix << "index = " << ++index << "\n";
			(*iter)->print();
		}
		print_prefix.resize(print_prefix.size() - 2);
	} else {
		select_items->print();
	}
	cout << "}select_items\n";
}

arg_list_t * sql_insert::get_ident_items()
{
	return ident_items;
}

arg_list_t * sql_insert::get_insert_items()
{
	return insert_items;
}

}
}

