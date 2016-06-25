#include "sql_base.h"
#include "dbc_control.h"
#include "auto_insert.h"

namespace ai
{
namespace scci
{

bind_field_t insert_class::select_fields0[] = {
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t insert_class::bind_fields0[] = {
	{ SQLTYPE_CHAR, ":default_region_code", sizeof(char), offsetof(insert_class, default_region_code), sizeof(short), -1 },
	{ SQLTYPE_STRING, ":area_code", 9, offsetof(insert_class, area_code), sizeof(short), -1 },
	{ SQLTYPE_DATETIME, ":eff_date", sizeof(time_t), offsetof(insert_class, eff_date), sizeof(short), -1 },
	{ SQLTYPE_DATETIME, ":exp_date", sizeof(time_t), offsetof(insert_class, exp_date), sizeof(short), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

string insert_class::_sql[] = {
	"INSERT INTO bds_area_code(default_region_code, area_code, eff_date, exp_date) VALUES(:default_region_code, :area_code, :eff_date, :exp_date)"
};

table_field_t insert_class::table_fields0[] = {
	{ "bds_area_code", 12 },
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
	return DBTYPE_DBC;
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

