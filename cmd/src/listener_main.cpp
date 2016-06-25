#include "listener.h"

using namespace ai::sg;

int main(int argc, char **argv)
{
	try {
		listener listener_mgr;

		return listener_mgr.run(argc, argv);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}
}

