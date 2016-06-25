#include "sql_base.h"
#include "dbc_control.h"
#include "auto_delete.h"

namespace ai
{
namespace scci
{

bind_field_t delete_class::select_fields0[] = {
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t delete_class::bind_fields0[] = {
	{ SQLTYPE_STRING, ":area_code", 9, offsetof(delete_class, area_code), sizeof(short), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

string delete_class::_sql[] = {
	"DELETE FROM bds_area_code WHERE area_code = :area_code"
};

table_field_t delete_class::table_fields0[] = {
	{ "bds_area_code", 12 },
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
	return DBTYPE_DBC;
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

