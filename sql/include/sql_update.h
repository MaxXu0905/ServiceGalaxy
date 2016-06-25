#if !defined(__SQL_UPDATE_H__)
#define __SQL_UPDATE_H__

#include "sql_statement.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;
using namespace std;

class sql_update : public sql_statement
{
public:
	sql_update();
	~sql_update();

	void set_table_items(composite_item_t *table_items_);
	void set_update_items(arg_list_t *update_items_);
	void set_where_items(relation_item_t *where_items_);
	
	bool gen_data();
	void print() const;
	
	arg_list_t * get_update_items();
	relation_item_t * get_where_items();

	virtual string gen_table();

private:
	virtual string gen_sql();
	composite_item_t *table_items;
	arg_list_t *update_items;
	relation_item_t *where_items;
};

}
}

#endif

