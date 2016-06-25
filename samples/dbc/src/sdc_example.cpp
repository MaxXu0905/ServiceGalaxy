#include "sdc_api.h"
#include "datastruct_structs.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bc = boost::chrono;
using namespace ai::sg;

int const PER_HANDLES = 1;

int main(int argc, char **argv)
{
	int count;
	int idx;
	sg_api& api_mgr = sg_api::instance();
	api_mgr.set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE).c_str());
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("count,c", po::value<int>(&count)->default_value(1), (_("execution count")).str(SGLOCALE).c_str())
		("index,i", po::value<int>(&idx)->default_value(0), (_("channel_id index")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return 0;
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		return 1;
	}

	try {
		if (!api_mgr.init()) {
			std::cout << (_("ERROR: Failed to call init()\n")).str(SGLOCALE);
			return 1;
		}

		BOOST_SCOPE_EXIT((&api_mgr)) {
			if (!api_mgr.fini()) {
				std::cout << (_("ERROR: Failed to call fini()\n")).str(SGLOCALE);
				exit(1);
			}
		} BOOST_SCOPE_EXIT_END

		sdc_api& sdc_mgr = sdc_api::instance();

		const char *channel_ids[] = {
			"85a0188", "71a0566", "59a4129", "75a2817", "83a0715",
			"85a1181", "97a2512", "85a0982", "91a6466", "11a0905"
		};

		boost::timer timer;
		bc::system_clock::time_point start = bc::system_clock::now();

		t_tf_chl_channel key;
		t_tf_chl_channel result;
		strcpy(key.channel_id, channel_ids[idx]);

		for (int i = 0; i < count; i++) {
			if (sdc_mgr.find(T_TF_CHL_CHANNEL, 0, &key, &result) != 1) {
				std::cout << (_("ERROR: Can't find data in T_TF_CHL_CHANNEL - ")).str(SGLOCALE) << api_mgr.strerror() << std::endl;
				return 1;
			}
		}

		bc::system_clock::duration sec = bc::system_clock::now() - start;
		std::cout << (_("clocks = {1}, elapsed = {2}") % timer.elapsed() % sec.count()).str(SGLOCALE) << std::endl;

		return 0;
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}
}


