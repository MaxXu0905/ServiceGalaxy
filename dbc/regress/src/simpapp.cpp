#include "dbc_api.h"
#include "oracle_control.h"
#include "sql_control.h"
#include "datastruct_structs.h"
#include "search_h.inl"

using namespace ai::sg;
using namespace std;
namespace po = boost::program_options;

void print_error(const string& filename, int line)
{
	cout << (boost::format("ERROR: at %1%:%2%, unexpected result occurs\n") %filename % line).str();
	exit(1);
}

int main(int argc, char **argv)
{
	string user_name;
	string password;

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("username,u", po::value<string>(&user_name)->default_value("system"), (_("specify DBC user name")).str(SGLOCALE).c_str())
		("password,p", po::value<string>(&password)->default_value("system"), (_("specify DBC password")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			cout << (boost::format("INFO: %1% exit normally in help mode.") % argv[0]).str();
			return 0;
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		return 1;
	}

	try {
		dbc_api& api_mgr = dbc_api::instance();
		char default_region_code[2];

		if (!api_mgr.login()) {
			cout << (boost::format("ERROR: login() failed.\n")).str();
			return 1;
		}

		if (!api_mgr.connect(user_name, password)) {
			cout << (boost::format("ERROR: connect() failed.\n")).str();
			return 1;
		}

		BOOST_SCOPE_EXIT((&api_mgr)) {
			api_mgr.logout();
		} BOOST_SCOPE_EXIT_END

		t_bds_area_code key;
		t_bds_area_code result;
		strcpy(key.area_code, "00244");
		key.eff_date = 1199116800;

		if (api_mgr.find(1, &key, &result) == 0)
			print_error(__FILE__, __LINE__);

		if (result.default_region_code != '5')
			print_error(__FILE__, __LINE__);

		timeval begin_time;
		timeval end_time;
		gettimeofday(&begin_time, NULL);

		for (int i = 0; i < 10000000; i++) {
			if (get_bds_area_code(&api_mgr, key.area_code, key.eff_date, default_region_code))
				print_error(__FILE__, __LINE__);
		}

		gettimeofday(&end_time, NULL);
		double t = (end_time.tv_sec - begin_time.tv_sec) + (end_time.tv_usec - begin_time.tv_usec) / 1000000.0;
		cout << (boost::format("time elapsed = ")).str() << t << endl;

		if (strcmp(default_region_code, "5") != 0)
			print_error(__FILE__, __LINE__);

		if (api_mgr.find(1, 0, &key, &result) == 0)
			print_error(__FILE__, __LINE__);

		if (result.default_region_code != '5')
			print_error(__FILE__, __LINE__);

		auto_ptr<dbc_cursor> cursor = api_mgr.find(1, &key);
		if (cursor->next()) {
			if (!api_mgr.erase_by_rowid(1, cursor->rowid()))
				print_error(__FILE__, __LINE__);
		}

		if (api_mgr.find(1, 0, &key, &result) != 0)
			print_error(__FILE__, __LINE__);

		api_mgr.commit();

		cout << argv[0] << (boost::format(" executed successfully\n")).str();
		return 0;
	} catch (exception& ex) {
		cout << ex.what();
		return 1;
	}
}

