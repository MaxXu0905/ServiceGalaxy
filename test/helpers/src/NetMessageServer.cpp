/*
 * NetMessageServer.cpp
 *
 *  Created on: 2012-3-26
 *      Author: renyz
 */
#include <iostream>
#include <map>
#include "boost/shared_ptr.hpp"
//include <boost/thread.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/xtime.hpp>
#include "boost/bind.hpp"
#include "NetMessageServer.h"
#include "ServerManager.h"
#include "Message.h"
#include "Message_types.h"
#include "Message_constants.h"

namespace ai {
namespace test {
namespace {
boost::mutex _mutex;
boost::condition _cond;
map<string,bool> returned;
map<string,string> returnVal;
namespace{
boost::thread_group _thread_group;
}
class MessageHandler: virtual public MessageIf {
public:
	MessageHandler() {
		// Your initialization goes here
	}
	int32_t send(const std::string &key,const std::string& msg) {
		boost::mutex::scoped_lock lock(_mutex);
		returnVal[key] = msg;
		returned[key] = true;
		_cond.notify_all();
		return 0;
	}
	int32_t sendadd(const std::string &key,const std::string& msg) {
			boost::mutex::scoped_lock lock(_mutex);
			if(returned[key]){
				returnVal[key] =returnVal[key]+ msg;
			}else{
				returnVal[key]=msg;
				returned[key] = true;
			}
			_cond.notify_all();
			return 0;
		}
};
}
NetMessageServer::NetMessageServer() :
	serverManager(0) {
	// TODO Auto-generated constructor stub

}
NetMessageServer::~NetMessageServer() {
	_cond.notify_all();
	stop();
}
void NetMessageServer::clean(const string & key){
	returned[key]=0;
	returnVal[key]="";
}
void NetMessageServer::cleanALL(){
	returned.clear();
	returnVal.clear();
}

int NetMessageServer::recive(const string &key,string &out, int time) {
	boost::mutex::scoped_lock lock(_mutex);
	boost::xtime xt;
	boost::xtime_get(&xt, boost::TIME_UTC);
	xt.sec += time;
	if (!returned[key]) {
		_cond.timed_wait(lock, xt);
	}
	if (returned[key]) {
		out = returnVal[key];
		returned[key]=false;
		returnVal[key]="";
		return 0;
	}
	returned[key]=false;
	returnVal[key]="";
	return -1;

}
int NetMessageServer::run(int port) {
	if (serverManager != 0) {
		return -1;
	}
	boost::shared_ptr<MessageHandler> handler(new MessageHandler());
	boost::shared_ptr<MessageProcessor> messageProcessor(new MessageProcessor(
			handler));
	serverManager = new ServerManager(messageProcessor, port, "", "", "", 16);
	_thread_group.create_thread(boost::bind(&ServerManager::run, serverManager));
	return 0;
}

int NetMessageServer::stop() {
	if (serverManager) {
		try{
		serverManager->stop();
		_thread_group.join_all();

		}catch(exception e){
			_thread_group.join_all();
		}
		delete serverManager;
		serverManager=0;
		return 0;
	}
	return -1;
}
NetMessageServer & NetMessageServer::instance(){
	static NetMessageServer netMessageServer;
	return netMessageServer;
}
}
}
