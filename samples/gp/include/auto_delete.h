#if !defined(__AUTO_DELETE_H__)
#define __AUTO_DELETE_H__

#include "libpq-fe.h"
#include "struct_base.h"

namespace ai
{
namespace scci
{

class delete_class : public struct_base
{
public:
	dbtype_enum get_dbtype();
	bind_field_t * getSelectFields(int index = 0);
	bind_field_t * getBindFields(int index = 0);
	table_field_t * getTableFields(int index = 0);
	string& getSQL(int index = 0);
	int size() const;

	char area_code[200][8 + 1];

private:
	static bind_field_t select_fields0[];
	static bind_field_t bind_fields0[];
	static table_field_t table_fields0[];
	static bind_field_t *select_fields[];
	static bind_field_t *bind_fields[];
	static string _sql[1];
	static table_field_t *table_fields[];
};

}
}

#endif

