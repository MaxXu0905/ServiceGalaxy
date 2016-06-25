#if !defined(__GW_SERVER_H__)
#define __GW_SERVER_H__

#include "sg_internal.h"
#include "gws_queue.h"
#include "gws_connection.h"
#include "gwp_ctx.h"

namespace basio = boost::asio;

namespace ai
{
namespace sg
{

class gws_server : public sg_svr
{
public:
	typedef boost::shared_ptr<gws_connection> pointer;

	static gws_server& instance(sgt_ctx *SGT);

	gws_server();
	~gws_server();

protected:
	int svrinit(int argc, char **argv);
	int svrfini();
	int run(int flags = 0);

private:
	void start_accept(basio::ip::tcp::acceptor& acceptor, bool tcpkeepalive, bool nodelay);
	bool start_connect(int local_mid);
	void handle_accept(basio::ip::tcp::acceptor& acceptor, pointer new_connection, bool tcpkeepalive, bool nodelay, const boost::system::error_code& error);
	void handle_timeout(const boost::system::error_code& error);
	bool get_address(int mid, std::set<gws_netent_t>& netents);

	static void * net_run(void *arg);

	gwp_ctx *GWP;
	std::vector<boost::shared_ptr<basio::ip::tcp::acceptor> > acceptors;
	basio::deadline_timer timer;
	bool timeout;
};

}
}

#endif

