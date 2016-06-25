#include "test_thread.h"

namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

test_thread::test_thread()
{
	stop_flag = false;
	status = true;

	mutex.lock();
	execute_mutex.lock();
}

test_thread::~test_thread()
{
	mutex.unlock();
	execute_mutex.unlock();
}

bool test_thread::execute(const dbc_action_t& dbc_action)
{
	if (dbc_action.action_type != ACTION_TYPE_GET_STATUS) {
		_dbc_action = &dbc_action;
		mutex.lock();
		cond.notify_one();
		mutex.unlock();
	}

	bp::ptime expire(bp::second_clock::universal_time() + bp::seconds(1));
	bool retval = execute_cond.timed_wait(execute_mutex, expire);
	if (dbc_action.flags & ACTION_FLAG_TIMEDOUT) {
		if (!retval)
			return true;
		else
			return false;
	} else {
		if (!retval)
			return false;
		else
			return status;
	}
}

void test_thread::stop()
{
	stop_flag = true;
	mutex.lock();
	cond.notify_one();
	mutex.unlock();

	execute_cond.wait(execute_mutex);
}

void test_thread::run(const string& user_name, const string& password)
{
	test_manager test_mgr(user_name, password);

	while (1) {
		cond.wait(mutex);

		if (stop_flag)
			break;

		if (!test_mgr.execute(*_dbc_action)) {
			status = false;
			break;
		}

		execute_mutex.lock();
		execute_cond.notify_one();
		execute_mutex.unlock();
	}

	execute_mutex.lock();
	execute_cond.notify_one();
	execute_mutex.unlock();
}

}
}

