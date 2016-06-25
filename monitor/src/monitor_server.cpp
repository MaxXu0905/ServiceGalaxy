#include "monitor_server.h"
#include "MonitorSetter.h"

namespace bp = boost::posix_time;
namespace ai {
namespace sg {

monitor_server::monitor_server() {
}

monitor_server::~monitor_server() {
}
int monitor_server::svrinit(int argc, char **argv) {

	//设置monitor
	MonitorSetter::factory()->set(monitor);
	MonitorSetter::factory(argc, argv)->set(monitor);
	//MonitorSetter::factory(getenv("TGWSCONFIG"))->set(monitor);
	// 关闭框架提供的线程机制
	SGP->_SGP_threads = 0;
	GPP->_GPP_do_threads = true;
	_thread_group.create_thread(boost::bind(&Monitor::run, &monitor));
	return 0;
}
int monitor_server::run(int flags) {
	sgc_ctx *SGC = SGT->SGC();
	message_pointer recvd_msg = sg_message::create(SGT);

	SGC->enter(SGT, 0);
	BOOST_SCOPE_EXIT((&SGT)(&SGC)) {
		SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	try {
		while (!SGP->_SGP_shutdown && monitor.isRunning()) {
			if (rcvrq(recvd_msg, SGNOBLOCK | SGGETANY))
				continue;
			else if (SGT->_SGT_error_code != SGEBLOCK)
				break;

			SGC->leave(SGT, 0);
			boost::this_thread::sleep(bp::millisec(100));
			//boost::this_thread::yield();
			SGC->enter(SGT, 0);
		}
	} catch (exception& ex) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Fatal error receiving requests, shutting process down")).str(SGLOCALE));
	}
	monitor.stop();
	_thread_group.join_all();
	cleanup();
	return 0;

}
int monitor_server::svrfini() {
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif
	GPP->write_log((_("INFO: Thrift Monitor stopped successfully.")).str(SGLOCALE));
	retval = 0;
	return retval;
}

}
}

