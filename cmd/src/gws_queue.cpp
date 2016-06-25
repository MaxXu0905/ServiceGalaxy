#include "gws_queue.h"
#include "gws_connection.h"
#include "gws_rpc.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

gws_queue::gws_queue()
{
	GWP = gwp_ctx::instance();
}

gws_queue::~gws_queue()
{
}

void * gws_queue::static_run(void *arg)
{
	gws_queue *_instance = reinterpret_cast<gws_queue *>(arg);

	_instance->run(0);
	return NULL;
}

int gws_queue::run(int flags)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, (_("flags={1}") % flags).str(SGLOCALE), &retval);
#endif

	// 重置线程相关对象
	SGT = sgt_ctx::instance();

	// 设置sg_svr对象指针
	if (SGT->_SGT_mgr_array[SVR_MANAGER] != NULL && SGT->_SGT_mgr_array[SVR_MANAGER] != this) {
		GPP->write_log(__FILE__, __LINE__, (_("FATAL: Internal error, sg_svr can only be called once per thread")).str(SGLOCALE));
		return -1;
	}
	SGT->_SGT_mgr_array[SVR_MANAGER] = this;

	// 设置gws_rpc对象
	if (SGT->_SGT_mgr_array[GRPC_MANAGER] != NULL) {
		delete SGT->_SGT_mgr_array[GRPC_MANAGER];
		SGT->_SGT_mgr_array[GRPC_MANAGER] = NULL;
	}
	SGT->_SGT_mgr_array[GRPC_MANAGER] = new gws_rpc();

	sgc_ctx *SGC = SGT->SGC();
	message_pointer recvd_msg = sg_message::create(SGT);
	bool is_clear = false;

	// continuously receive messages
	try {
		while (!SGP->_SGP_shutdown) {
			if (!rcvrq(recvd_msg, SGGETANY | SGKEEPCMPRS))
				continue;

			// 当收到sgmngr的请求时，需要重新打开消息队列，
			// 因为sgmngr可能重新创建该队列
			if (SGP->_SGP_ambsgws && !is_clear) {
				sg_msghdr_t& msghdr = *recvd_msg;
				if (msghdr.sender.is_bbm()) {
					SGP->clear();
					is_clear = true;
				}
			}

			// 检查是否为meta消息
			sg_metahdr_t& metahdr = *recvd_msg;
			if ((metahdr.flags & METAHDR_FLAGS_GW) && !SGC->remote(metahdr.mid)) {
				gws_rpc& grpc_mgr = gws_rpc::instance(SGT);
				(void)grpc_mgr.process_rpc(NULL, recvd_msg);
			} else {
				int pidx = SGC->midpidx(metahdr.mid);
				gws_circuit_t& circuit = GWP->_GWP_circuits[pidx];
				gws_line_t *line = circuit.get_line();
				BOOST_SCOPE_EXIT((&line)(&circuit)) {
					circuit.put_line(line);
				} BOOST_SCOPE_EXIT_END

				gws_connection *conn = line->conn.get();
				if (conn == NULL) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: message was dropped since remote network is out of operation, hostid = {1}") % metahdr.mid).str(SGLOCALE));
					continue;
				}

				conn->write(SGT, recvd_msg);
			}

			if (!finrq())
				break;
		}

		/*
		 * This thread will still do work even receive shutdown message.
		 * It's because sg_ste::mbremove() will use this thread to
		 * send message to sgclst
		 */
		while (!GWP->_GWP_shutdown) {
			if (!rcvrq(recvd_msg, SGGETANY | SGNOBLOCK | SGKEEPCMPRS)) {
				boost::this_thread::sleep(boost::posix_time::millisec(1));
				continue;
			}

			// 当收到sgmngr的请求时，需要重新打开消息队列，
			// 因为sgmngr可能重新创建该队列
			if (SGP->_SGP_ambsgws && !is_clear) {
				sg_msghdr_t& msghdr = *recvd_msg;
				if (msghdr.sender.is_bbm()) {
					SGP->clear();
					is_clear = true;
				}
			}

			// 检查是否为meta消息
			sg_metahdr_t& metahdr = *recvd_msg;
			if ((metahdr.flags & METAHDR_FLAGS_GW) && !SGC->remote(metahdr.mid)) {
				gws_rpc& grpc_mgr = gws_rpc::instance(SGT);
				(void)grpc_mgr.process_rpc(NULL, recvd_msg);
			} else {
				int pidx = SGC->midpidx(metahdr.mid);
				gws_circuit_t& circuit = GWP->_GWP_circuits[pidx];
				gws_line_t *line = circuit.get_line();
				BOOST_SCOPE_EXIT((&line)(&circuit)) {
					circuit.put_line(line);
				} BOOST_SCOPE_EXIT_END

				gws_connection *conn = line->conn.get();
				if (conn == NULL) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: message was dropped since remote network is out of operation, hostid = {1}") % metahdr.mid).str(SGLOCALE));
					continue;
				}

				conn->write(SGT, recvd_msg);
			}

			if (!finrq())
				break;
		}
	} catch (exception& ex) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Fatal error receiving requests, shutting process down, {1}") % ex.what()).str(SGLOCALE));
		return retval;
	}

	retval = 0;
	return retval;	// no more processing
}

}
}

