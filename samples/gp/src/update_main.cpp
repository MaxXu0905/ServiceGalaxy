#include <map>
#include "auto_update.h"
#include "sql_control.h"
#include "gp_control.h"
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
		conn_info["hostaddr"] = "10.1.247.2";
		conn_info["port"] = "5801";
		conn_info["dbname"] = "testdb";
		conn_info["user"] = "gptest";
		auto_ptr<Generic_Database> auto_db(new GP_Database());
		Generic_Database *db = auto_db.get();
		db->connect(conn_info);

		auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
		Generic_Statement *stmt = auto_stmt.get();

		auto_ptr<struct_base> auto_data(new update_class());
		struct_base *data = auto_data.get();
		if (data == NULL) {
			cout << (boost::format("ERROR: Memory allocation failure")).str() << std::endl;
			exit(1);
		}

		stmt->bind(data);
		stmt->setChar(1, 'C');
		stmt->setString(2, "111");
		stmt->setLong(3, 1199116800);

		stmt->addIteration();
		stmt->setChar(1, 'D');
		stmt->setString(2, "222");
		stmt->setLong(3, 1199116800);

		stmt->executeUpdate();
		db->commit();

		return 0;
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		return 1;
	}
}
