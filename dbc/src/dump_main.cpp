#include "dbc_dump.h"
#include "dbc_internal.h"

namespace po = boost::program_options;
namespace bf = boost::filesystem;
using namespace ai::sg;

int main(int argc, char **argv)
{
	bool background = false;
	int table_id;
	string table_name;
	int parallel;
	long commit_size;
	string user_name;
	string password;

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("version,v", (_("print current dbc_dump version")).str(SGLOCALE).c_str())
		("table_id,t", po::value<int>(&table_id)->default_value(-1), (_("specify table_id to dump")).str(SGLOCALE).c_str())
		("table_name,T", po::value<string>(&table_name)->default_value(""), (_("specify table_name to dump")).str(SGLOCALE).c_str())
		("parallel,P",  po::value<int>(&parallel)->default_value(1), (_("specify parallel threads to dump")).str(SGLOCALE).c_str())
		("commit_size,c", po::value<long>(&commit_size)->default_value(0), (_("specify rows to commit")).str(SGLOCALE).c_str())
		("background,b", (_("run dbc_sync in background mode")).str(SGLOCALE).c_str())
		("username,u", po::value<string>(&user_name)->default_value("system"), (_("specify DBC user name")).str(SGLOCALE).c_str())
		("password,p", po::value<string>(&password)->default_value("system"), (_("specify DBC password")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			::exit(0);
		} else if (vm.count("version")) {
			std::cout << (_("dbc_dump version ")).str(SGLOCALE) << DBC_RELEASE << std::endl;
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

	if (table_id != -1 && !table_name.empty()) {
		cerr << (_("ERROR: Can't specify -t and -T options together.\n")).str(SGLOCALE);
		::exit(1);
	}

	if (table_id == -1 && table_name.empty()) {
		cerr << (_("ERROR: must specify -t or -T option.\n")).str(SGLOCALE);
		::exit(1);
	}

	if (background) {
		sys_func& sys_mgr = sys_func::instance();
		sys_mgr.background();
	}

	// 设置可执行文件名称
	gpp_ctx *GPP = gpp_ctx::instance();
	GPP->set_procname(argv[0]);

	try {
		dbc_dump dump_mgr(table_id, table_name, parallel, commit_size, user_name, password);

		long rows = dump_mgr.run();
		if (rows == -1) {
			cout << (_("ERROR: dbc_dump failed, please see LOG for more information.\n")).str(SGLOCALE);
			return -1;
		}

		if (rows > 1)
			cout << rows << (_(" rows dumped to database successfully.\n")).str(SGLOCALE);
		else
			cout << rows << (_(" row dumped to database successfully.\n")).str(SGLOCALE);
		return 0;
	} catch (exception& ex) {
		cout << ex.what();
		return -1;
	}
}

