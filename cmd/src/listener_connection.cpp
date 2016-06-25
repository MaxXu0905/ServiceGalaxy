#include "sg_internal.h"
#include "listener_connection.h"

namespace basio = boost::asio;
namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

listener_connection::pointer listener_connection::create(basio::io_service& io_svc)
{
	return pointer(new listener_connection(io_svc));
}

basio::ip::tcp::socket& listener_connection::socket()
{
	return socket_;
}

void listener_connection::start()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif

	basio::socket_base::keep_alive option1(true);
	socket_.set_option(option1);

	basio::ip::tcp::no_delay option2(true);
    socket_.set_option(option2);

	basio::async_read(socket_,
		basio::buffer(read_msg->raw_data(), MSGHDR_LENGTH),
		boost::bind(&listener_connection::handle_read, shared_from_this(),
			basio::placeholders::error));
}

void listener_connection::close()
{
	basio::io_service& io_svc = socket_.get_io_service();
	io_svc.post(boost::bind(&listener_connection::do_close, shared_from_this()));
}

listener_connection::listener_connection(basio::io_service& io_svc)
	: socket_(io_svc)
{
	GPP = gpp_ctx::instance();
	SGP = sgp_ctx::instance();
	SGT = sgt_ctx::instance();

	read_msg = sg_message::create(SGT);
	read_msg->reserve(sg_rpc_rqst_t::need(0));
}

listener_connection::~listener_connection()
{
}

void listener_connection::handle_read(const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("error={1}") % error).str(SGLOCALE), NULL);
#endif

	if (!error) {
		sg_metahdr_t& metahdr = *read_msg;
		size_t size = metahdr.size - MSGHDR_LENGTH;
		read_msg->reserve(size);

		boost::system::error_code error_code;
		basio::read(socket_, basio::buffer(read_msg->data(), size), error_code);
		if (error_code) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: read error, {1}") % error_code).str(SGLOCALE));
			do_close();
			return;
		}

		// 对消息解码
		read_msg->decode();

		// 处理消息
		sg_agent& agent_mgr = sg_agent::instance(SGT);
		agent_mgr.ta_process(socket_, *read_msg);

		basio::async_read(socket_,
			basio::buffer(read_msg->raw_data(), MSGHDR_LENGTH),
			boost::bind(&listener_connection::handle_read, shared_from_this(),
				basio::placeholders::error));
	} else {
		if (error != basio::error::eof)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: read error, {1}") % error).str(SGLOCALE));
		do_close();
	}
}

void listener_connection::do_close()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif

	boost::system::error_code ignore_error;
	socket_.close(ignore_error);
}

}
}

