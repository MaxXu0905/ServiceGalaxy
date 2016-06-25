#include "sg_build.h"

int main(int argc, char **argv)
{
	try {
		ai::sg::sg_build build_mgr;

		return build_mgr.run(argc, argv);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}
}

