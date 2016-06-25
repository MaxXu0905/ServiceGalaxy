#include <map>
#include "auto_select.h"
#include "sql_control.h"
#include "dbc_control.h"
#include "sg_api.h"

using namespace ai::sg;
using namespace ai::scci;
using namespace std;

int main(int argc, char **argv)
{
	sg_api& api_mgr = sg_api::instance();
	api_mgr.set_procname(argv[0]);

	try {
		map<string, string> conn_info;
		conn_info["user_name"] = "xugq";
		conn_info["password"] = "123";

		auto_ptr<Generic_Database> auto_db(new Dbc_Database());
		Generic_Database *db = auto_db.get();
		db->connect(conn_info);

		auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
		Generic_Statement *stmt = auto_stmt.get();

		auto_ptr<struct_base> auto_data(new select_class());
		struct_base *data = auto_data.get();
		if (data == NULL) {
			cout << (_("ERROR: Memory allocation failure")).str(SGLOCALE) << std::endl;
			exit(1);
		}

		stmt->bind(data);

		stmt->setString(1, "111");
		stmt->setLong(2, 1199116800);

		auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
		Generic_ResultSet *rset = auto_rset.get();

		while (rset->next()) {
			cout << rset->getColumnName(1) << " = " << rset->getChar(1) << std::endl;
			cout << rset->getColumnName(2) << " = " << rset->getLong(2) << std::endl;
		}

		return 0;
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		return 1;
	}
}
