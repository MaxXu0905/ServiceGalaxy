#include "sg_boot.h"

int main(int argc, char **argv)
{
	try {
		ai::sg::sg_boot boot_mgr;

		boot_mgr.init(argc, argv);
		boot_mgr.run();
		return 0;
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}
}

