#if !defined(__GWP_CTX_H__)
#define __GWP_CTX_H__

#include <unistd.h>
#include <string>
#include <map>
#include "log_file.h"
#include "uunix.h"
#include "sg_internal.h"
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread/once.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/unordered_map.hpp>

namespace ai
{
namespace sg
{

enum gws_node_state_t
{
	NODE_STATE_INIT,
	NODE_STATE_CONNECTING,
	NODE_STATE_CONNECTED,
	NODE_STATE_SUSPEND,
	NODE_STATE_TIMED
};

class gws_connection;
class gws_queue;
typedef boost::shared_ptr<gws_connection> connection_pointer;

// 网络连接参数
struct gws_netent_t
{
	std::string ipaddr;
	std::string port;
	std::string netgroup;
	bool tcpkeepalive;
	bool nodelay;

	bool operator<(const gws_netent_t& rhs) const {
		return netgroup < rhs.netgroup;
	}
};


// 两个主机间的单个连接
struct gws_line_t
{
	gws_netent_t netent;			// 网络连接信息
	int nidx;						// 网络连接下标
	gws_node_state_t node_state;	// 连接状态
	time_t recon_time;				// 重连时间
	connection_pointer conn;		// 连接对象

	gws_line_t(const gws_netent_t& netent_)
		: netent(netent_)
	{
		nidx = 0;
		node_state = NODE_STATE_INIT;
		recon_time = std::numeric_limits<time_t>::max();
	}
};

// 两个主机间的多个连接
struct gws_circuit_t
{
	std::vector<gws_line_t> lines;
	boost::mutex mutex;
	boost::condition cond;

	gws_circuit_t(int maxconn, const std::vector<gws_netent_t>& netents);
	gws_circuit_t(const gws_circuit_t& rhs);
	~gws_circuit_t();

	gws_circuit_t& operator=(const gws_circuit_t& rhs);

	bool all_ready() const;
	bool is_local() const;
	gws_line_t * get_line();
	void put_line(gws_line_t *line);
	void set_init(int slot);
	void set_connected(int nidx, connection_pointer conn);
	void suspend(int seconds);
	void unsuspend();

private:
	std::deque<gws_line_t *> free_queue;
	int used_count;
	int readies;
};

class gwp_ctx
{
public:
	static gwp_ctx * instance();
	~gwp_ctx();

	sg_proc_t _GWP_dbbm_proc;
	bool _GWP_dbbm_set;
	boost::mutex _GWP_find_dbbm_mutex;
	boost::condition _GWP_find_dbbm_cond;

	std::vector<gws_circuit_t> _GWP_circuits;				// 网络连接数组

	bool _GWP_shutdown;
	boost::mutex _GWP_serial_mutex;

	basio::io_service _GWP_io_svc;
	boost::mutex _GWP_exit_mutex;
	boost::condition _GWP_exit_cond;

	int _GWP_net_threads;
	int _GWP_queue_threads;
	std::vector<pthread_t> _GWP_thread_group;
	std::vector<gws_queue> _GWP_queue_svcs;

private:
	gwp_ctx();

	static void init_once();
	static void handle_timeout(bool& expired, const boost::system::error_code& error);

	static boost::once_flag once_flag;
	static gwp_ctx *_instance;
};

}
}

#endif

