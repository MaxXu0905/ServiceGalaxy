#include "sql_base.h"
#include "oracle_control.h"
#include "auto_delete.h"

namespace ai
{
namespace scci
{

bind_field_t delete_class::select_fields0[] = {
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t delete_class::bind_fields0[] = {
	{ SQLTYPE_STRING, ":col1", 11, offsetof(delete_class, col1), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col2", sizeof(int), offsetof(delete_class, col2), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col3", sizeof(int), offsetof(delete_class, col3), sizeof(sb2), -1 },
	{ SQLTYPE_DATETIME, ":col4", sizeof(OCIDate), offsetof(delete_class, col4), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

string delete_class::_sql[] = {
	"DELETE FROM test_table WHERE col1 = :col1 AND col2 = :col2 AND col3 = :col3 AND col4 = :col4"
};

table_field_t delete_class::table_fields0[] = {
	{ "test_table", 12 },
	{ "", -1 }
};

bind_field_t *delete_class::select_fields[] = {
	delete_class::select_fields0
};

bind_field_t *delete_class::bind_fields[] = {
	delete_class::bind_fields0
};

table_field_t *delete_class::table_fields[] = {
	delete_class::table_fields0
};

dbtype_enum delete_class::get_dbtype()
{
	return DBTYPE_ORACLE;
}

bind_field_t * delete_class::getSelectFields(int index)
{
	return select_fields[index];
}

bind_field_t * delete_class::getBindFields(int index)
{
	return bind_fields[index];
}

string& delete_class::getSQL(int index)
{
	return _sql[index];
}

table_field_t * delete_class::getTableFields(int index)
{
	return table_fields[index];
}

int delete_class::size() const
{
	return 200;
}

}
}

