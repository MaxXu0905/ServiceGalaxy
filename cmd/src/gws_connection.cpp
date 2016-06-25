#include "gws_connection.h"
#include "gws_rpc.h"

namespace basio = boost::asio;
namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

struct sg_negotiation_t
{
	int mid;
	int slot;
};

gws_connection::pointer gws_connection::create(basio::io_service& io_svc, int slot_)
{
	return pointer(new gws_connection(io_svc, slot_));
}

int gws_connection::get_slot() const
{
	return slot;
}

basio::ip::tcp::socket& gws_connection::socket()
{
	return socket_;
}

void gws_connection::start(bool tcpkeepalive, bool nodelay)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("tcpkeepalive={1}, nodelay={2}") % tcpkeepalive % nodelay).str(SGLOCALE), NULL);
#endif

	basio::socket_base::keep_alive option1(tcpkeepalive);
	socket_.set_option(option1);

	basio::ip::tcp::no_delay option2(nodelay);
	socket_.set_option(option2);

	basio::socket_base::reuse_address option3(true);
	socket_.set_option(option3);

	basio::socket_base::receive_buffer_size option4;
	socket_.get_option(option4);
	buffer_size = option4.value();
	buffer = new char[buffer_size];

	read_msg->reserve(sizeof(sg_negotiation_t));

	basio::async_read(socket_,
		basio::buffer(read_msg->raw_data(), MSGHDR_LENGTH + sizeof(sg_negotiation_t)),
		boost::bind(&gws_connection::handle_negotiation, shared_from_this(),
			basio::placeholders::error));
}

void gws_connection::connect(int remote_mid_, const string& ipaddr, const string& port, bool tcpkeepalive, bool nodelay)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("remote_mid_={1}, ipaddr={2}, port={3}, tcpkeepalive={4}, nodelay={5}") % remote_mid_ % ipaddr % port % tcpkeepalive % nodelay).str(SGLOCALE), NULL);
#endif

	remote_mid = remote_mid_;

	// 发起连接
	basio::ip::tcp::resolver resolver(socket_.get_io_service());
	basio::ip::tcp::resolver::query query(ipaddr, port);
	basio::ip::tcp::resolver::iterator resolve_iter = resolver.resolve(query);
	basio::async_connect(socket_, resolve_iter,
		boost::bind(&gws_connection::handle_connect, shared_from_this(), tcpkeepalive, nodelay,
		basio::placeholders::error));
}

void gws_connection::write(sgt_ctx *SGT, message_pointer& msg)
{
	sg_metahdr_t& metahdr = *msg;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("mtype={1}, size={2}") % metahdr.mtype % metahdr.size).str(SGLOCALE), NULL);
#endif

	msg->encode();

	boost::system::error_code error_code;
	basio::write(socket_, basio::buffer(msg->raw_data(), metahdr.size), error_code);
	if (error_code) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: write error, {1}") % error_code).str(SGLOCALE));
		do_close();
		return;
	}

#if defined(NET_DEBUG)
	sent_bytes += metahdr.size;
	if (++sent_msgs % 100000 == 0)
		GPP->write_log((_("sent_msgs={1}, sent_bytes={2}") % sent_msgs % sent_bytes).str(SGLOCALE));
#endif
}

void gws_connection::close()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif

	basio::io_service& io_svc = socket_.get_io_service();
	io_svc.post(boost::bind(&gws_connection::do_close, shared_from_this()));
}

int gws_connection::broadcast(sgt_ctx *SGT, message_pointer& msg)
{
	gwp_ctx *GWP = gwp_ctx::instance();
	int retval = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, "", &retval);
#endif

	BOOST_FOREACH(gws_circuit_t& circuit, GWP->_GWP_circuits) {
		if (circuit.is_local())
			continue;

		gws_line_t *line = circuit.get_line();
		BOOST_SCOPE_EXIT((&line)(&circuit)) {
			circuit.put_line(line);
		} BOOST_SCOPE_EXIT_END

		gws_connection& conn = *line->conn;
		conn.write(SGT, msg);
		retval++;
	}

	return retval;
}

gws_connection::gws_connection(basio::io_service& io_svc, int slot_)
	: slot(slot_),
	  socket_(io_svc)
{
	GPP = gpp_ctx::instance();
	SGP = sgp_ctx::instance();
	GWP = gwp_ctx::instance();
	remote_mid = -1;
	read_msg = sg_message::create();
	buffer = NULL;

#if defined(NET_DEBUG)
	sent_msgs = 0;
	rcvd_msgs = 0;
	sent_bytes = 0;
	rcvd_bytes = 0;
#endif
}

gws_connection::~gws_connection()
{
	do_close();
	delete []buffer;

#if defined(NET_DEBUG)
	GPP->write_log((_("sent_msgs={1}, sent_bytes={2}") % sent_msgs % sent_bytes).str(SGLOCALE));
	GPP->write_log((_("rcvd_msgs={1}, rcvd_bytes={2}") % rcvd_msgs % rcvd_bytes).str(SGLOCALE));
#endif
}

void gws_connection::handle_negotiation(const boost::system::error_code& error)
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("error={1}") % error).str(SGLOCALE), NULL);
#endif

	if (!error) {
		sg_negotiation_t *nego_data = reinterpret_cast<sg_negotiation_t *>(read_msg->data());
		remote_mid = nego_data->mid;
		slot = nego_data->slot;

		sg_config& config_mgr = sg_config::instance(SGT);
		sg_mchent_t mchent;
		mchent.mid = remote_mid;

		if (!config_mgr.find(mchent)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown hostid {1}.") % remote_mid).str(SGLOCALE));
			do_close();
			return;
		}

		int pidx = SGC->midpidx(remote_mid);
		if (pidx >= GWP->_GWP_circuits.size()) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid remote hostid {1}") % remote_mid).str(SGLOCALE));
			do_close();
			return;
		}

		gws_circuit_t& circuit = GWP->_GWP_circuits[pidx];
		if (slot >= circuit.lines.size()) {
			if (!SGP->_SGP_ambsgws)
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid remote hostid {1}") % remote_mid).str(SGLOCALE));
			do_close();
			return;
		}

		circuit.set_connected(SGC->midnidx(remote_mid), shared_from_this());

		basio::async_read(socket_,
			basio::buffer(buffer, buffer_size),
			basio::transfer_at_least(MSGHDR_LENGTH),
			boost::bind(&gws_connection::handle_read, shared_from_this(),
				basio::placeholders::bytes_transferred,
				basio::placeholders::error));
	} else {
		if (error != basio::error::eof)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: negotiation error, {1}") % error).str(SGLOCALE));
		do_close();
	}
}

void gws_connection::handle_read(size_t bytes_transferred, const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("bytes_transferred={1}, error={2}") % bytes_transferred % error).str(SGLOCALE), NULL);
#endif

	if (!error) {
		const char *bufp = buffer;

		do {
			if (bytes_transferred >= MSGHDR_LENGTH) {
				memcpy(read_msg->raw_data(), bufp, MSGHDR_LENGTH);
				bufp += MSGHDR_LENGTH;
				bytes_transferred -= MSGHDR_LENGTH;
			} else {
				memcpy(read_msg->raw_data(), bufp, bytes_transferred);

				boost::system::error_code error_code;
				basio::read(socket_, basio::buffer(read_msg->raw_data() + bytes_transferred, MSGHDR_LENGTH - bytes_transferred), error_code);
				if (error_code) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: read error, {1}") % error_code).str(SGLOCALE));
					do_close();
					return;
				}

				bytes_transferred = 0;
			}

			sg_metahdr_t *metahdr = *read_msg;
			size_t size = metahdr->size - MSGHDR_LENGTH;
			read_msg->reserve(size);

			if (size <= bytes_transferred) {
				memcpy(read_msg->data(), bufp, size);
				bufp += size;
				bytes_transferred -= size;
			} else {
				if (bytes_transferred > 0)
					memcpy(read_msg->data(), bufp, bytes_transferred);

				boost::system::error_code error_code;
				basio::read(socket_, basio::buffer(read_msg->data() + bytes_transferred, size - bytes_transferred), error_code);
				if (error_code) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: read error, {1}") % error_code).str(SGLOCALE));
					do_close();
					return;
				}

				bytes_transferred = 0;
			}

			bool for_gws;
			sgt_ctx *SGT = sgt_ctx::instance();
			sgc_ctx *SGC = SGT->SGC();

			// 处理收到的消息
			read_msg->decode();
			metahdr = *read_msg;
			sg_msghdr_t& msghdr = *read_msg;

#if defined(NET_DEBUG)
			rcvd_bytes += metahdr->size;
			if (++rcvd_msgs % 100000 == 0)
				GPP->write_log((_("rcvd_msgs={1}, rcvd_bytes={2}") % rcvd_msgs % rcvd_bytes).str(SGLOCALE));
#endif

			// 检查是否为meta消息
			if (metahdr->flags & METAHDR_FLAGS_GW) {
				if (msghdr.rcvr.is_gws())
					for_gws = true;
				else
					for_gws = false;
			} else {
				for_gws = false;
			}

			if (for_gws) {
				gws_rpc& grpc_mgr = gws_rpc::instance(SGT);
				(void)grpc_mgr.process_rpc(this, read_msg);
			} else {
				// 如果不是应答，则增加负载
				if (!metahdr->is_rply()) {
					sg_qte& qte_mgr = sg_qte::instance(SGT);
					sg_scte& scte_mgr = sg_scte::instance(SGT);
					sg_scte_t *scte = scte_mgr.sgid2ptr(msghdr.alt_flds.svcsgid);
					SGC->_SGC_svcindx = scte->hashlink.rlink;
					(void)qte_mgr.enqueue(scte);
				}

				sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
				if (!ipc_mgr.send(read_msg, SGBYPASS))
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: sggws failed to send message to target {1}") % metahdr->qaddr).str(SGLOCALE));
			}
		} while (bytes_transferred > 0);

		basio::async_read(socket_,
			basio::buffer(buffer, buffer_size),
			basio::transfer_at_least(MSGHDR_LENGTH),
			boost::bind(&gws_connection::handle_read, shared_from_this(),
				basio::placeholders::bytes_transferred,
				basio::placeholders::error));
	} else {
		if (error != basio::error::eof)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: read error, {1}") % error).str(SGLOCALE));
		do_close();
	}
}

void gws_connection::handle_connect(bool tcpkeepalive, bool nodelay, const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("tcpkeepalive={1}, nodelay={2}, error={3}") % tcpkeepalive % nodelay % error).str(SGLOCALE), NULL);
#endif

	if (!error) {
		basio::socket_base::keep_alive option1(tcpkeepalive);
		socket_.set_option(option1);

		basio::ip::tcp::no_delay option2(nodelay);
		socket_.set_option(option2);

		basio::socket_base::reuse_address option3(true);
		socket_.set_option(option3);

		basio::socket_base::receive_buffer_size option4;
		socket_.get_option(option4);
		buffer_size = option4.value();
		buffer = new char[buffer_size];

		sgt_ctx *SGT = sgt_ctx::instance();
		sgc_ctx *SGC = SGT->SGC();
		int pidx = SGC->midpidx(remote_mid);
		gws_circuit_t& circuit = GWP->_GWP_circuits[pidx];
		circuit.set_connected(SGC->midnidx(remote_mid), shared_from_this());

		message_pointer nego_msg = sg_message::create(SGT);
		sg_negotiation_t nego_data;
		nego_data.mid = SGC->getmid();
		nego_data.slot = slot;
		nego_msg->assign(reinterpret_cast<char *>(&nego_data), sizeof(sg_negotiation_t));
		write(SGT, nego_msg);

		basio::async_read(socket_,
			basio::buffer(buffer, buffer_size),
			basio::transfer_at_least(MSGHDR_LENGTH),
			boost::bind(&gws_connection::handle_read, shared_from_this(),
				basio::placeholders::bytes_transferred,
				basio::placeholders::error));
	} else {
		if (error != basio::error::eof)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: connect error, {1}") % error).str(SGLOCALE));
		do_close();
	}
}

void gws_connection::do_close()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif

	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();

	boost::system::error_code ignore_error;
	socket_.close(ignore_error);

	int pidx = SGC->midpidx(remote_mid);
	if (pidx < GWP->_GWP_circuits.size()) {
		gws_circuit_t& circuit = GWP->_GWP_circuits[pidx];
		circuit.set_init(slot);
	}
}

}
}

