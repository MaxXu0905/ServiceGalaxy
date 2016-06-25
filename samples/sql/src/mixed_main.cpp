#include <map>
#include "auto_mixed.h"
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

		auto_ptr<struct_base> auto_data(new mixed_class());
		struct_base *data = auto_data.get();
		if (data == NULL) {
			cout << (_("ERROR: Memory allocation failure")).str(SGLOCALE) << std::endl;
			exit(1);
		}

		// Select
		stmt->setTable("test_table", "test_table2");
		stmt->bind(data, 0);

		datetime dt("20100101000000");

		stmt->setInt(1, 222);
		stmt->setDatetime(2, dt);

		auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
		Generic_ResultSet *rset = auto_rset.get();

		while (rset->next()) {
			cout << rset->getColumnName(1) << " = " << rset->getInt(1) << std::endl;
			cout << rset->getColumnName(2) << " = " << rset->getString(2) << std::endl;
		}

		// Insert
		stmt->bind(data, 1);

		stmt->setString(1, "222");
		stmt->setInt(2, 2);
		stmt->setInt(3, 222);
		stmt->setDatetime(4, dt);

		stmt->addIteration();
		stmt->setString(1, "333");
		stmt->setInt(2, 3);
		stmt->setInt(3, 333);
		stmt->setDatetime(4, dt);

		stmt->executeUpdate();
		db->commit();

		// Update
		stmt->bind(data, 2);

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

		// Delete
		stmt->bind(data, 3);

		stmt->setString(1, "222");
		stmt->setInt(2, 2);
		stmt->setInt(3, 222);
		stmt->setDatetime(4, dt);

		stmt->addIteration();
		stmt->setString(1, "333");
		stmt->setInt(2, 3);
		stmt->setInt(3, 333);
		stmt->setDatetime(4, dt);

		stmt->executeUpdate();
		db->commit();

		return 0;
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		return 1;
	}
}
