#if !defined(__STRUCT_DYNAMIC_H__)
#define __STRUCT_DYNAMIC_H__

#include <string>
#include "struct_base.h"
#include "sql_base.h"
#include <boost/thread.hpp>

namespace ai
{
namespace scci
{

class sql_statement;
class column_desc_t;

class struct_dynamic : public struct_base
{
public:
	~struct_dynamic();

	dbtype_enum get_dbtype();
	void setSQL(const string& sql, const vector<column_desc_t>& binds = vector<column_desc_t>());
	void alloc_select();
	int size() const;

private:
	struct_dynamic(dbtype_enum dbtype_ = DBTYPE_ORACLE, bool ind_required_ = false, int _size_ = 1000);

	void clear();
	bind_field_t * getSelectFields(int index = 0);
	bind_field_t * getBindFields(int index = 0);
	string& getSQL(int index = 0);
	table_field_t * getTableFields(int index = 0);
	void alloc_bind(const vector<column_desc_t>& binds);

	dbtype_enum dbtype;
	bool ind_required;
	vector<bind_field_t> select_fields;
	vector<bind_field_t> bind_fields;
	string _sql;
	vector<table_field_t> table_fields;
	int _size;
	int select_count;
	int bind_count;
	char **select_data;
	char **bind_data;
	char **select_inds;
	char **bind_inds;

	static boost::mutex mutex;

	friend class sql_statement;
	friend class Generic_Database;
	friend class Generic_Statement;
};

}
}

#endif

