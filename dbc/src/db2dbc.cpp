#include "dbc_internal.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;
using namespace ai::scci;
using namespace ai::sg;
using namespace std;

string prefix;

void load_data_detail(int refresh_type, Generic_Database *db, int table_id, ofstream& ofs)
{
	auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(db->create_data());
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		cout << (_("ERROR: Memory allocation failure")).str(SGLOCALE) << std::endl;
		exit(1);
	}

	data->setSQL("select field_name{char[127]}, fetch_name{char[255]}, update_name{char[255]}, column_name{char[31]}, "
		"field_type{int}, field_size{int}, is_primary{int}, is_partition{int} "
		"from dbc_data_detail where table_id = :table_id{int} "
		"order by field_seq;");
	stmt->bind(data);

	stmt->setInt(1, table_id);

	auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
	Generic_ResultSet *rset = auto_rset.get();

	while (rset->next()) {
		ofs << prefix << "<field>\n";
		prefix += '\t';

		ofs << prefix << "<field_name>" << rset->getString(1) << "</field_name>\n";

		string fetch_name = rset->getString(2);
		if (memcmp(fetch_name.c_str(), "date_to_number(", 15) == 0)
			fetch_name = fetch_name.substr(15, fetch_name.length() - 16);

		if (fetch_name != rset->getString(1))
			ofs << prefix << "<fetch_name>" << fetch_name<< "</fetch_name>\n";

		if (rset->getString(3) != string(":") + rset->getString(1))
			ofs << prefix << "<update_name>" << rset->getString(3) << "</update_name>\n";

		if (rset->getString(4) != rset->getString(1))
			ofs << prefix << "<column_name>" << rset->getString(4) << "</column_name>\n";

		ofs << prefix << "<field_type>";

		switch (rset->getInt(5)) {
		case SQLTYPE_CHAR:
			ofs << "CHAR";
			break;
		case SQLTYPE_UCHAR:
			ofs << "UCHAR";
			break;
		case SQLTYPE_SHORT:
			ofs << "SHORT";
			break;
		case SQLTYPE_USHORT:
			ofs << "USHORT";
			break;
		case SQLTYPE_INT:
			ofs << "INT";
			break;
		case SQLTYPE_UINT:
			ofs << "UINT";
			break;
		case SQLTYPE_LONG:
			ofs << "LONG";
			break;
		case SQLTYPE_ULONG:
			ofs << "ULONG";
			break;
		case SQLTYPE_FLOAT:
			ofs << "FLOAT";
			break;
		case SQLTYPE_DOUBLE:
			ofs << "DOUBLE";
			break;
		case SQLTYPE_STRING:
			ofs << "STRING";
			break;
		case SQLTYPE_VSTRING:
			ofs << "VSTRING";
			break;
		case SQLTYPE_DATE:
			ofs << "DATE";
			break;
		case SQLTYPE_TIME:
			ofs << "TIME";
			break;
		case SQLTYPE_DATETIME:
			ofs << "DATETIME";
			break;
		}

		ofs << "</field_type>\n";
		if (rset->getInt(5) == SQLTYPE_STRING || rset->getInt(5) == SQLTYPE_VSTRING)
			ofs << prefix << "<field_size>" << rset->getInt(6) << "</field_size>\n";
		if (rset->getInt(7) == 1)
			ofs << prefix << "<is_primary>Y</is_primary>\n";
		if (rset->getInt(8) == 1)
			ofs << prefix << "<is_partition>Y</is_partition>\n";
		if (refresh_type & REFRESH_TYPE_FILE)
			ofs << prefix << "<load_size>" << rset->getInt(6) << "</load_size>\n";

		prefix.resize(prefix.size() - 1);
		ofs << prefix << "</field>\n";
	}
}

void load_data_info(Generic_Database *db, ofstream& ofs, ofstream& sdc_ofs)
{
	auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(db->create_data());
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		cout << (_("ERROR: Memory allocation failure")).str(SGLOCALE) << std::endl;
		exit(1);
	}

	data->setSQL("select table_id{int}, table_name{char[127]}, conditions{char[4000]}, segment_rows{long}, refresh_type{int} "
		"from dbc_data_info order by table_id;");
	stmt->bind(data);

	auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
	Generic_ResultSet *rset = auto_rset.get();

	ofs << prefix << "<tables>\n";
	prefix += '\t';

	sdc_ofs << "\t<tables>\n";

	while (rset->next()) {
		ofs << prefix << "<table>\n";
		prefix += '\t';

		ofs << prefix << "<table_id>" << rset->getInt(1) << "</table_id>\n"
			<< prefix << "<table_name>" << rset->getString(2) << "</table_name>\n";

		string conditions = rset->getString(3);
		if (!conditions.empty())
			ofs << prefix << "<conditions>" << conditions << "</conditions>\n";

		ofs << prefix << "<segment_rows>" << rset->getLong(4) << "</segment_rows>\n"
			<< prefix << "<options>";

		bool first = true;
		int refresh_type = rset->getInt(5);
		if (refresh_type & REFRESH_TYPE_DB) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "DB";
		}
		if (refresh_type & REFRESH_TYPE_FILE) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "FILE";
		}
		if (refresh_type & REFRESH_TYPE_BFILE) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "BFILE";
		}
		if (refresh_type & REFRESH_TYPE_INCREMENT) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "INCREMENT";
		}
		if (refresh_type & REFRESH_TYPE_IN_MEM) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "IN_MEM";
		}
		if (refresh_type & REFRESH_TYPE_NO_LOAD) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "NO_LOAD";
		}

		ofs << " DISCARD";

		ofs << "</options>\n"
			<< prefix << "<fields>\n";
		prefix += '\t';

		load_data_detail(refresh_type, db, rset->getInt(1), ofs);

		prefix.resize(prefix.size() - 1);
		ofs << prefix << "</fields>\n";

		prefix.resize(prefix.size() - 1);
		ofs << prefix << "</table>\n";

		sdc_ofs << "\t\t<table>\n";

		sdc_ofs << "\t\t\t<table_name>" << rset->getString(2) << "</table_name>\n"
			<< "\t\t\t<hostid>${SGHOSTS}</hostid>\n"
			<< "\t\t\t<redundancy>1</redundancy>\n";

		sdc_ofs << "\t\t</table>\n";
	}

	prefix.resize(prefix.size() - 1);
	ofs << prefix << "</tables>\n";

	sdc_ofs << "\t</tables>\n";
}

void load_index_detail(Generic_Database *db, int table_id, int index_id, ofstream& ofs)
{
	auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(db->create_data());
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		cout << (_("ERROR: Memory allocation failure")).str(SGLOCALE) << std::endl;
		exit(1);
	}

	data->setSQL("select field_name{char[127]}, is_hash{int}, hash_format{char[255]}, upper(search_type){char[255]}, "
		"special_type{int}, range_group{int} "
		"from dbc_index_detail where table_id = :table_id{int} and index_id = :index_id{int}");
	stmt->bind(data);

	stmt->setInt(1, table_id);
	stmt->setInt(2, index_id);

	auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
	Generic_ResultSet *rset = auto_rset.get();

	while (rset->next()) {
		ofs << prefix << "<field>\n";
		prefix += '\t';

		ofs << prefix << "<field_name>" << rset->getString(1) << "</field_name>\n";

		if (rset->getInt(2) == 1)
			ofs << prefix << "<is_hash>Y</is_hash>\n";

		if (!rset->getString(3).empty())
			ofs << prefix << "<hash_format>" << rset->getString(3) << "</hash_format>\n";

		string search_type = rset->getString(4);
		BOOST_FOREACH(char& v, search_type) {
			if (v == '|')
				v = ' ';
		}
		if (!search_type.empty())
			ofs << prefix << "<search_type>" << search_type << "</search_type>\n";

		if (rset->getInt(5) != 0)
			ofs << prefix << "<special_type>" << rset->getInt(5) << "</special_type>\n";

		if (rset->getInt(6) != 0)
			ofs << prefix << "<range_group>" << rset->getInt(6) << "</range_group>\n";

		prefix.resize(prefix.size() - 1);
		ofs << prefix << "</field>\n";
	}
}

void load_index_info(Generic_Database *db, ofstream& ofs)
{
	auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(db->create_data());
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		cout << (_("ERROR: Memory allocation failure")).str(SGLOCALE) << std::endl;
		exit(1);
	}

	data->setSQL("select table_id{int}, index_id{int}, index_name{char[127]}, index_type{int}, "
		"method_type{int}, buckets{long}, segment_rows{long} "
		"from dbc_index_info order by table_id, index_id;");
	stmt->bind(data);

	auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
	Generic_ResultSet *rset = auto_rset.get();

	ofs << prefix << "<indexes>\n";
	prefix += '\t';

	while (rset->next()) {
		ofs << prefix << "<index>\n";
		prefix += '\t';

		ofs << prefix << "<table_id>" << rset->getInt(1) << "</table_id>\n"
			<< prefix << "<index_id>" << rset->getInt(2) << "</index_id>\n"
			<< prefix << "<index_name>" << rset->getString(3) << "</index_name>\n"
			<< prefix << "<index_type>";

		bool first = true;
		int index_type = rset->getInt(4);
		if (index_type & INDEX_TYPE_PRIMARY) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "PRIMARY";
		}
		if (index_type & INDEX_TYPE_UNIQUE) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "UNIQUE";
		}
		if (index_type & INDEX_TYPE_NORMAL) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "NORMAL";
		}
		if (index_type & INDEX_TYPE_RANGE) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "RANGE";
		}

		ofs << "</index_type>\n"
			<< prefix << "<method_type>";

		first = true;
		int method_type = rset->getInt(5);
		if (method_type & METHOD_TYPE_SEQ) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "SEQ";
		}
		if (method_type & METHOD_TYPE_BINARY) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "BINARY";
		}
		if (method_type & METHOD_TYPE_HASH) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "HASH";
		}
		if (method_type & METHOD_TYPE_STRING) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "STRING";
		}
		if (method_type & METHOD_TYPE_NUMBER) {
			if (!first)
				ofs << " ";
			else
				first = false;
			ofs << "NUMBER";
		}

		ofs << "</method_type>\n"
			<< prefix << "<buckets>" << rset->getInt(6) << "</buckets>\n"
			<< prefix << "<segment_rows>" << rset->getInt(7) << "</segment_rows>\n"
			<< prefix << "<fields>\n";
		prefix += '\t';

		load_index_detail(db, rset->getInt(1), rset->getInt(2), ofs);

		prefix.resize(prefix.size() - 1);
		ofs << prefix << "</fields>\n";

		prefix.resize(prefix.size() - 1);
		ofs << prefix << "</index>\n";
	}

	prefix.resize(prefix.size() - 1);
	ofs << prefix << "</indexes>\n";
}

void load_func_info(Generic_Database *db, ofstream& ofs)
{
	auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(db->create_data());
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		cout << (_("ERROR: Memory allocation failure")).str(SGLOCALE) << std::endl;
		exit(1);
	}

	data->setSQL("select table_id{int}, index_id{int}, field_name{char[127]}, nullable{int}, "
		"field_ref{char[127]} from dbc_func_info order by table_id, index_id, field_seq;");
	stmt->bind(data);

	auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
	Generic_ResultSet *rset = auto_rset.get();
	int table_id = -1;
	int index_id = -1;

	ofs << prefix << "<functions>\n";
	prefix += '\t';

	while (rset->next()) {
		if (table_id != rset->getInt(1) || index_id != rset->getInt(2)) {
			if (table_id != -1) {
				prefix.resize(prefix.size() - 1);
				ofs << prefix << "</fields>\n";

				prefix.resize(prefix.size() - 1);
				ofs << prefix << "</function>\n";
			}

			table_id = rset->getInt(1);
			index_id = rset->getInt(2);
			ofs << prefix << "<function>\n";
			prefix += '\t';

			ofs << prefix << "<table_id>" << table_id << "</table_id>\n"
				<< prefix << "<index_id>" << index_id << "</index_id>\n"
				<< prefix << "<fields>\n";
			prefix += '\t';
		}

		ofs << prefix << "<field>\n";
		prefix += '\t';

		ofs << prefix << "<field_name>" << rset->getString(3) << "</field_name>\n";
		if (rset->getInt(4) == 1)
			ofs << prefix << "<nullable>Y</nullable>\n";

		if (!rset->getString(5).empty())
			ofs << prefix << "<field_ref>" << rset->getString(5) << "</field_ref>\n";

		prefix.resize(prefix.size() - 1);
		ofs << prefix << "</field>\n";
	}

	if (table_id != -1) {
		prefix.resize(prefix.size() - 1);
		ofs << prefix << "</fields>\n";

		prefix.resize(prefix.size() - 1);
		ofs << prefix << "</function>\n";
	}

	prefix.resize(prefix.size() - 1);
	ofs << prefix << "</functions>\n";
}

int main(int argc, char **argv)
{
	string user_name;
	string password;
	string connect_string;
	string dbc_file;
	string sdc_file;

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("user_name,u", po::value<string>(&user_name)->required(), (_("set database user name")).str(SGLOCALE).c_str())
		("password,p", po::value<string>(&password)->required(), (_("set database password")).str(SGLOCALE).c_str())
		("connect_string,c", po::value<string>(&connect_string)->required(), (_("set database connect string")).str(SGLOCALE).c_str())
		("dbc,d", po::value<string>(&dbc_file)->default_value("dbc.xml"), (_("set dbc XML file")).str(SGLOCALE).c_str())
		("sdc,s", po::value<string>(&sdc_file)->default_value("sdc.xml"), (_("set sdc XML file")).str(SGLOCALE).c_str())
	;

	try {
		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			exit(1);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		exit(1);
	}

	try {
		map<string, string> conn_info;

		conn_info["user_name"] = user_name;
		conn_info["password"] = password;
		conn_info["connect_string"] = connect_string;

		ofstream ofs(dbc_file.c_str());
		ofstream sdc_ofs(sdc_file.c_str());

		database_factory& factory_mgr = database_factory::instance();
		auto_ptr<Generic_Database> auto_db(factory_mgr.create("ORACLE"));
		Generic_Database *db = auto_db.get();
		db->connect(conn_info);

		ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			<< "<dbc xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"dbc.xsd\">\n"
			<< "\t<resource>\n"
			<< "\t\t<ipckey>43567</ipckey>\n"
			<< "\t\t<uid>" << getuid() << "</uid>\n"
			<< "\t\t<gid>" << getgid() << "</gid>\n"
			<< "\t\t<perm>0660</perm>\n"
			<< "\t\t<polltime>60</polltime>\n"
			<< "\t\t<robustint>10</robustint>\n"
			<< "\t\t<stacksize>33554432</stacksize>\n"
			<< "\t\t<statlevel>0</statlevel>\n"
			<< "\t\t<sysdir>${DBCDATA}/sys</sysdir>\n"
			<< "\t\t<syncdir>${DBCDATA}/sync</syncdir>\n"
			<< "\t\t<datadir>${DBCDATA}/dat</datadir>\n"
			<< "\t\t<shmcount>10</shmcount>\n"
			<< "\t\t<reservecount>10</reservecount>\n"
			<< "\t\t<postsigno>14</postsigno>\n"
			<< "\t\t<shmsize>1073741824</shmsize>\n"
			<< "\t\t<maxusers>100</maxusers>\n"
			<< "\t\t<maxtes>100</maxtes>\n"
			<< "\t\t<maxses>100</maxses>\n"
			<< "\t\t<maxclts>1000</maxclts>\n"
			<< "\t\t<sebkts>100</sebkts>\n"
			<< "\t\t<segredos>1000000</segredos>\n"
			<< "\t\t<libs>libsqloracle.so</libs>\n"
			<< "\t</resource>\n"
			<< "\t<users>\n"
			<< "\t\t<user>\n"
			<< "\t\t\t<usrname>admin</usrname>\n"
			<< "\t\t\t<password>ba1f2511fc30423bdbb183fe33f3dd0f</password>\n"
			<< "\t\t\t<dbname>ORACLE</dbname>\n"
			<< "\t\t\t<openinfo>${OPENINFO}</openinfo>\n"
			<< "\t\t\t<perm>INSERT UPDATE DELETE SELECT TRUNCATE RELOAD</perm>\n"
			<< "\t\t</user>\n"
			<< "\t</users>\n";

		prefix += '\t';

		sdc_ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			<< "<sdc xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"sdc.xsd\">\n";

		load_data_info(db, ofs, sdc_ofs);
		load_index_info(db, ofs);
		load_func_info(db, ofs);

		prefix.resize(prefix.size() - 1);
		ofs << "</dbc>";

		sdc_ofs << "</sdc>";

		return 0;
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		return 1;
	}
}

