#if !defined(__SQL_CREATE_H__)
#define __SQL_CREATE_H__

#include "sql_statement.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;
using namespace std;

class sql_create : public sql_statement
{
public:
	sql_create();
	~sql_create();

	void set_table_items(composite_item_t *table_items_);
	void set_field_items(arg_list_t *field_items_);

	bool gen_data();
	void print() const;

	virtual string gen_code(const map<string, sql_field_t>& field_map, const bind_field_t *bind_fields);
	virtual string gen_table();
	arg_list_t * get_field_items();

private:
	virtual string gen_sql();

	composite_item_t *table_items;
	arg_list_t *field_items;
};

}
}

#endif

