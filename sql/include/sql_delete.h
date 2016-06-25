#if !defined(__SQL_DELETE_H__)
#define __SQL_DELETE_H__

#include "sql_statement.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;
using namespace std;

class sql_delete : public sql_statement
{
public:
	sql_delete();
	~sql_delete();

	void set_table_items(composite_item_t *table_items_);
	void set_where_items(relation_item_t *where_items_);
	
	bool gen_data();
	void print() const;

	virtual string gen_table();
	relation_item_t * get_where_items();

private:
	virtual string gen_sql();
	
	composite_item_t *table_items;
	relation_item_t *where_items;
};

}
}

#endif

