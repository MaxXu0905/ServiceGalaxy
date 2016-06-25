#include "dbc_internal.h"

namespace ai
{
namespace sg
{

void dbc_rte_t::reset_tran()
{
	ipc_sem::tas(&accword, SEM_WAIT);
	flags &= ~RTE_TRAN_MASK;
	ipc_sem::semclear(&accword);
}

void dbc_rte_t::set_flags(int flags_)
{
	ipc_sem::tas(&accword, SEM_WAIT);
	flags |= flags_;
	ipc_sem::semclear(&accword);
}

void dbc_rte_t::clear_flags(int flags_)
{
	ipc_sem::tas(&accword, SEM_WAIT);
	flags &= ~flags_;
	ipc_sem::semclear(&accword);
}

bool dbc_rte_t::in_flags(int flags_) const
{
	return (flags & flags_);
}

dbc_rte& dbc_rte::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<dbc_rte *>(DBCT->_DBCT_mgr_array[DBC_RTE_MANAGER]);
}

void dbc_rte::init()
{
	rte->next = -1;
	rte->accword = 0;
	rte->flags = 0;
	rte->rte_id = rte - DBCT->_DBCT_rtes;
	(void)new (&rte->mutex) bi::interprocess_mutex();
	(void)new (&rte->cond) bi::interprocess_condition();
	rte->autocommit = false;
	rte->pid = 0;
	rte->timeout = 0;
	rte->redo_full = -1;
	rte->redo_free = -1;
	rte->redo_root = -1;
	rte->tran_id = 0;
 	rte->rtimestamp = 0;
}

void dbc_rte::reinit()
{
	rte->flags = RTE_IN_USE;
	(void)new (&rte->mutex) bi::interprocess_mutex();
	(void)new (&rte->cond) bi::interprocess_condition();
	rte->autocommit = false;
	rte->timeout = 0;
	rte->pid = DBCP->_DBCP_current_pid;
	rte->tran_id = 0;
 	rte->rtimestamp = 0;
}

void dbc_rte::set_ctx(dbc_rte_t *rte_)
{
	rte = rte_;
}

dbc_rte_t * dbc_rte::get_rte()
{
	return rte;
}

dbc_rte_t * dbc_rte::base()
{
	return DBCT->_DBCT_rtes;
}

bool dbc_rte::get_slot()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (DBCT->_DBCT_bbp == NULL) {
		DBCT->_DBCT_error_code = DBCESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function should be called after login.")).str(SGLOCALE));
		return retval;
	}

	dbc_rte_t *currte;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	if (DBCP->_DBCP_is_server) {
		currte = DBCT->_DBCT_rtes + RTE_SLOT_SERVER;
		if (currte->flags & RTE_IN_USE) {
			if (DBCP->_DBCP_current_pid != currte->pid) {
				if (kill(currte->pid, 0) != -1)
					DBCT->_DBCT_error_code = DBCEDUPENT;
				else
					DBCT->_DBCT_error_code = DBCENOSERVER;

				return retval;
			}
		} else {
			rte = currte;
			reinit();
			retval = true;
			return retval;
		}
	} else if (DBCP->_DBCP_is_sync) {
		currte = DBCT->_DBCT_rtes + RTE_SLOT_SYNC;
		if (currte->flags & RTE_IN_USE) {
			if (kill(currte->pid, 0) != -1)
				DBCT->_DBCT_error_code = DBCEDUPENT;
			else
				DBCT->_DBCT_error_code = DBCENOSERVER;
			return retval;
		}

		rte = currte;
		reinit();
		retval = true;
		return retval;
	} else if (DBCP->_DBCP_is_admin) {
		currte = DBCT->_DBCT_rtes + RTE_SLOT_ADMIN;
		if (!(currte->flags & RTE_IN_USE) || (kill(currte->pid, 0) == -1)) {
			rte = currte;
			reinit();
			retval = true;
			return retval;
		}

		// If we have started a dbc_admin, the second will start as normal mode.
		if (currte->pid != DBCP->_DBCP_current_pid) {
			cout << (_("WARN: dbc_admin is already started, start in normal mode.\n")).str(SGLOCALE);
			DBCP->_DBCP_is_admin = false;
		}
	}

	currte = DBCT->_DBCT_rtes + RTE_SLOT_MAX_RESERVED;
	dbc_rte_t *endrte = DBCT->_DBCT_rtes + bbp->bbparms.maxrtes;
	for (; currte < endrte; ++currte) {
		if (!currte->in_flags(RTE_IN_USE)) {
			rte = currte;
			reinit();
			break;
		}
	}

	if (currte == endrte) {
		DBCT->_DBCT_error_code = DBCENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: No free RTE is available.")).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

void dbc_rte::put_slot()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("rte={1}") % rte).str(SGLOCALE), NULL);
#endif

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	if (rte) {
		rte->clear_flags(RTE_IN_USE);
		rte = NULL;
	}
}

bool dbc_rte::set_tran()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (in_flags(RTE_ABORT_ONLY)) {
		DBCT->_DBCT_error_code = DBCETIME;
		return retval;
	}

	if (in_flags(RTE_IN_TRANS)) {
		retval = true;
		return retval;
	}

	rte->set_flags(RTE_IN_TRANS);
	DBCT->_DBCT_redo_sequence = 0;

	{
		dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
		dbc_bbmeters_t& bbmeters = bbp->bbmeters;
		bi::scoped_lock<bi::interprocess_mutex> lock(bbmeters.sqlite_mutex);
		rte->tran_id = ++bbmeters.tran_id;
	}

	rte->rtimestamp = time(0);

	retval = true;
	return retval;
}

void dbc_rte::reset_tran()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	rte->reset_tran();
	rte->tran_id = 0;
}

void dbc_rte::set_flags(int flags)
{
	rte->set_flags(flags);
}

void dbc_rte::clear_flags(int flags)
{
	rte->clear_flags(flags);
}

bool dbc_rte::in_flags(int flags) const
{
	return rte->in_flags(flags);
}

void dbc_rte::set_autocommit(bool autocommit)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("autocommit={1}") % autocommit).str(SGLOCALE), NULL);
#endif

	rte->autocommit = autocommit;
}

bool dbc_rte::get_autocommit() const
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	retval = rte->autocommit;
	return retval;
}

void dbc_rte::set_timeout(long timeout)
{
#if defined(DEBUG)
	scoped_debug<long> debug(100, __PRETTY_FUNCTION__, (_("timeout={1}") % timeout).str(SGLOCALE), NULL);
#endif

	rte->timeout = timeout;
}

long dbc_rte::get_timeout() const
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	retval = rte->timeout;
	return retval;
}

dbc_rte::dbc_rte()
{
}

dbc_rte::~dbc_rte()
{
}

}
}

