#include <map>
#include "sql_control.h"
#include "dbc_api.h"
#include "datastruct_structs.h"
#include "dbc_control.h"
#include "struct_dynamic.h"
#include "sg_api.h"

#define TESTTABLE t_tf_chl_chnl_developer
using namespace ai::sg;
using namespace ai::scci;
using namespace std;
bool compareValue(TESTTABLE &key, TESTTABLE &result);
int main(int argc, char **argv) {
	sg_api& api_mgr = sg_api::instance();
	api_mgr.set_procname(argv[0]);

	try {
		map<string, string> conn_info;
		conn_info["user_name"]="renyz";
		conn_info["password"]="123"

		auto_ptr<Generic_Database> auto_db(new Dbc_Database());
		Generic_Database *db = auto_db.get();
		db->connect(conn_info);
		auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
		Generic_Statement *stmt = auto_stmt.get();

		auto_ptr<struct_dynamic> auto_data(new struct_dynamic(DBTYPE_DBC));
		struct_dynamic *data = auto_data.get();
		if (data == NULL) {
			cout << (boost::format("ERROR: Memory allocation failure")).str() << std::endl;
			exit(1);
		}
		data->setSQL(
				"select dev_id_num{long}, channel_id{char[7]},eff_date{datetime},exp_date{datetime}, chl_dev_state{char[8]} from v_tf_chl_chnl_developer");
		stmt->bind(data);
		auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());

		dbc_api& api_mgr = dbc_api::instance();
		TESTTABLE key;
		int i = 0;
		int errorCount = 0;
		int const maxErrorCount=10;
		while (auto_rset->next()) {
			i++;
			TESTTABLE result;
			key.dev_id_num = auto_rset->getLong(1);
			strcpy(key.channel_id, auto_rset->getString(2).c_str());
			key.eff_date = auto_rset->getLong(3);
			key.exp_date = auto_rset->getLong(4);
			strcpy(key.chl_dev_state, auto_rset->getString(5).c_str());
			if (api_mgr.find(T_TF_CHL_CHNL_DEVELOPER, 0, &key, &result) == 0) {
				std::cout << "fails:" << i << " row no found!" << std::endl;
				cout << "key values:" << endl;
				cout << "		dev_id_num=" << key.dev_id_num << endl;
				cout << "		channel_id=" << key.channel_id << endl;
				cout << "		eff_date=" << key.eff_date << endl;
				cout << "		exp_date=" << key.exp_date << endl;
				cout << "		chl_dev_state=" << key.chl_dev_state << endl;
				errorCount++;

			}else if (!compareValue(key, result)) {
				std::cout << "fails:" << i << " row not equal!" << std::endl;
				cout << "key values:" << endl;
				cout << "		dev_id_num=" << key.dev_id_num << endl;
				cout << "		channel_id=" << key.channel_id << endl;
				cout << "		eff_date=" << key.eff_date << endl;
				cout << "		exp_date=" << key.exp_date << endl;
				cout << "		chl_dev_state=" << key.chl_dev_state << endl;
				cout << "result values:" << endl;
				cout << "		dev_id_num=" << result.dev_id_num << endl;
				cout << "		channel_id=" << result.channel_id << endl;
				cout << "		eff_date=" << result.exp_date << endl;
				cout << "		exp_date=" << result.eff_date << endl;
				cout << "		chl_dev_state=" << result.chl_dev_state << endl;
				errorCount++;

			}
			if(maxErrorCount<=errorCount){
				std::cout << "total fails:" << errorCount << " rows" << std::endl;
				cout<<"total error beyond maxCount"<<endl;
				return -1;
			}
		}
		std::cout << "total fails:" << errorCount << " rows" << std::endl;
		std::cout << "total success:" << i-errorCount << " rows" << std::endl;
		return 0;
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		return 1;
	}
}
bool compareValue(TESTTABLE &key, TESTTABLE &result) {
	return key.dev_id_num == key.dev_id_num && !strcmp(key.channel_id,
			result.channel_id) && key.eff_date == result.eff_date
			&& key.exp_date == result.exp_date && !strcmp(key.chl_dev_state,
			result.chl_dev_state);

}

