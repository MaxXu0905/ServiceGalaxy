#if !defined(__SQL_DROP_H__)
#define __SQL_DROP_H__

#include "sql_statement.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;
using namespace std;

class sql_drop : public sql_statement
{
public:
	sql_drop();
	~sql_drop();

	void set_table_items(composite_item_t *table_items_);

	bool gen_data();
	void print() const;

	virtual string gen_code(const map<string, sql_field_t>& field_map, const bind_field_t *bind_fields);
	virtual string gen_table();

private:
	virtual string gen_sql();

	composite_item_t *table_items;
};

}
}

#endif

