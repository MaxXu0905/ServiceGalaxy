#if !defined(__TEST_THREAD_H__)
#define __TEST_THREAD_H__

#include "test_action.h"
#include "test_manager.h"
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

namespace ai
{
namespace sg
{

class test_thread
{
public:
	test_thread();
	~test_thread();

	void run(const std::string& user_name, const std::string& password);
	bool execute(const dbc_action_t& dbc_action);
	void stop();

private:
	const dbc_action_t *_dbc_action;
	bool stop_flag;
	boost::mutex mutex;
	boost::mutex execute_mutex;
	boost::condition cond;
	boost::condition execute_cond;
	bool status;
};

}
}

#endif

