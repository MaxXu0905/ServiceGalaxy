#include "sql_base.h"
#include "oracle_control.h"
#include "auto_insert.h"

namespace ai
{
namespace scci
{

bind_field_t insert_class::select_fields0[] = {
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t insert_class::bind_fields0[] = {
	{ SQLTYPE_STRING, ":col1", 11, offsetof(insert_class, col1), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col2", sizeof(int), offsetof(insert_class, col2), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col3", sizeof(int), offsetof(insert_class, col3), sizeof(sb2), -1 },
	{ SQLTYPE_DATETIME, ":col4", sizeof(OCIDate), offsetof(insert_class, col4), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

string insert_class::_sql[] = {
	"INSERT INTO test_table(col1, col2, col3, col4) VALUES(:col1, :col2, :col3, :col4)"
};

table_field_t insert_class::table_fields0[] = {
	{ "test_table", 12 },
	{ "", -1 }
};

bind_field_t *insert_class::select_fields[] = {
	insert_class::select_fields0
};

bind_field_t *insert_class::bind_fields[] = {
	insert_class::bind_fields0
};

table_field_t *insert_class::table_fields[] = {
	insert_class::table_fields0
};

dbtype_enum insert_class::get_dbtype()
{
	return DBTYPE_ORACLE;
}

bind_field_t * insert_class::getSelectFields(int index)
{
	return select_fields[index];
}

bind_field_t * insert_class::getBindFields(int index)
{
	return bind_fields[index];
}

string& insert_class::getSQL(int index)
{
	return _sql[index];
}

table_field_t * insert_class::getTableFields(int index)
{
	return table_fields[index];
}

int insert_class::size() const
{
	return 200;
}

}
}

