#include "test_main.h"

using namespace ai::sg;
using namespace std;
namespace po = boost::program_options;

test_main::test_main()
{
	verbose = false;
	max_thread = -1;
}

test_main::~test_main()
{
}

bool test_main::run(int argc, char **argv)
{
	string user_name;
	string password;
	int case_id;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "produce help message")
		("username,u", po::value<string>(&user_name)->default_value("system"), "specify DBC user name")
		("password,p", po::value<string>(&password)->default_value("system"), "specify DBC password")
		("case_id,i", po::value<int>(&case_id)->required(), "specify case id")
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			cout << "INFO: " << argv[0] << " exit normally in help mode.";
			return 0;
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		return 1;
	}

	if (case_id < 0 || case_id >= sizeof(test_plan) / sizeof(dbc_action_t *)) {
		cout << "ERROR: case_id out of range\n";
		::exit(1);
	}

	for (const dbc_action_t *dbc_action = test_plan[case_id]; dbc_action->action_type != ACTION_TYPE_END; ++dbc_action) {
		if (dbc_action->thread_id > max_thread)
			max_thread = dbc_action->thread_id;
	}

	test_manager test_mgr(user_name, password);
	test_thread *threads;
	bool result = true;
	boost::thread_group thread_group;

	if (max_thread >= 0) {
		threads = new test_thread[max_thread + 1];
		for (int i = 0; i <= max_thread; i++)
			thread_group.create_thread(boost::bind(&test_thread::run, &threads[i], user_name, password));
	} else {
		threads = NULL;
	}

	for (const dbc_action_t *current_action = test_plan[case_id]; current_action->action_type != ACTION_TYPE_END; ++current_action) {
		int thread_id = current_action->thread_id;
		if (thread_id == -1) {
			result = test_mgr.execute(*current_action);
			if (!result)
				break;
		} else {
			result = threads[thread_id].execute(*current_action);
			if (!result)
				break;
		}
	}

	for (int i = 0; i <= max_thread; i++)
		threads[i].stop();

	thread_group.join_all();

 	if (threads)
		delete []threads;

	return result;
}

int main(int argc, char **argv)
{
	try {
		test_main test_mgr;

		bool result = test_mgr.run(argc, argv);
		if (result)
			cout << "Test successfully\n";
		else
			cout << "Test failed\n";

		return 0;
	} catch (exception& ex) {
		cout << ex.what();
		return 1;
	}
}

