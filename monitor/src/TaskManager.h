/*
 * TaskManager.h
 *	异部任务管理类
 *  Created on: 2012-2-28
 *      Author: renyz
 */

#ifndef TASKMANAGER_H_
#define TASKMANAGER_H_
#include "monitorpch.h"
namespace ai {
namespace sg {
class OperateTask {
public:
	virtual ~OperateTask(){};
	virtual void run()=0;
};

typedef boost::shared_ptr<OperateTask> OperateTaskPtr;

class TaskManager {
public:
	const int max_size;
	virtual ~TaskManager();
	int32_t push(OperateTaskPtr task);
	void taskProcess();
	void stop();
	static TaskManager& instance() {
		static TaskManager manager;
		return manager;
	}
private:
	TaskManager();
	boost::mutex _mutex;
	boost::condition _cond;

	bool stopflag;
	std::queue<OperateTaskPtr> _queue;
};
}
}
#endif /* SERVEROPERATETASKMANAGER_H_ */
