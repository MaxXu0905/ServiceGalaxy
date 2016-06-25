#include <map>
#include "auto_update.h"
#include "sql_control.h"
#include "gp_control.h"
#include "struct_dynamic.h"
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

		auto_ptr<struct_dynamic> auto_data(db->create_data());
		struct_dynamic *data = auto_data.get();
		if (data == NULL) {
			cout << (boost::format("ERROR: Memory allocation failure")).str() << std::endl;
			exit(1);
		}

		vector<column_desc_t> column_desc;
		column_desc_t item;
		item.field_type = SQLTYPE_STRING;
		item.field_length = 8 + 1;
		column_desc.push_back(item);
		item.field_type = SQLTYPE_DATETIME;
		item.field_length = sizeof(time_t);
		column_desc.push_back(item);

		data->setSQL("select default_region_code{char}, exp_date{datetime}  from bds_area_code where area_code = :area_code and eff_date = :eff_date", column_desc);
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

