#include "sql_base.h"
#include "oracle_control.h"
#include "auto_select.h"

namespace ai
{
namespace scci
{

bind_field_t select_class::select_fields0[] = {
	{ SQLTYPE_STRING, "col1", 11, offsetof(select_class, col1), sizeof(sb2), -1 },
	{ SQLTYPE_INT, "col2", sizeof(int), offsetof(select_class, col2), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t select_class::bind_fields0[] = {
	{ SQLTYPE_INT, ":col3", sizeof(int), offsetof(select_class, col3), sizeof(sb2), -1 },
	{ SQLTYPE_DATETIME, ":col4", sizeof(OCIDate), offsetof(select_class, col4), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

string select_class::_sql[] = {
	"SELECT col1, col2 FROM test_table2 WHERE col3 = :col3 AND col4 = :col4"
};

table_field_t select_class::table_fields0[] = {
	{ "test_table2", 23 },
	{ "", -1 }
};

bind_field_t *select_class::select_fields[] = {
	select_class::select_fields0
};

bind_field_t *select_class::bind_fields[] = {
	select_class::bind_fields0
};

table_field_t *select_class::table_fields[] = {
	select_class::table_fields0
};

dbtype_enum select_class::get_dbtype()
{
	return DBTYPE_ORACLE;
}

bind_field_t * select_class::getSelectFields(int index)
{
	return select_fields[index];
}

bind_field_t * select_class::getBindFields(int index)
{
	return bind_fields[index];
}

string& select_class::getSQL(int index)
{
	return _sql[index];
}

table_field_t * select_class::getTableFields(int index)
{
	return table_fields[index];
}

int select_class::size() const
{
	return 200;
}

}
}

