#include "sg_internal.h"

namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

sharable_bblock::sharable_bblock(sgt_ctx *SGT_)
	: SGT(SGT_)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	if ((SGT->_SGT_lock_type_by_me & (SHARABLE_BBLOCK | EXCLUSIVE_BBLOCK)) || bbp == NULL) {
		require_unlock = false;
		return;
	}

	if (!bbp->bb_mutex.try_lock_sharable()) {
		time_t next_time;
		sgp_ctx *SGP = sgp_ctx::instance();

		while (1) {
			if (SGP->_SGP_ambbm)
				next_time = time(0) + bbp->bbparms.scan_unit;
			else
				next_time = bbp->bbmeters.timestamp + 1 + bbp->bbparms.scan_unit;

			if (bbp->bb_mutex.timed_lock_sharable(bp::from_time_t(next_time)))
				break;

			gpp_ctx *GPP = gpp_ctx::instance();
			if (!SGP->_SGP_ambbm) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: BB exclusive locked by process {1}") % bbp->bblock_pid).str(SGLOCALE));
				continue;
			}

			if (bbp->bblock_pid <= 0 || kill(bbp->bblock_pid, 0) == -1) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: BB exclusive locked by dead process {1}, reset by sgmngr, system may be corrupted") % bbp->bblock_pid).str(SGLOCALE));
				bbp->bblock_pid = 0;
				(void)new (&bbp->bb_mutex) bi::interprocess_recursive_mutex();
			}
		}
	}

	if (SGC->_SGC_crte)
		SGC->_SGC_crte->lock_type |= SHARABLE_BBLOCK;
	SGT->_SGT_lock_type_by_me |= SHARABLE_BBLOCK;
	require_unlock = true;
}

sharable_bblock::~sharable_bblock()
{
	if (!require_unlock)
		return;

	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	if (SGC->_SGC_crte)
		SGC->_SGC_crte->lock_type &= ~SHARABLE_BBLOCK;
	bbp->bb_mutex.unlock_sharable();
	SGT->_SGT_lock_type_by_me &= ~SHARABLE_BBLOCK;
}

scoped_bblock::scoped_bblock(sgt_ctx *SGT_)
	: SGT(SGT_)
{
	lock();
}

scoped_bblock::scoped_bblock(sgt_ctx *SGT_, bi::defer_lock_type)
	: SGT(SGT_)
{
	require_unlock = false;
}

scoped_bblock::~scoped_bblock()
{
	if (!require_unlock)
		return;

	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	if (SGC->_SGC_crte)
		SGC->_SGC_crte->lock_type &= ~EXCLUSIVE_BBLOCK;
	bbp->bblock_pid = 0;
	bbp->bb_mutex.unlock();
	SGT->_SGT_lock_type_by_me &= ~EXCLUSIVE_BBLOCK;
}

void scoped_bblock::lock()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	if ((SGT->_SGT_lock_type_by_me & EXCLUSIVE_BBLOCK) || bbp == NULL) {
		require_unlock = false;
		return;
	}

	if (!bbp->bb_mutex.try_lock()) {
		time_t next_time;
		sgp_ctx *SGP = sgp_ctx::instance();

		while (1) {
			if (SGP->_SGP_ambbm)
				next_time = time(0) + bbp->bbparms.scan_unit;
			else
				next_time = bbp->bbmeters.timestamp + 1 + bbp->bbparms.scan_unit;

			if (bbp->bb_mutex.timed_lock(bp::from_time_t(next_time)))
				break;

			gpp_ctx *GPP = gpp_ctx::instance();
			if (!SGP->_SGP_ambbm) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: BB exclusive locked by process {1}") % bbp->bblock_pid).str(SGLOCALE));
				continue;
			}

			if (bbp->bblock_pid <= 0 || kill(bbp->bblock_pid, 0) == -1) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: BB exclusive locked by dead process {1}, reset by sgmngr, system may be corrupted") % bbp->bblock_pid).str(SGLOCALE));
				bbp->bblock_pid = 0;
				(void)new (&bbp->bb_mutex) bi::interprocess_recursive_mutex();
			}
		}
	}

	bbp->bblock_pid = SGC->_SGC_proc.pid;
	if (SGC->_SGC_crte)
		SGC->_SGC_crte->lock_type |= EXCLUSIVE_BBLOCK;
	SGT->_SGT_lock_type_by_me |= EXCLUSIVE_BBLOCK;
	require_unlock = true;
}

void scoped_bblock::unlock()
{
	if (!require_unlock)
		return;

	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	if (SGC->_SGC_crte)
		SGC->_SGC_crte->lock_type &= ~EXCLUSIVE_BBLOCK;
	bbp->bblock_pid = 0;
	bbp->bb_mutex.unlock();
	SGT->_SGT_lock_type_by_me &= ~EXCLUSIVE_BBLOCK;
	require_unlock = false;
}

scoped_syslock::scoped_syslock(sgt_ctx *SGT_)
	: SGT(SGT_)
{
	lock();
}

scoped_syslock::~scoped_syslock()
{
	if (!require_unlock)
		return;

	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	if (SGC->_SGC_crte)
		SGC->_SGC_crte->lock_type &= ~EXCLUSIVE_SYSLOCK;
	bbp->syslock_pid = 0;
	bbp->sys_mutex.unlock();
	SGT->_SGT_lock_type_by_me &= ~EXCLUSIVE_SYSLOCK;
}

void scoped_syslock::lock()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;

	if ((SGT->_SGT_lock_type_by_me & EXCLUSIVE_SYSLOCK) || bbp == NULL) {
		require_unlock = false;
		return;
	}

	if (!bbp->sys_mutex.try_lock()) {
		time_t next_time;
		sgp_ctx *SGP = sgp_ctx::instance();

		while (1) {
			if (SGP->_SGP_ambbm)
				next_time = time(0) + bbp->bbparms.scan_unit;
			else
				next_time = bbp->bbmeters.timestamp + 1 + bbp->bbparms.scan_unit;

			if (bbp->sys_mutex.timed_lock(bp::from_time_t(next_time)))
				break;

			gpp_ctx *GPP = gpp_ctx::instance();
			if (!SGP->_SGP_ambbm) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: SYSTEM exclusive locked by process {1}") % bbp->syslock_pid).str(SGLOCALE));
				continue;
			}

			if (bbp->syslock_pid <= 0 || kill(bbp->syslock_pid, 0) == -1) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: SYSTEM exclusive locked by dead process {1}, reset by sgmngr, system may be corrupted") % bbp->syslock_pid).str(SGLOCALE));
				bbp->syslock_pid = 0;
				(void)new (&bbp->sys_mutex) bi::interprocess_recursive_mutex();
			}
		}
	}

	bbp->syslock_pid = SGC->_SGC_proc.pid;
	if (SGC->_SGC_crte)
		SGC->_SGC_crte->lock_type |= EXCLUSIVE_SYSLOCK;
	SGT->_SGT_lock_type_by_me |= EXCLUSIVE_SYSLOCK;
	require_unlock = true;
}

void scoped_syslock::unlock()
{
	if (!require_unlock)
		return;

	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	if (SGC->_SGC_crte)
		SGC->_SGC_crte->lock_type &= ~EXCLUSIVE_SYSLOCK;
	bbp->syslock_pid = 0;
	bbp->sys_mutex.unlock();
	SGT->_SGT_lock_type_by_me &= ~EXCLUSIVE_SYSLOCK;
	require_unlock = false;
}

}
}

