#if !defined(__TEST_MAIN_H__)
#define __TEST_MAIN_H__

#include "test_manager.h"
#include "test_thread.h"

namespace ai
{
namespace sg
{

class test_main
{
public:
	test_main();
	~test_main();

	bool run(int argc, char **argv);

private:
	bool verbose;
	int max_thread;
};

}
}

#endif

