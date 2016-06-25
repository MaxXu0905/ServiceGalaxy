#if !defined(__GWS_CONNECTION_H__)
#define __GWS_CONNECTION_H__

#include "sg_internal.h"
#include "gwp_ctx.h"
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

namespace basio = boost::asio;

namespace ai
{
namespace sg
{

class gws_connection : public boost::enable_shared_from_this<gws_connection>
{
public:
	typedef boost::shared_ptr<gws_connection> pointer;

	static pointer create(basio::io_service& io_svc, int slot_);

	int get_slot() const;
	basio::ip::tcp::socket& socket();
	void start(bool tcpkeepalive, bool nodelay);
	void connect(int remote_mid, const std::string& ipaddr, const std::string& port, bool tcpkeepalive, bool nodelay);
	void write(sgt_ctx *SGT, message_pointer& msg);
	void close();

	static int broadcast(sgt_ctx *SGT, message_pointer& msg);

	~gws_connection();

private:
	typedef std::deque<message_pointer> sg_message_queue;

	gws_connection(basio::io_service& io_svc, int slot_);

	void handle_negotiation(const boost::system::error_code& error);
	void handle_read(size_t bytes_transferred, const boost::system::error_code& error);
	void handle_connect(bool tcpkeepalive, bool nodelay, const boost::system::error_code& error);
	void do_close();

	int slot;
	gpp_ctx *GPP;
	sgp_ctx *SGP;
	gwp_ctx *GWP;
	basio::ip::tcp::socket socket_;
	int remote_mid;	// 当前连接的远程主机
	message_pointer read_msg;
	int buffer_size;
	char *buffer;

#if defined(NET_DEBUG)
	int sent_msgs;
	int rcvd_msgs;
	long sent_bytes;
	long rcvd_bytes;
#endif
};

}
}

#endif

