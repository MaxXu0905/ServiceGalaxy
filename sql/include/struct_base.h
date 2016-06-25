#if !defined(__STRUCT_BASE_H__)
#define __STRUCT_BASE_H__

#include <string>
#include "machine.h"
#include "sql_base.h"

namespace ai
{
namespace scci
{

using namespace std;

class bind_field_t;
class table_field_t;

class struct_base
{
public:
	struct_base() {}
	virtual ~struct_base() {}
	virtual dbtype_enum get_dbtype() = 0;
	virtual bind_field_t * getSelectFields(int index = 0) = 0;
	virtual bind_field_t * getBindFields(int index = 0) = 0;
	virtual string& getSQL(int index = 0) = 0;
	virtual table_field_t * getTableFields(int index = 0) = 0;
	virtual int size() const = 0;
	virtual void alloc_select() {}
};

}
}

#endif

