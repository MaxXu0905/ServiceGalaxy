#if !defined(__SQL_SELECT_H__)
#define __SQL_SELECT_H__

#include "sql_statement.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;

class sql_select : public sql_statement
{
public:
	sql_select();
	~sql_select();

	void set_distinct_flag(bool distinct_flag_);
	void set_select_items(arg_list_t *select_items_);
	void set_table_items(arg_list_t *table_items_);
	void set_where_items(relation_item_t *where_items_);
	void set_group_items(arg_list_t *group_items_);
	void set_order_items(arg_list_t *order_items_);
	void set_having_items(relation_item_t *having_items_);
	void set_lock_flag(bool lock_flag_);
	
	bool gen_data();
	void print() const;
	void push(vector<sql_item_t>& vector_root);
	
	arg_list_t * get_select_items();
	relation_item_t * get_where_items();
	arg_list_t * get_group_items();
	arg_list_t * get_order_items();
	relation_item_t * get_having_items();

	virtual string gen_sql();
	virtual string gen_table();

private:
	bool distinct_flag;
	arg_list_t *select_items;
	arg_list_t *table_items;
	relation_item_t *where_items;
	arg_list_t *group_items;
	arg_list_t *order_items;
	relation_item_t *having_items;
	bool lock_flag;
};

}
}

#endif

