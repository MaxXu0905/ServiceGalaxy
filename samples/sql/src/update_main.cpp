#include <map>
#include "auto_update.h"
#include "sql_control.h"
#include "oracle_control.h"
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

		conn_info["user_name"] = "devusgdb1";
		conn_info["password"] = "devusg123";
		conn_info["connect_string"] = "devl10g1";

		auto_ptr<Generic_Database> auto_db(new Oracle_Database());
		Generic_Database *db = auto_db.get();
		db->connect(conn_info);

		auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
		Generic_Statement *stmt = auto_stmt.get();

		auto_ptr<struct_base> auto_data(new update_class());
		struct_base *data = auto_data.get();
		if (data == NULL) {
			cout << (_("ERROR: Memory allocation failure")).str(SGLOCALE) << std::endl;
			exit(1);
		}

		stmt->bind(data);

		datetime dt("20100101000000");

		stmt->setInt(1, 11111);
		stmt->setInt(2, 55555);
		stmt->setString(3, "222");
		stmt->setDatetime(4, dt);

		stmt->addIteration();
		stmt->setInt(1, 1111);
		stmt->setInt(2, 5555);
		stmt->setString(3, "333");
		stmt->setDatetime(4, dt);

		stmt->executeUpdate();
		db->commit();

		return 0;
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		return 1;
	}
}
