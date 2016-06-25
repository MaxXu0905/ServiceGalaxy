#include "sg_shut.h"

int main(int argc, char **argv)
{
	try {
		ai::sg::sg_shut shut_mgr;

		shut_mgr.init(argc, argv);
		shut_mgr.run();
		return 0;
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}
}

