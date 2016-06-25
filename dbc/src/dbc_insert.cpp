#include "dbc_insert.h"
#include "sql_control.h"

namespace po = boost::program_options;

using namespace std;
using namespace ai::scci;

namespace ai
{
namespace sg
{

dbc_insert::dbc_insert()
{
	db = NULL;
	select_stmt = NULL;
	insert_stmt = NULL;
	rset = NULL;

	select_data = NULL;
	insert_data = NULL;
}

dbc_insert::~dbc_insert()
{
	delete insert_data;
	delete select_data;

	if (rset)
		select_stmt->closeResultSet(rset);

	if (db) {
		db->terminate_statement(insert_stmt);
		db->terminate_statement(select_stmt);
		delete db;
	}
}

int dbc_insert::run(int argc, char **argv)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(800, __PRETTY_FUNCTION__, "", &retval);
#endif
	bool background = false;

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("version,v", (_("print current dbc_insert version")).str(SGLOCALE).c_str())
		("background,b", (_("run dbc_insert in background mode")).str(SGLOCALE).c_str())
		("table,t", po::value<string>(&target_table), (_("target table name")).str(SGLOCALE).c_str())
		("sql,s", po::value<string>(&select_sql), (_("select SQL statement to execute")).str(SGLOCALE).c_str())
		("username,u", po::value<string>(&user_name), (_("specify DBC user name")).str(SGLOCALE).c_str())
		("password,p", po::value<string>(&password), (_("specify DBC password")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			::exit(0);
		} else if (vm.count("version")) {
			std::cout << (_("dbc_insert version ")).str(SGLOCALE) << DBC_RELEASE << std::endl;
			std::cout << (_("Compiled on ")).str(SGLOCALE) << __DATE__ << " " << __TIME__ << std::endl;
			::exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		::exit(1);
	}

	if (vm.count("background"))
		background = true;

	if (background) {
		sys_func& sys_mgr = sys_func::instance();
		sys_mgr.background();
	}

	// 设置可执行文件名称
	GPP->set_procname(argv[0]);

	retval = execute();
	return retval;
}

int dbc_insert::execute()
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(800, __PRETTY_FUNCTION__, "", &retval);
#endif
	map<string, string> conn_info;

	conn_info["user_name"] = user_name;
	conn_info["password"] = password;

	db = new Dbc_Database();
	db->connect(conn_info);

	select_stmt = db->create_statement();
	insert_stmt = db->create_statement();

	select_data = db->create_data();
	insert_data = db->create_data();

	select_data->setSQL(select_sql);
	select_stmt->bind(select_data);

	string insert_sql = "insert into ";
	insert_sql += target_table;
	insert_sql += " values(";

	bool first = true;
	bind_field_t *fields = dynamic_cast<struct_base *>(select_data)->getSelectFields();
	for (bind_field_t *field = fields; field->field_type != SQLTYPE_UNKNOWN; field++) {
		if (first)
			first = false;
		else
			insert_sql += ", ";

		insert_sql += ":";
		insert_sql += field->field_name;
		insert_sql += "{";

		switch (field->field_type) {
		case SQLTYPE_CHAR:
			insert_sql += "char";
			break;
		case SQLTYPE_UCHAR:
			insert_sql += "uchar";
			break;
		case SQLTYPE_SHORT:
			insert_sql += "short";
			break;
		case SQLTYPE_USHORT:
			insert_sql += "ushort";
			break;
		case SQLTYPE_INT:
			insert_sql += "int";
			break;
		case SQLTYPE_UINT:
			insert_sql += "uint";
			break;
		case SQLTYPE_LONG:
			insert_sql += "long";
			break;
		case SQLTYPE_ULONG:
			insert_sql += "ulong";
			break;
		case SQLTYPE_FLOAT:
			insert_sql += "float";
			break;
		case SQLTYPE_DOUBLE:
			insert_sql += "double";
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			insert_sql += "char[";
			insert_sql += boost::lexical_cast<string>(field->field_length - 1);
			insert_sql += "]";
			break;
		case SQLTYPE_DATE:
			insert_sql += "date";
			break;
		case SQLTYPE_TIME:
			insert_sql += "time";
			break;
		case SQLTYPE_DATETIME:
			insert_sql += "datetime";
			break;
		default:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid SQL given, missing type")).str(SGLOCALE));
			return retval;
		}

		insert_sql += "}";
	}

	insert_sql += ")";

	insert_data->setSQL(insert_sql);
	insert_stmt->bind(insert_data);

	int execute_count = 0;
	long total_count = 0;

	rset = select_stmt->executeQuery();
	while (rset->next()) {
		if (execute_count > 0)
			insert_stmt->addIteration();

		int idx = 1;
		for (bind_field_t *field = fields; field->field_type != SQLTYPE_UNKNOWN; field++, idx++) {
			switch (field->field_type) {
			case SQLTYPE_CHAR:
				insert_stmt->setChar(idx, rset->getChar(idx));
				break;
			case SQLTYPE_UCHAR:
				insert_stmt->setUChar(idx, rset->getUChar(idx));
				break;
			case SQLTYPE_SHORT:
				insert_stmt->setShort(idx, rset->getShort(idx));
				break;
			case SQLTYPE_USHORT:
				insert_stmt->setUShort(idx, rset->getUShort(idx));
				break;
			case SQLTYPE_INT:
				insert_stmt->setInt(idx, rset->getInt(idx));
				break;
			case SQLTYPE_UINT:
				insert_stmt->setUInt(idx, rset->getUInt(idx));
				break;
			case SQLTYPE_LONG:
				insert_stmt->setLong(idx, rset->getLong(idx));
				break;
			case SQLTYPE_ULONG:
				insert_stmt->setULong(idx, rset->getULong(idx));
				break;
			case SQLTYPE_FLOAT:
				insert_stmt->setFloat(idx, rset->getFloat(idx));
				break;
			case SQLTYPE_DOUBLE:
				insert_stmt->setDouble(idx, rset->getDouble(idx));
				break;
			case SQLTYPE_STRING:
			case SQLTYPE_VSTRING:
				insert_stmt->setString(idx, rset->getString(idx));
				break;
			case SQLTYPE_DATE:
				insert_stmt->setDate(idx, rset->getDate(idx));
				break;
			case SQLTYPE_TIME:
				insert_stmt->setTime(idx, rset->getTime(idx));
				break;
			case SQLTYPE_DATETIME:
				insert_stmt->setDatetime(idx, rset->getDatetime(idx));
				break;
			default:
				break;
			}
		}

		if (++execute_count == insert_data->size()) {
			insert_stmt->executeUpdate();
			total_count += execute_count;
			execute_count = 0;
			db->commit();
		}
	}

	if (execute_count > 0) {
		insert_stmt->executeUpdate();
		total_count += execute_count;
		db->commit();
	}

	select_stmt->closeResultSet(rset);

	GPP->write_log((_("INFO: {1} rows inserted successfully") % total_count).str(SGLOCALE));
	retval = 0;
	return retval;
}

}
}

using namespace ai::sg;

int main(int argc, char **argv)
{
	try {
		dbc_insert insert_mgr;
		return insert_mgr.run(argc, argv);
	} catch (exception& ex) {
		cout << ex.what();
		return -1;
	}
}

