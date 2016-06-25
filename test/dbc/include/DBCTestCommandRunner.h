/*
 * DBCTestCommandRunner.h
 *
 *  Created on: 2012-4-28
 *      Author: Administrator
 */

#ifndef DBCTESTCOMMANDRUNNER_H_
#define DBCTESTCOMMANDRUNNER_H_
#include "dbc_api.h"
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include "boost/bind.hpp"
namespace ai {
namespace test {
namespace dbc {
class ActionContext{
public:
	void init(const string & username,const string password){
		_dbc=&dbc_api::instance();
		_dbc->login();
		_dbc->connect(username,password);
	}
	dbc_api* dbc(){
		return _dbc;
	}
private:
	dbc_api* _dbc;
};
class Action{
public:
	enum CheckType{
		Expect,No,Assert
	};
	virtual ~Action(){}
	virtual int threadnum();
	virtual string stepdesc()=0;
	virtual string errordesc()=0;
	virtual CheckType checkType()=0;
	virtual bool checkResult()=0;
	virtual auto_ptr<ActionResult> doit(const ActionContext *ctx)=0;
};
class TreadExcutor{
	TreadExcutor(const string& username,const string& password):isstop(false),action(0),username(username),password(password){}
public:
	void run(){
		ActionContext context=ActionContext();
		context.init(username,password);
		while (!isstop) {
			boost::mutex::scoped_lock lock(mutex);
			while(!action&&!isstop)
				cond.wait(mutex);
			if(!isstop){
				action->doit(&context);
				action=0;
			}
			cond.notify_one();
		}
	}
	void stop(){
		boost::mutex::scoped_lock lock(mutex);
		isstop=true;
		cond.notify_one();
	}
	void run(Action * action){
		boost::mutex::scoped_lock lock(mutex);
		this->action=action;
		cond.notify_one();
		cond.wait(mutex);
	}
private:
	Action * action;
	bool isstop;
	boost::mutex mutex;
	boost::condition cond;
	const string username;
	const string password;

};

class ActionReader{
public:
	ActionReader(const string& actionfile);
	auto_ptr<Action> read();
};
class DBCTestCommandRunner {
public:
	DBCTestCommandRunner();
	virtual ~DBCTestCommandRunner();
	void init(int maxthread,const string username,const string &password);
	void run(Action * action);
	void stop();

};

} /* namespace dbc */
} /* namespace test */
} /* namespace ai */
#endif /* DBCTESTCOMMANDRUNNER_H_ */
