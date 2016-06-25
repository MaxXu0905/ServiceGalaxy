#include "sg_internal.h"

namespace ai
{
namespace sg
{

sg_svc::sg_svc()
{
	GPP = gpp_ctx::instance();
	SGP = sgp_ctx::instance();
	SGT = sgt_ctx::instance();
}

sg_svc::~sg_svc()
{
}

svc_fini_t sg_svc::forward(const char *service, message_pointer& msg, int flags)
{
	sgc_ctx *SGC = SGT->SGC();
	int acall_flags;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("operation={1}, flags={2}") % service % flags).str(SGLOCALE), NULL);
#endif

	if (!SGC->enter(SGT, 0))
		return svc_fini(SGFAIL, 0, msg);

	BOOST_SCOPE_EXIT((&SGT)(&SGC)) {
		SGC->leave(SGT, 0);
	} BOOST_SCOPE_EXIT_END

	if (SGC->_SGC_crte == NULL) { // no bulletin board
		SGT->_SGT_error_code = SGEPROTO;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Cannot call ATMI routines until you have joined system")).str(SGLOCALE));
	}

	if (SGC->_SGC_amclient || SGC->_SGC_svrstate != SGIN_SVC) {
		SGT->_SGT_error_code = SGEPROTO;
		GPP->write_log((_("WARN: forward called outside operation routine")).str(SGLOCALE));
	}

	if (!SGT->_SGT_intcall && flags != 0) {
		GPP->write_log((_("WARN: forward called with invalid flags {1}") % flags).str(SGLOCALE));
		SGT->_SGT_error_code = SGESVCERR;
		return svc_fini(SGFAIL, 0, msg);
	}

	if (SGC->_SGC_crte->hndl.numghandle > 0) {
		// have good messages outstanding
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Performed forward() with outstanding replies")).str(SGLOCALE));
		SGT->_SGT_error_code = SGESVCERR;
		return svc_fini(SGFAIL, 0, msg);
	}

	// If original message was SGNOREPLY, propagate to the next server.
	if (SGT->_SGT_msghdr.rplymtype == NULLMTYPE)
		acall_flags = (flags | SGNOREPLY);
	else
		acall_flags = flags;

	int retval;
	if (!SGT->_SGT_intcall && SGP->int_name(service)) {
		SGT->_SGT_error_code = SGENOENT;
		retval = -1;
	} else {
		SGT->_SGT_intcall++;
		BOOST_SCOPE_EXIT((&SGT)) {
			SGT->_SGT_intcall--;
		} BOOST_SCOPE_EXIT_END

		acall_flags |= SGFORWARD | SGSIGRSTRT;
		sg_api& api_mgr = sg_api::instance(SGT);
		retval = api_mgr.acall(service, msg, acall_flags);
	}
	if (retval == -1) {
		/*
		 *  Forward request failed, so send reply to client.
		 */
		GPP->write_log((_("WARN: forward acall failure - {1}") % SGT->strerror()).str(SGLOCALE));
		SGT->_SGT_error_code = SGESVCERR;
		return svc_fini(SGFAIL, 0, msg);
	}

	SGT->_SGT_forward_called = true;
	return svc_fini(SGSUCCESS, 0, msg);
}

svc_fini_t sg_svc::svc_fini(int rval, int urcode, message_pointer& fini_msg)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("rval={1}, urcode={2}") % rval % urcode).str(SGLOCALE), NULL);
#endif

	if (SGT->_SGT_fini_called)
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: svc_fini() called more times")).str(SGLOCALE));

	SGT->_SGT_fini_called = true;
	SGT->_SGT_fini_urcode = urcode;
	SGT->_SGT_fini_msg = fini_msg;

	switch (rval) {
	case SGSUCCESS:
		SGT->_SGT_fini_rval = rval;
		SGT->_SGT_error_code = 0;
		break;
	case SGFAIL:
		SGT->_SGT_fini_rval = rval;
		SGT->_SGT_error_code = SGESVCFAIL;
		break;
	case SGEXIT:
		SGT->_SGT_fini_rval = SGFAIL;
		SGT->_SGT_fini_cmd = SGEXIT;
		if (SGT->_SGT_error_code != SGESVCERR)
			SGT->_SGT_error_code = SGESVCFAIL;
		break;
	default:
		throw sg_exception(__FILE__, __LINE__, SGEPROTO, 0, (_("ERROR: Invalid parameter given")).str(SGLOCALE));
	}

	return svc_fini_t();
}

}
}

