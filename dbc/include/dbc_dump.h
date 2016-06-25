#if !defined(__DBC_SYNC_H__)
#define __DBC_SYNC_H__

#include "dbc_struct.h"
#include "dbct_ctx.h"
#include "sql_control.h"
#include "struct_dynamic.h"
#include "dbc_api.h"

namespace ai
{
namespace sg
{

using namespace ai::scci;

class dbc_dump;

class dbc_dump : public dbc_manager
{
public:
	dbc_dump(int table_id_, const string& table_name_, int parallel_, long commit_size_,
		const string& user_name, const string& password);
	~dbc_dump();

	long run();

private:
	void execute(int parallel_slot, int& retval);
	void add_bind(const dbc_data_field_t& data_field, vector<column_desc_t>& binds);
	void set_data(const dbc_data_field_t& data_field, int param_index, const char *data, Generic_Statement *stmt);

	dbc_api& api_mgr;
	std::string sql_str;
	vector<column_desc_t> binds;

	int table_id;
	string table_name;
	int parallel;
	long commit_size;
};

}
}

#endif

