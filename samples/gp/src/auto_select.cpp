#include "sql_base.h"
#include "gp_control.h"
#include "auto_select.h"

namespace ai
{
namespace scci
{

bind_field_t select_class::select_fields0[] = {
	{ SQLTYPE_CHAR, "default_region_code", sizeof(char), offsetof(select_class, default_region_code), sizeof(short), -1 },
	{ SQLTYPE_DATETIME, "exp_date", sizeof(time_t), offsetof(select_class, exp_date), sizeof(short), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t select_class::bind_fields0[] = {
	{ SQLTYPE_STRING, ":area_code", 9, offsetof(select_class, area_code), sizeof(short), -1 },
	{ SQLTYPE_DATETIME, ":eff_date", sizeof(time_t), offsetof(select_class, eff_date), sizeof(short), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

string select_class::_sql[] = {
	"SELECT default_region_code, exp_date FROM bds_area_code WHERE area_code = $1 AND eff_date = $2"
};

table_field_t select_class::table_fields0[] = {
	{ "bds_area_code", 42 },
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
	return DBTYPE_GP;
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

