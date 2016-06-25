#if !defined(__LISTENER_CONNECTION_H__)
#define __LISTENER_CONNECTION_H__

#include <ctime>
#include <iostream>
#include <string>
#include "sg_message.h"
#include "sg_struct.h"

namespace basio = boost::asio;

namespace ai
{
namespace sg
{

class listener_connection : public boost::enable_shared_from_this<listener_connection>
{
public:
	typedef boost::shared_ptr<listener_connection> pointer;

	static pointer create(basio::io_service& io_svc);

	basio::ip::tcp::socket& socket();
	void start();
	void close();
	~listener_connection();

private:
	listener_connection(basio::io_service& io_svc);

	void handle_read(const boost::system::error_code& error);
	void do_close();

	gpp_ctx *GPP;
	sgp_ctx *SGP;
	sgt_ctx *SGT;
	message_pointer read_msg;
	basio::ip::tcp::socket socket_;
};

}
}

#endif

