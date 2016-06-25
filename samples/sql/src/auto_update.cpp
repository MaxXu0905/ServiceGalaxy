#include "sql_base.h"
#include "oracle_control.h"
#include "auto_update.h"

namespace ai
{
namespace scci
{

bind_field_t update_class::select_fields0[] = {
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t update_class::bind_fields0[] = {
	{ SQLTYPE_INT, ":col2", sizeof(int), offsetof(update_class, col2), sizeof(sb2), -1 },
	{ SQLTYPE_INT, ":col3", sizeof(int), offsetof(update_class, col3), sizeof(sb2), -1 },
	{ SQLTYPE_STRING, ":col1", 11, offsetof(update_class, col1), sizeof(sb2), -1 },
	{ SQLTYPE_DATETIME, ":col4", sizeof(OCIDate), offsetof(update_class, col4), sizeof(sb2), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

string update_class::_sql[] = {
	"UPDATE test_table SET col2 = :col2, col3 = :col3 WHERE col1 = :col1 AND col4 = :col4"
};

table_field_t update_class::table_fields0[] = {
	{ "test_table", 7 },
	{ "", -1 }
};

bind_field_t *update_class::select_fields[] = {
	update_class::select_fields0
};

bind_field_t *update_class::bind_fields[] = {
	update_class::bind_fields0
};

table_field_t *update_class::table_fields[] = {
	update_class::table_fields0
};

dbtype_enum update_class::get_dbtype()
{
	return DBTYPE_ORACLE;
}

bind_field_t * update_class::getSelectFields(int index)
{
	return select_fields[index];
}

bind_field_t * update_class::getBindFields(int index)
{
	return bind_fields[index];
}

string& update_class::getSQL(int index)
{
	return _sql[index];
}

table_field_t * update_class::getTableFields(int index)
{
	return table_fields[index];
}

int update_class::size() const
{
	return 200;
}

}
}

