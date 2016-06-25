#include "sg_internal.h"
#include "bb_switch.h"
#include "bbm_chk.h"
#include "bbm_rpc.h"

namespace ai
{
namespace sg
{

bb_switch::bb_switch()
{
	BBM = bbm_ctx::instance();
}

bb_switch::~bb_switch()
{
}

bool bb_switch::getack(sgt_ctx *SGT, message_pointer& msg, int flags, int timeout)
{
	bool done = false;
	int	spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
	sgc_ctx *SGC = SGT->SGC();
	sg_ipc& ipc_mgr = sg_ipc::instance(SGT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("flags={1}, timeout={2}") % flags % timeout).str(SGLOCALE), &retval);
#endif

	sg_msghdr_t *msghdr = *msg;
	msghdr->rcvr = SGC->_SGC_proc;

	while (!done) {
		if (SGP->_SGP_amserver) {
			/*
			 * Registered with the sgclst, so we can
			 * receive remote BB access requests.
			 * Use receive() so that we can handle
			 * service requests while we wait for our
			 * reply. When the ACK/NACK is received,
			 * copy it into msg and return. Note that
			 * using receive() reads the sgmngr's request
			 * queue whereas replies normally arrive on a
			 * server's reply queue.  The assumption here
			 * is that a sgmngr always has a single queue for
			 * requests and replies, so the receive() works.
			 * If a sgmngr is ever allowed to have separate
			 * request and reply queues this code will
			 * have to change.
			 */
			sg_svr& svr_mgr = sg_svr::instance(SGT);
			while (!svr_mgr.rcvrq(msg, flags | SGGETANY | SGSIGRSTRT, timeout)) {
				/*
				 * If the sgmngr message queue has been
				 * removed, return an error.
				 */
				if (SGT->_SGT_error_code == SGEOS
					&& SGT->_SGT_native_code == bi::not_such_file_or_directory
					&& (errno == EIDRM || errno == EINVAL)) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Shutting down sgmngr due to removal of message queue")).str(SGLOCALE));
					return retval;
				}

				if (!SGP->_SGP_shutdown)
					GPP->write_log((_("WARN: sgmngr received stray reply from the sgmngr's reply queue - {1}, {2}") % SGT->strerror() % SGT->strnative()).str(SGLOCALE));
			}

			if (msg == NULL)
				return retval;

			sg_metahdr_t *metahdr = *msg;
			msghdr = *msg;

			if (metahdr->mtype == sg_metahdr_t::bbrqstmtype()) {
				string svc_name = msghdr->svc_name;

				SGT->_SGT_fini_called = false;
				SGT->_SGT_fini_rval = 0;
				sg_svcdsp::instance(SGT).svcdsp(msg, 0);

				if (SGT->_SGT_fini_rval < 0) {
					string temp;
					if (SGT->_SGT_error_code == SGEOS)
						temp = (_(": {1}: {2}") % SGT->strnative() % strerror(errno)).str(SGLOCALE);

					GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr requested {1} operation failed - {2}")
						% svc_name % temp).str(SGLOCALE));

					spawn_flags = SPAWN_RESTARTSRV | SPAWN_CLEANUPSRV;
					bbclean(SGT, SGC->_SGC_proc.mid, NOTAB, spawn_flags);
					BBM->_BBM_cleantime = 0;
				}
				continue;
			}
		} else {
			// can only be an ACK or NACK
			if (!ipc_mgr.receive(msg, flags | SGREPLY, timeout))
				return retval;
		}
		done = true;		/* ACK or NACK */
	}

	/* Remember that rmsg could have changed */
	msghdr = *msg;
	if (msghdr->flags & SGNACK) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGENOCLST;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgmngr cannot find the sgclst")).str(SGLOCALE));
		}
		return retval;
	}

	sg_rpc_rply_t *rply = msg->rpc_rply();
	SGT->_SGT_error_code = rply->error_code;
	if (SGT->_SGT_error_code == SGEOS)
		SGT->_SGT_native_code = msghdr->native_code;

	retval = true;
	return retval;
}

void bb_switch::shutrply(sgt_ctx *SGT, message_pointer& msg, int status, sg_bbparms_t *bbparms)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("status={1}") % status).str(SGLOCALE), NULL);
#endif

	sg_rpc_rply_t *rply = msg->rpc_rply();
	memset(rply, 0, sizeof(sg_rpc_rply_t));
	rply->rtn = -status;
	rply->error_code = status;
	msg->set_length(sg_rpc_rply_t::need(0));
}

}
}

