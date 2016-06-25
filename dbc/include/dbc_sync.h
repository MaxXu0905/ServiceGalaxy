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

int const SQL_CTX_INSERT = 0;
int const SQL_CTX_UPDATE = 1;
int const SQL_CTX_DELETE = 2;
int const SQL_CTX_TOTAL = 3;

struct sql_struct_t
{
	Generic_Statement *stmt;
	struct_dynamic *dynamic_data;
 	int execute_count;
};

class dbc_sync : public dbc_manager
{
public:
	dbc_sync();
	~dbc_sync();

	void run();

private:
	bool run_once();
	void create_db(dbc_ue_t *ue);
	void create_stmt(dbc_te_t *te, int type);
	void add_bind(const dbc_data_field_t& data_field, vector<column_desc_t>& binds);
	void execute_insert(dbc_te_t *te, const char *row_data);
	void execute_update(dbc_te_t *te, const char *row_data, const char *old_row_data);
	void execute_delete(dbc_te_t *te, const char *row_data);
	void executeUpdate();
	void set_data(const dbc_data_field_t& data_field, int param_index, const char *data, Generic_Statement *stmt);

	dbc_api& api_mgr;
	sqlite3 *sqlite_db;
	sqlite3_stmt *sqlite_stmt;
	Generic_Database *db;
	sql_struct_t sql_ctx[SQL_CTX_TOTAL];
};

}
}

#endif

