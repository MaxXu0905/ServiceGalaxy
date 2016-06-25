#if !defined(__SQL_INSERT_H__)
#define __SQL_INSERT_H__

#include "sql_statement.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;
using namespace std;

class sql_select;
class sql_insert : public sql_statement
{
public:
	sql_insert();
	~sql_insert();

	void set_table_items(composite_item_t *table_items_);
	void set_ident_items(arg_list_t *ident_items_);
	void set_insert_items(arg_list_t *insert_items_);
	void set_select_items(sql_select *select_items_);
	
	bool gen_data();
	void print() const;
	
	arg_list_t * get_ident_items();
	arg_list_t * get_insert_items();

private:
	virtual string gen_sql();
	virtual string gen_table();

	composite_item_t *table_items;
	arg_list_t *ident_items;
	arg_list_t *insert_items;
	sql_select *select_items;
};

}
}

#endif

