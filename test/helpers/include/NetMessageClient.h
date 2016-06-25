/*
 * NetMessageClient.h
 *
 *  Created on: 2012-3-26
 *      Author: renyz
 */

#ifndef NETMESSAGECLIENT_H_
#define NETMESSAGECLIENT_H_
#include <string>
namespace ai {
namespace test {
using namespace std;
class MessageIf;
class NetMessageClient {
public:
	NetMessageClient(char * host,int port);
	NetMessageClient();
	virtual ~NetMessageClient();
	int send(const string &key,const string &msg);
	int sendadd(const string &key,const string &msg);
	int connect();
	int close();
private:
	MessageIf * messageClient;
	void * transport;


};
}
}
#endif /* NETMESSAGECLIENT_H_ */
