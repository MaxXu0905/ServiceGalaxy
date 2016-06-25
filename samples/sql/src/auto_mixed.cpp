#include "sql_base.h"
#include "oracle_control.h"
#include "auto_mixed.h"

namespace ai
{
namespace scci
{

bind_field_t mixed_class::select_fields0[] = {
	{ SQLTYPE_STRING, "col1", 11, offsetof(mixed_class, col1), sizeof(sb2), -1 },
	{ SQLTYPE_INT, "col2", sizeof(int), offsetof(mixed_class, col2), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t mixed_class::bind_fields0[] = {
	{ SQLTYPE_INT, ":col3", sizeof(int), offsetof(mixed_class, col3), sizeof(sb2), -1 },
	{ SQLTYPE_DATETIME, ":col4", sizeof(OCIDate), offsetof(mixed_class, col4), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t mixed_class::select_fields1[] = {
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t mixed_class::bind_fields1[] = {
	{ SQLTYPE_STRING, ":col1", 11, offsetof(mixed_class, col1), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col2", sizeof(int), offsetof(mixed_class, col2), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col3", sizeof(int), offsetof(mixed_class, col3), sizeof(sb2), -1 },
	{ SQLTYPE_DATETIME, ":col4", sizeof(OCIDate), offsetof(mixed_class, col4), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t mixed_class::select_fields2[] = {
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t mixed_class::bind_fields2[] = {
	{ SQLTYPE_INT, ":col2", sizeof(int), offsetof(mixed_class, col2), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col3", sizeof(int), offsetof(mixed_class, col3), sizeof(sb2), -1 },
	{ SQLTYPE_STRING, ":col1", 11, offsetof(mixed_class, col1), sizeof(sb2), -1 },
	{ SQLTYPE_DATETIME, ":col4", sizeof(OCIDate), offsetof(mixed_class, col4), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t mixed_class::select_fields3[] = {
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t mixed_class::bind_fields3[] = {
	{ SQLTYPE_STRING, ":col1", 11, offsetof(mixed_class, col1), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col2", sizeof(int), offsetof(mixed_class, col2), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col3", sizeof(int), offsetof(mixed_class, col3), sizeof(sb2), -1 },
	{ SQLTYPE_DATETIME, ":col4", sizeof(OCIDate), offsetof(mixed_class, col4), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

string mixed_class::_sql[] = {
	"SELECT col1, col2 FROM test_table2 WHERE col3 = :col3 AND col4 = :col4",
	"INSERT INTO test_table(col1, col2, col3, col4) VALUES(:col1, :col2, :col3, :col4)",
	"UPDATE test_table SET col2 = :col2, col3 = :col3 WHERE col1 = :col1 AND col4 = :col4",
	"DELETE FROM test_table WHERE col1 = :col1 AND col2 = :col2 AND col3 = :col3 AND col4 = :col4"
};

table_field_t mixed_class::table_fields0[] = {
	{ "test_table2", 23 },
	{ "", -1 }
};

table_field_t mixed_class::table_fields1[] = {
	{ "test_table", 12 },
	{ "", -1 }
};

table_field_t mixed_class::table_fields2[] = {
	{ "test_table", 7 },
	{ "", -1 }
};

table_field_t mixed_class::table_fields3[] = {
	{ "test_table", 12 },
	{ "", -1 }
};

bind_field_t *mixed_class::select_fields[] = {
	mixed_class::select_fields0,
	mixed_class::select_fields1,
	mixed_class::select_fields2,
	mixed_class::select_fields3
};

bind_field_t *mixed_class::bind_fields[] = {
	mixed_class::bind_fields0,
	mixed_class::bind_fields1,
	mixed_class::bind_fields2,
	mixed_class::bind_fields3
};

table_field_t *mixed_class::table_fields[] = {
	mixed_class::table_fields0,
	mixed_class::table_fields1,
	mixed_class::table_fields2,
	mixed_class::table_fields3
};

dbtype_enum mixed_class::get_dbtype()
{
	return DBTYPE_ORACLE;
}

bind_field_t * mixed_class::getSelectFields(int index)
{
	return select_fields[index];
}

bind_field_t * mixed_class::getBindFields(int index)
{
	return bind_fields[index];
}

string& mixed_class::getSQL(int index)
{
	return _sql[index];
}

table_field_t * mixed_class::getTableFields(int index)
{
	return table_fields[index];
}

int mixed_class::size() const
{
	return 200;
}

}
}

