#if !defined(__LISTENER_H__)
#define __LISTENER_H__

#include "sg_internal.h"
#include "listener_connection.h"

namespace basio = boost::asio;

namespace ai
{
namespace sg
{

class listener
{
public:
	listener();
	~listener();

	int run(int argc, char **argv);

private:
	void start_accept();
	void handle_accept(listener_connection::pointer new_connection, const boost::system::error_code& error);
	bool get_address(const string& nlsaddr, string& ipaddr, string& port);

	static void sigterm(int signo);

	gpp_ctx *GPP;
	basio::io_service io_svc;
	basio::ip::tcp::acceptor acceptor;

	static bool started;
};

}
}

#endif

