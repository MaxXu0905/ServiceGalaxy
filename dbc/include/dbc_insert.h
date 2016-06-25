#if !defined(__DBC_INSERT_H__)
#define __DBC_INSERT_H__

#include "dbc_internal.h"
#include "dbc_api.h"

using namespace std;
using namespace ai::scci;

namespace ai
{
namespace sg
{

class dbc_insert : public dbc_manager
{
public:
	dbc_insert();
	~dbc_insert();

	int run(int argc, char **argv);

private:
	int execute();

	std::string target_table;
	std::string select_sql;
	std::string user_name;
	std::string password;

	Generic_Database *db;
	Generic_Statement *select_stmt;
	Generic_Statement *insert_stmt;
	Generic_ResultSet *rset;

	struct_dynamic *select_data;
	struct_dynamic *insert_data;
};

}
}

#endif

