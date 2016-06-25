#include "sql_base.h"
#include "gp_control.h"
#include "auto_update.h"

namespace ai
{
namespace scci
{

bind_field_t update_class::select_fields0[] = {
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

bind_field_t update_class::bind_fields0[] = {
	{ SQLTYPE_CHAR, ":default_region_code", sizeof(char), offsetof(update_class, default_region_code), sizeof(short), -1 },
	{ SQLTYPE_STRING, ":area_code", 9, offsetof(update_class, area_code), sizeof(short), -1 },
	{ SQLTYPE_DATETIME, ":eff_date", sizeof(time_t), offsetof(update_class, eff_date), sizeof(short), -1 },
	{ SQLTYPE_UNKNOWN, "", 0, -1, 0, -1 }
};

string update_class::_sql[] = {
	"UPDATE bds_area_code SET default_region_code = $1 WHERE area_code = $2 AND eff_date = $3"
};

table_field_t update_class::table_fields0[] = {
	{ "bds_area_code", 7 },
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
	return DBTYPE_GP;
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

