#include "sg_public.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bc = boost::chrono;
using namespace ai::sg;

void run(bool synchronized, int count, int handles, const string& svc_name, int& retval)
{
	sg_api& api_mgr = sg_api::instance();
	sg_init_t init_info;

	init_info.flags |= SGMULTICONTEXTS;
	try {
		if (!api_mgr.init(&init_info)) {
			std::cout << (_("ERROR: Failed to call init() - ")).str(SGLOCALE) << api_mgr.strerror() << std::endl;
			retval = 1;
			return;
		}

		boost::shared_array<message_pointer> auto_send_msg(new message_pointer[handles]);
		boost::shared_array<message_pointer> auto_recv_msg(new message_pointer[handles]);
		boost::shared_array<int> auto_handle(new int[handles]);
		message_pointer *send_msg = auto_send_msg.get();
		message_pointer *recv_msg = auto_recv_msg.get();
		int *handle = auto_handle.get();

		for (int i = 0; i < handles; i++) {
			send_msg[i] = sg_message::create();
			recv_msg[i] = sg_message::create();
		}

		boost::timer timer;
		bc::system_clock::time_point start = bc::system_clock::now();

		if (synchronized) {
			for (int i = 0; i < count; i++) {
				char buff[32];
				strcpy(buff, "example");
				send_msg[0]->assign(buff, sizeof(buff));

				if (!api_mgr.call(svc_name.c_str(), send_msg[0], recv_msg[0], 0)) {
					std::cout << (_("ERROR: Call operation {1} failed - ")% svc_name).str(SGLOCALE) << api_mgr.strerror() << std::endl;
					retval = 1;
					return;
				}
			}
		} else {
			for (int i = 0; i < count; i++) {
				for (int j = 0; j < handles; j++) {
					char buff[32];
					strcpy(buff, "example");
					send_msg[j]->assign(buff, sizeof(buff));
					handle[j] = api_mgr.acall(svc_name.c_str(), send_msg[j], 0);
				}
				for (int j = 0; j < handles; j++) {
					if (!api_mgr.getrply(handle[j], recv_msg[j], 0)) {
						std::cout << (_("getrply failed for operation {1} - ") % svc_name).str(SGLOCALE) << api_mgr.strerror() << std::endl;
						retval = 1;
						return;
					}
				}
			}
		}
		bc::system_clock::duration sec = bc::system_clock::now() - start;
		std::cout << (_("clocks = {1}, elapsed = {2}")% timer.elapsed() % (sec.count() / 1000000000.0)).str(SGLOCALE) << std::endl;

		if (!api_mgr.fini()) {
			std::cout << (_("ERROR: Failed to call fini() - ")).str(SGLOCALE) << api_mgr.strerror() << std::endl;
			retval = 1;
			return;
		}

		retval = 0;
		return;
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		retval = 1;
		return;
	}
}

int main(int argc, char **argv)
{
	bool synchronized;
	int count;
	int threads;
	int handles;
	string svc_name;

	sg_api& api_mgr = sg_api::instance();
	api_mgr.set_procname(argv[0]);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("count,c", po::value<int>(&count)->default_value(1), (_("execution count")).str(SGLOCALE).c_str())
		("sync,S", (_("execute in sync/async mode")).str(SGLOCALE).c_str())
		("threads,t", po::value<int>(&threads)->default_value(1), (_("execution threads")).str(SGLOCALE).c_str())
		("handles,h", po::value<int>(&handles)->default_value(1), (_("how many handles to run in parallel")).str(SGLOCALE).c_str())
		("service,s", po::value<string>(&svc_name)->default_value("SERVICE"), (_("specify operation name")).str(SGLOCALE).c_str())
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

	if (vm.count("sync"))
		synchronized = true;
	else
		synchronized = false;

	if (threads <= 0) {
		int retval;
		run(synchronized, count, handles, svc_name, retval);
		return retval;
	}

	int i;
	boost::thread_group thread_group;
	boost::shared_array<int> retvals(new int[threads]);
	memset(retvals.get(), 0, sizeof(int) * threads);

	for (i = 0; i < threads; i++)
		thread_group.create_thread(boost::bind(run, synchronized, count, handles, svc_name, boost::ref(retvals[i])));

	thread_group.join_all();

	for (i = 0; i < threads; i++) {
		if (retvals[i] != 0)
			return retvals[i];
	}

	return 0;
}

