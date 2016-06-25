/*
 * TGWSClient.cpp
 *
 *  Created on: 2012-3-28
 *      Author: Administrator
 */
#include "machine.h"
#include "monitorpch.h"
#include "boost/token_functions.hpp"
#include "boost/tokenizer.hpp"
#include "boost/shared_ptr.hpp"
#include <protocol/TProtocol.h>
#include <transport/TSocket.h>
#include <transport/TTransport.h>
#include <protocol/TBinaryProtocol.h>
#include <transport/TBufferTransports.h>
#include <Monitor.h>

namespace ai {
namespace sg {
using namespace apache::thrift;
using namespace apache::thrift::transport;
using namespace apache::thrift::protocol;
using namespace std;
class TGWSClient {
public:
	typedef  boost::tokenizer<boost::char_separator<char> > tokenizer;
	TGWSClient(const char * ip, int port);
	~TGWSClient();
	void init();
	void run();
private:
	int connect();
	int close();
	const char * ip;
	int port;
	boost::shared_ptr<TTransport> transport;
	boost::shared_ptr<MonitorIf> client;
};
TGWSClient::TGWSClient(const char * ip, int port) :
		ip(ip), port(port) {

}

TGWSClient::~TGWSClient() {
	close();
	// TODO Auto-generated destructor stub
}
void TGWSClient::init() {
	connect();
}
int TGWSClient::connect() {
	try {
		boost::shared_ptr<TSocket> socket(new TSocket(ip, port));
		transport.reset(new TFramedTransport(socket));
		boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
		client.reset(new MonitorClient(protocol));
		transport->open();
	} catch (...) {
		return -1;
	}
	return 0;
}
int TGWSClient::close() {
	if (transport.get()) {
		transport->close();
	}
	return 0;
}
void TGWSClient::run() {
	string cmd;
	boost::char_separator<char> sep(" \t\b");
	cout.clear();
	cerr.clear();
	cout << "> " << flush;
	while (std::getline(cin, cmd)) {
		cout << "> " << flush;
		vector<string> token_vector;
		tokenizer tokens(cmd, sep);
		for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end();
				++iter)
			token_vector.push_back(*iter);
		if (token_vector.empty()) {
			continue;
		}
		string funcname = token_vector[0];
		if ("all" == funcname) {
			AllInfo _return;
			int times = 1;
			if (token_vector.size() > 1) {
				times = atoi(token_vector[1].c_str());
			}
			for (int i = 0; i < times; i++) {
				client->getAllInfo(_return, -1);
				std::cout << (_("called ")).str(SGLOCALE) << i << endl;;
			}
		}else if ("quit" == funcname) {
			return;
		}else {
			continue;
		}
	}
}
}
}
int main() {
	using namespace ai::sg;
	TGWSClient client("localhost",40001);
	client.init();
	client.run();

}
