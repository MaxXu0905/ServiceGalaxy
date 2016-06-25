#include "shm_vector.h"
#include "shm_malloc.h"

using namespace ai::sg;
using namespace std;

static void usage(const string& cmd);

struct data_t
{
	int data1;
	char data2[10 + 1];
};

int main(int argc, char **argv)
{
	int c;
	int key = -1;
	bool clean = false;
	shm_link_t shm_link = shm_link_t::SHM_LINK_NULL;

	while ((c = ::getopt(argc, argv,"k:i:o:c")) != EOF) {
		switch (c) {
 		case 'k':
			key = ::atoi(optarg);
			break;
		case 'i':
			shm_link.block_index = ::atol(optarg);
			break;
		case 'o':
			shm_link.block_offset = ::atol(optarg);
			break;
		case 'c':
			clean = true;
			break;
		default:
			usage(argv[0]);
			::exit(1);
		}
	}

	if (optind < argc) {
		cerr << (_("ERROR: Invalid arguments passed to ")).str(SGLOCALE) << argv[0] << "\n";
		::exit(1);
	}

	if (key == -1) {
		usage(argv[0]);
		::exit(1);
	}

	try {
		shm_malloc shm_mgr;
		shm_mgr.attach(key, 0);

		shm_vector<data_t> instance(shm_mgr, shm_link);

		data_t item;
		::strcpy(item.data2, "data");

		for (int i = 0; i < 10; i++) {
			item.data1 = i;
			instance.push_back(item);
		}

		shm_link = instance.get_shm_link();
		cout << (_("shm_link = (")).str(SGLOCALE) << shm_link.block_index << ", " << shm_link.block_offset << ")\n";
		cout << (_("size = ")).str(SGLOCALE) << instance.size() << "\n";
		cout << (_("capacity = ")).str(SGLOCALE) << instance.capacity() << "\n";

		cout << "\n\n" << (_("Tree Status:\n")).str(SGLOCALE);
		for (shm_vector<data_t>::iterator iter = instance.begin(); iter != instance.end(); ++iter) {
			cout << "-------------------------------------------\n";
			cout << (_("data1 = ")).str(SGLOCALE) << iter->data1 << std::endl;
			cout << (_("data2 = ")).str(SGLOCALE) << iter->data2 << std::endl;
		}

		if (clean) {
			// Memory will be freed, after that, no operation on instance is permitted.
			instance.destroy();
		}
		return 0;
	} catch (exception& ex) {
		cout << (_("Error occurred in ")).str(SGLOCALE) << ex.what() << std::endl;
		return 1;
	}
}

static void usage(const string& cmd)
{
	cout << (_("usage: ")).str(SGLOCALE) << cmd << " -k ipc_key\n";
	::exit(1);
}
