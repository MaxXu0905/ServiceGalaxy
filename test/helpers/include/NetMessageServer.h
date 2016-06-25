/*
 * NetMessageServer.h
 *
 *  Created on: 2012-3-26
 *      Author: renyz
 */

#ifndef NETMESSAGESERVER_H_
#define NETMESSAGESERVER_H_
#include <string>
namespace ai {
namespace test {
using namespace std;
class ServerManager;
class NetMessageServer {
public:
	typedef void (*callback)();
	virtual ~NetMessageServer();
	int run(int port);
	int stop();
	void clean(const string & key);
	void cleanALL();
	int recive(const string & key,string &out,int time= 60);
	static NetMessageServer &instance();
private:
	NetMessageServer();
	ServerManager* serverManager;

};
}
}
#endif /* NETMESSAGESERVER_H_ */
