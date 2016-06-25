#include "shm_map.h"
#include "shm_malloc.h"

using namespace ai::sg;
using namespace std;

static void usage(const string& cmd);

struct key_data_t
{
	int key1;
	char key2[10 + 1];

	bool operator<(const key_data_t& rhs) const {
		if (key1 < rhs.key1)
			return true;
		else if (key1 > rhs.key1)
			return false;

		return ::strcmp(key2, rhs.key2) < 0;
	}
};

struct value_data_t
{
	int value1;
	long value2;
	char value3[20 + 1];
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

		shm_map<key_data_t, value_data_t> instance(shm_mgr, shm_link);

		key_data_t key_item;
		::strcpy(key_item.key2, "First");

		value_data_t value_item;
		value_item.value2 = 2;
		::strcpy(value_item.value3, "Second");

		for (int i = 0; i < 10; i++) {
			key_item.key1 = i;
			value_item.value1 = i;
			instance[key_item] = value_item;
		}

		shm_link = instance.get_shm_link();
		cout << (_("shm_link = (")).str(SGLOCALE) << shm_link.block_index << ", " << shm_link.block_offset << ")\n";
		cout << (_("size = ")).str(SGLOCALE) << instance.size() << "\n";
		cout << (_("capacity = ")).str(SGLOCALE) << instance.capacity() << "\n";

		key_item.key1 = 5;
		shm_map<key_data_t, value_data_t>::iterator iter = instance.find(key_item);
		if (iter == instance.end()) {
			cerr << (_("ERROR: No data found.\n")).str(SGLOCALE);
			::exit(1);
		}

		cout << (_("value1 = ")).str(SGLOCALE) << iter->second.value1 << std::endl;
		cout << (_("value2 = ")).str(SGLOCALE) << iter->second.value2 << std::endl;
		cout << (_("value3 = ")).str(SGLOCALE) << iter->second.value3 << std::endl;

		cout << "\n\n" << (_("Tree Status:\n")).str(SGLOCALE);
		for (iter = instance.begin(); iter != instance.end(); ++iter) {
			cout << "-------------------------------------------\n";
			cout << (_("value1 = ")).str(SGLOCALE) << iter->second.value1 << std::endl;
			cout << (_("value2 = ")).str(SGLOCALE) << iter->second.value2 << std::endl;
			cout << (_("value3 = ")).str(SGLOCALE) << iter->second.value3 << std::endl;
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
