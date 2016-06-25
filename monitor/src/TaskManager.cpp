/*
 * TaskManager.cpp
 *
 *  Created on: 2012-2-28
 *      Author: renyz
 */

#include "TaskManager.h"
namespace ai {
namespace sg {

TaskManager::TaskManager():max_size (5000),
	stopflag(false) {
	boost::thread(boost::bind(&TaskManager::taskProcess, this));
}

TaskManager::~TaskManager() {
	stop();
}

int32_t TaskManager::push(OperateTaskPtr task) {
	boost::mutex::scoped_lock lock(_mutex);
	if (_queue.size() >= max_size) {
		return 1;
	}
	_queue.push(task);
	_cond.notify_all();
	return 0;
}
void TaskManager::stop() {
	stopflag = true;
	_cond.notify_all();
}
void TaskManager::taskProcess() {
	while (!stopflag) {
		OperateTaskPtr task;
		{
			boost::mutex::scoped_lock lock(_mutex);
			while (_queue.empty()) {
				_cond.wait(lock);
				if(stopflag){
					return ;
				}
			}
			task = _queue.front();
			_queue.pop();
		}
		if(task.get()){
			task->run();
		}
	}
}
}
}
