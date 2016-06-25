#include "gwp_ctx.h"
#include "gws_connection.h"
#include "gws_queue.h"

namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

gws_circuit_t::gws_circuit_t(int maxconn, const vector<gws_netent_t>& netents)
{
	vector<gws_netent_t>::const_iterator iter = netents.begin();
	for (int i = 0; i < maxconn; i++) {
		lines.push_back(gws_line_t(*iter));
		if (++iter == netents.end())
			iter = netents.begin();
	}
	used_count = 0;
	readies = 0;
}

gws_circuit_t::gws_circuit_t(const gws_circuit_t& rhs)
{
	lines = rhs.lines;
	free_queue = rhs.free_queue;
	used_count = rhs.used_count;
	readies = rhs.readies;
}

gws_circuit_t::~gws_circuit_t()
{
}

gws_circuit_t& gws_circuit_t::operator=(const gws_circuit_t& rhs)
{
	lines = rhs.lines;
	free_queue = rhs.free_queue;
	used_count = rhs.used_count;
	readies = rhs.readies;

	return *this;
}

bool gws_circuit_t::all_ready() const
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", &retval);
#endif

	retval = (lines.size() == readies);
	return retval;
}

bool gws_circuit_t::is_local() const
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", &retval);
#endif

	retval = (readies == 0);
	return retval;
}

gws_line_t * gws_circuit_t::get_line()
{
	gws_line_t *retval = NULL;
#if defined(DEBUG)
	scoped_debug<gws_line_t *> debug(50, __PRETTY_FUNCTION__, "", &retval);
#endif
	bi::scoped_lock<boost::mutex> lock(mutex);

	while (free_queue.empty())
		cond.wait(mutex);

	gws_line_t *line = free_queue.front();
	free_queue.pop_front();

	retval = line;
	return retval;
}

void gws_circuit_t::put_line(gws_line_t *line)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("line={1}") % line).str(SGLOCALE), NULL);
#endif
	bi::scoped_lock<boost::mutex> lock(mutex);

	if (free_queue.empty())
		cond.notify_all();

	free_queue.push_back(line);
}

void gws_circuit_t::set_init(int slot)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("slot={1}") % slot).str(SGLOCALE), NULL);
#endif
	bi::scoped_lock<boost::mutex> lock(mutex);

	if (slot < 0 || slot >= lines.size())
		return;

	gws_line_t& line = lines[slot];
	if (line.node_state == NODE_STATE_CONNECTED) {
		readies--;

		// 从空闲列表中删除
		deque<gws_line_t *>::iterator iter = std::find(free_queue.begin(), free_queue.end(), &line);
		if (iter != free_queue.end())
			free_queue.erase(iter);
		else
			used_count--;
	}

	line.node_state = NODE_STATE_INIT;
	line.recon_time = 0;
	line.conn.reset();

	cond.notify_all();
}

void gws_circuit_t::set_connected(int nidx, connection_pointer conn)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("nidx={1}, conn={2}") % nidx % conn.get()).str(SGLOCALE), NULL);
#endif
	bi::scoped_lock<boost::mutex> lock(mutex);

	int slot = conn->get_slot();
	gws_line_t& line = lines[slot];
	if (line.node_state != NODE_STATE_INIT && line.node_state != NODE_STATE_CONNECTING) {
		gpp_ctx *GPP = gpp_ctx::instance();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error")).str(SGLOCALE));
		abort();
	}

	line.nidx = nidx;
	line.node_state = NODE_STATE_CONNECTED;
	line.conn = conn;

	free_queue.push_back(&line);
	readies++;
	cond.notify_all();
}

/*
 *  Suspend a machine, preventing attempts to connect to it.
 *  	     Multiple suspensions of the same machine cause it
 *  	     to be suspended until the latest time requested.
 */
void gws_circuit_t::suspend(int seconds)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("seconds={1}") % seconds).str(SGLOCALE), NULL);
#endif
	bi::scoped_lock<boost::mutex> lock(mutex);

	// 等待正在使用的链接完成
	while (used_count > 0)
		cond.wait(mutex);

	sg_bboard_t *bbp = SGC->_SGC_bbp;
	time_t recon_time = bbp->bbmeters.timestamp + seconds;
	bool warned = false;
	BOOST_FOREACH(gws_line_t& line, lines) {
		switch (line.node_state) {
		case NODE_STATE_INIT:
		default:
			continue;
		case NODE_STATE_CONNECTING:
		case NODE_STATE_CONNECTED:
			// Log warning message only on first suspension.
			if (!warned) {
				GPP->write_log((_("WARN: Suspending communication with {1}") % SGC->_SGC_ntes[line.nidx].pnode).str(SGLOCALE));
				warned = true;
			}
			break;
		case NODE_STATE_SUSPEND:
		case NODE_STATE_TIMED:
			break;
		}

		if (seconds <= 0) {
			line.node_state = NODE_STATE_SUSPEND;
		} else {
			line.node_state = NODE_STATE_TIMED;
			line.recon_time = recon_time;
		}
		line.conn.reset();
	}

	readies = 0;
	cond.notify_all();
}

void gws_circuit_t::unsuspend()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif
	bi::scoped_lock<boost::mutex> lock(mutex);

	BOOST_FOREACH(gws_line_t& line, lines) {
		switch (line.node_state) {
		case NODE_STATE_SUSPEND:
		case NODE_STATE_TIMED:
			line.node_state = NODE_STATE_INIT;
			line.recon_time = 0;
			break;
		default:
			break;
		}
	}

	cond.notify_all();
}


boost::once_flag gwp_ctx::once_flag = BOOST_ONCE_INIT;
gwp_ctx * gwp_ctx::_instance = NULL;

gwp_ctx * gwp_ctx::instance()
{
	if (_instance == NULL)
		boost::call_once(once_flag, gwp_ctx::init_once);
	return _instance;
}

gwp_ctx::gwp_ctx()
{
	_GWP_dbbm_set = false;
	_GWP_shutdown = false;
	_GWP_net_threads = 0;
	_GWP_queue_threads = 0;
}

gwp_ctx::~gwp_ctx()
{
}

void gwp_ctx::init_once()
{
	_instance = new gwp_ctx();
}

void gwp_ctx::handle_timeout(bool& expired, const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("error={1}") % error).str(SGLOCALE), NULL);
#endif

	if (!error)
		expired = true;
}

}
}

