#if !defined(__AUTO_MIXED_H__)
#define __AUTO_MIXED_H__

#include "oci.h"
#include "struct_base.h"

namespace ai
{
namespace scci
{

class mixed_class : public struct_base
{
public:
	dbtype_enum get_dbtype();
	bind_field_t * getSelectFields(int index = 0);
	bind_field_t * getBindFields(int index = 0);
	table_field_t * getTableFields(int index = 0);
	string& getSQL(int index = 0);
	int size() const;

	char col1[200][10 + 1];
	int col2[200];
	int col3[200];
	OCIDate col4[200];

private:
	static bind_field_t select_fields0[];
	static bind_field_t bind_fields0[];
	static table_field_t table_fields0[];
	static bind_field_t select_fields1[];
	static bind_field_t bind_fields1[];
	static table_field_t table_fields1[];
	static bind_field_t select_fields2[];
	static bind_field_t bind_fields2[];
	static table_field_t table_fields2[];
	static bind_field_t select_fields3[];
	static bind_field_t bind_fields3[];
	static table_field_t table_fields3[];
	static bind_field_t *select_fields[];
	static bind_field_t *bind_fields[];
	static string _sql[4];
	static table_field_t *table_fields[];
};

}
}

#endif

