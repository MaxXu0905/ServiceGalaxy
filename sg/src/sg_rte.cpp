#include "sg_internal.h"

namespace bi = boost::interprocess;
namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

sg_rte& sg_rte::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<sg_rte *>(SGT->_SGT_mgr_array[RTE_MANAGER]);
}

bool sg_rte::enter()
{
	return (this->*SGT->SGC()->_SGC_authsw->enter)();
}

bool sg_rte::smenter()
{
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (!SGC->_SGC_proc.pid)
		SGC->_SGC_proc.pid = getpid();

 	if (SGC->_SGC_slot != -1 && SGC->_SGC_slot >= SGC->MAXACCSRS()) {
		retval = get_Rslot();
		return retval;
	}

	if (SGP->_SGP_oldpid) {
		if ((SGP->_SGP_boot_flags & BF_RESTART) && !(SGP->_SGP_boot_flags & BF_MIGRATE)) {
			retval = get_oldslot();
			return retval;
		}

		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Migrating and given an old pid when none expected")).str(SGLOCALE));
		return retval;
	}

	if (!get_slot()) {
		SGT->_SGT_error_code = SGENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to register, registry table full")).str(SGLOCALE));
		return retval;
	}

	sg_rte_t *rte = SGC->_SGC_crte;
	SGC->_SGC_slot = ptr2link(rte);
	rte->slot = SGC->_SGC_slot;
	rte->pid = SGC->_SGC_proc.pid;
	rte->mid = SGC->_SGC_proc.mid;
	rte->qaddr = -1;
	if (strcmp(SGC->_SGC_authsw->name, "SERVER") == 0)
		rte->rflags = CLIENT | SERVER;
	else
		rte->rflags = CLIENT;

	rte->hndl.numghandle = 0;
	rte->hndl.numshandle = 0;
	rte->hndl.gen = 0;
	reset_slot(rte, time(0));

	retval = true;
	return retval;
}

bool sg_rte::nullenter()
{
	bool retval = true;
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", &retval);
#endif

	return retval;
}

void sg_rte::leave(sg_rte_t *rte)
{
	(this->*SGT->SGC()->_SGC_authsw->leave)(rte);
}

void sg_rte::smleave(sg_rte_t *rte)
{
	sgc_ctx *SGC = SGT->SGC();
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!SGP->_SGP_amserver && (SGP->_SGP_boot_flags & BF_RESTART))
		return;

	if (rte == NULL && (rte = SGC->_SGC_crte) == NULL)
		return;

	if (rte < SGC->_SGC_rtes || rte >= SGC->_SGC_rtes + SGC->MAXACCSRS() + MAXADMIN) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid registry table slot index passed, unable to leave")).str(SGLOCALE));
		return;
	}

	scoped_syslock syslock(SGT);

	rte->pid = 0;
	rte->mid = BADMID;
	rte->qaddr = -1;
	rte->hndl.numghandle = 0;
	rte->hndl.numshandle = 0;
	rte->hndl.gen = 0;
	reset_slot(rte, 0);

	if (rte->slot < SGC->MAXACCSRS())
		put(rte);
}

void sg_rte::nullleave(sg_rte_t *rte)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", NULL);
#endif
}

bool sg_rte::update(sg_rte_t *rte)
{
	return (this->*SGT->SGC()->_SGC_authsw->update)(rte);
}

/*
 * Update the registry table entry for the current process based on the
 * information passed to this routine. Mind you that the process must have
 * joined the application before updating its entry.
 */
bool sg_rte::smupdate(sg_rte_t *rte)
{
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (SGC->_SGC_crte == NULL) {
		SGT->_SGT_error_code = SGEPROTO;
		return retval;
	}

	// update only those fields that are set in rte
	if (rte->usrname[0] != '\0')
		strcpy(SGC->_SGC_crte->usrname, rte->usrname);
	if (rte->cltname[0] != '\0')
		strcpy(SGC->_SGC_crte->cltname, rte->cltname);
	if (rte->grpname[0] != '\0')
		strcpy(SGC->_SGC_crte->grpname, rte->grpname);

	retval = true;
	return retval;
}

bool sg_rte::nullupdate(sg_rte_t *rte)
{
	bool retval = true;
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", &retval);
#endif

	return retval;
}

bool sg_rte::clean(pid_t pid, sg_rte_t *rte)
{
	return (this->*SGT->SGC()->_SGC_authsw->clean)(pid, rte);
}

// 清除给定pid下的注册节点，
// 该函数只能由BBM调用，这里不加锁，以防止死锁
bool sg_rte::smclean(pid_t pid, sg_rte_t *rte)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	sg_rte_t *rtep;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, (_("pid={1}") % pid).str(SGLOCALE), &retval);
#endif

	if (rte == NULL) {
		sg_rte_t *res = link2ptr(SGC->MAXACCSRS());
		sg_rte_t *endp = res + MAXADMIN;
		if (bbmap->rte_use == -1)
			rtep = res;
		else
			rtep = link2ptr(bbmap->rte_use);

		while (rtep < endp) {
			if (rtep->pid == pid) {
				if (rte == NULL)
					rte = rtep;

				if (rtep->lock_type != UNLOCK) {
					rte = rtep;
					break;
				}
			}

			if (rtep >= res) {
				rtep++;
				continue;
			}

			if (rtep->nextreg != -1)
				rtep = link2ptr(rtep->nextreg);
			else
				rtep = res;
		}

		if (rte == NULL) {
			SGT->_SGT_error_code = SGENOENT;
			return retval;
		}
	}

	if (!SGC->_SGC_proc.pid)
		SGC->_SGC_proc.pid = getpid();

	// 调整锁，看有没有加锁后死掉的情况发生
	if (rte->lock_type & EXCLUSIVE_BBLOCK) {
		GPP->write_log((_("INFO: BB exclusive lock recovered.")).str(SGLOCALE));
		bbp->bb_mutex.unlock();
	}
	if (rte->lock_type & SHARABLE_BBLOCK) {
		GPP->write_log((_("INFO: BB sharable lock recovered.")).str(SGLOCALE));
		bbp->bb_mutex.unlock_sharable();
	}
	if (rte->lock_type & EXCLUSIVE_SYSLOCK) {
		GPP->write_log((_("INFO: syslock recovered.")).str(SGLOCALE));
		bbp->sys_mutex.unlock();
	}
	rte->lock_type = UNLOCK;

	if (!(rte->rflags & (SERVER | SERVER_CTXT)) && rte->qaddr != -1) {
		sg_proc& proc_mgr = sg_proc::instance(SGT);
		proc_mgr.removeq(rte->qaddr);
	}

	if (!(rte->rflags & (SERVER | SERVER_CTXT))
		|| (rte->rflags & (SERVER | SERVER_CTXT)
			&& (rte->rflags & PROC_DEAD))
		|| (rte->slot == SGC->MAXACCSRS() + AT_BBM)) {
		int boot_flags = SGP->_SGP_boot_flags;
		SGP->_SGP_boot_flags &= ~BF_RESTART;
		int sv_slot = SGC->_SGC_slot;
		smleave(rte);
		SGC->_SGC_slot = sv_slot;
		SGP->_SGP_boot_flags = boot_flags;
	}

	sg_ste_t ste;
	if ((ste.grpid() = rte->rt_grpid) != 0 && (ste.svrid() = rte->rt_svrid) != 0) {
		int cnt = sg_ste::instance(SGT).retrieve(S_GRPID, &ste, &ste, NULL);
		if (cnt != 1 || (cnt == 1 && ste.pid() != rte->pid))
			rte->rflags |= PROC_DEAD;
	} else {
		rte->rflags |= PROC_DEAD;
	}

	retval = true;
	return retval;
}

bool sg_rte::nullclean(pid_t pid, sg_rte_t *rte)
{
	bool retval = true;
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, (_("pid={1}") % pid).str(SGLOCALE), &retval);
#endif
	return retval;
}

sg_rte_t * sg_rte::find_slot(pid_t pid)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_rte_t *strtp, *endp, *res, *p;
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, (_("pid={1}") % pid).str(SGLOCALE), NULL);
#endif

	// find the slot first
	res = link2ptr(SGC->MAXACCSRS());
	endp = link2ptr(SGC->MAXACCSRS() + MAXADMIN);
	if (bbp->bbmap.rte_use == -1)
		strtp = res;
	else
		strtp = link2ptr(bbp->bbmap.rte_use);

    p = strtp;
    while (p < endp) {
		if ((p->pid == pid) && !(p->rflags & SERVER_CTXT))
			return p;

		if (p >= res) {
			p++;
			continue;
		}
		if (p->nextreg != -1)
			p = link2ptr(p->nextreg);
		else
			p = res;
	}

	return NULL;
}

// return indication of whether or not process is "privileged"
bool sg_rte::suser(const sg_bbparms_t& bbp)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", &retval);
#endif

	uid_t uid = geteuid();
	if (uid != 0 && uid != bbp.uid) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGEPERM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Not a privileged user, incorrect permissions")).str(SGLOCALE));
		}

		return retval;
	}

	if (uid == 0) {
		// running as root, become the administrator
		if (setgid(bbp.gid) == -1) {
			SGT->_SGT_error_code = SGEPERM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to change group via setgid()")).str(SGLOCALE));
			return retval;
		}
		if (setgid(bbp.uid) == -1) {
			SGT->_SGT_error_code = SGEPERM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to change user via setuid()")).str(SGLOCALE));
			return retval;
		}
	}

	sgc_ctx *SGC = SGT->SGC();
	if (SGC->_SGC_slot >= bbp.maxaccsrs + MAXADMIN	|| SGC->_SGC_slot < bbp.maxaccsrs) {
		if (SGT->_SGT_error_code == 0) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Invalid registry table slot index passed, unable to register")).str(SGLOCALE));
		}

		return retval;
	}

	retval = true;
	return retval;
}

sg_rte_t * sg_rte::link2ptr(int link)
{
	sgc_ctx *SGC = SGT->SGC();
	return SGC->_SGC_rtes + link;
}

int sg_rte::ptr2link(const sg_rte_t *rte) const
{
	return rte->slot;
}

sg_rte::sg_rte()
{
}

sg_rte::~sg_rte()
{
}

bool sg_rte::venter()
{
	return true;
}

void sg_rte::vleave(sg_rte_t *rte)
{
}

bool sg_rte::vupdate(sg_rte_t *rte)
{
	return true;
}

bool sg_rte::vclean(pid_t, sg_rte_t *rte)
{
	return true;
}

// 获取保留节点
bool sg_rte::get_Rslot()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_rte_t *rte = link2ptr(SGC->_SGC_slot);
	bool retval = false;
	sg_proc_t tmp_proc;
	sg_proc& proc_mgr = sg_proc::instance(SGT);
#if defined(DEBUG)
	scoped_debug<bool> debug(350, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (rte->pid == SGC->_SGC_proc.pid) {
		SGC->_SGC_crte = rte;
		retval = true;
		return retval;
	}

	if (rte->pid > 0) {
		tmp_proc = SGC->_SGC_proc;
		tmp_proc.pid = rte->pid;
		if (proc_mgr.alive(tmp_proc)) {
			SGT->_SGT_error_code = SGEDUPENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to register because the slot is already owned by another process {1}") % rte->pid).str(SGLOCALE));
			return retval;
		}

		// 只有BBM可以做清理工作
		if (SGC->_SGC_slot == SGC->MAXACCSRS() + AT_BBM) {
			if (!clean(rte->pid, rte))
				return retval;
		} else {
			SGT->_SGT_error_code = SGEDUPENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to register because the slot is already owned by another process {1}") % rte->pid).str(SGLOCALE));
			return retval;
		}
	}

	SGC->_SGC_crte = rte;
	rte->slot = SGC->_SGC_slot;
	rte->pid = SGC->_SGC_proc.pid;
	rte->mid = SGC->_SGC_proc.mid;
	rte->qaddr = -1;
	if (strcmp(SGC->_SGC_authsw->name, "ADMIN") == 0) {
		rte->rflags = ADMIN;
		rte->hndl.numghandle = 0;
		rte->hndl.numshandle = 0;
		rte->hndl.gen = 0;
	} else if (strcmp(SGC->_SGC_authsw->name, "CLIENT") == 0) {
		rte->rflags = CLIENT;
		rte->hndl.numghandle = 0;
		rte->hndl.numshandle = 0;
		rte->hndl.gen = 0;
	} else {
		rte->rflags = SERVER;
		if ((SGP->_SGP_boot_flags & BF_RESTART) && !(SGP->_SGP_boot_flags & BF_MIGRATE)) {
			rte->hndl.numshandle += rte->hndl.numghandle;
			rte->hndl.numghandle = 0;
			rte->hndl.gen++;
		} else {
			rte->hndl.numghandle = 0;
			rte->hndl.numshandle = 0;
			rte->hndl.gen = 0;
		}
	}

	reset_slot(rte, time(0));
	retval = true;
	return retval;
}

// 重启进程获取它的原注册节点
bool sg_rte::get_oldslot()
{
	sgc_ctx *SGC = SGT->SGC();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(350, __PRETTY_FUNCTION__, "", &retval);
#endif

	sg_rte_t *rte = link2ptr(SGC->_SGC_bbmap->rte_use);
	for (; rte->nextreg != -1; rte = link2ptr(rte->nextreg)) {
		if (rte->pid == SGP->_SGP_oldpid && !(rte->rflags & SERVER_CTXT))
			break;
	}

	if (rte->pid != SGP->_SGP_oldpid || (rte->rflags & SERVER_CTXT)) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to obtain the registry slot owned by process")).str(SGLOCALE));
		return retval;
	}

	SGC->_SGC_slot = rte->slot;
	SGC->_SGC_crte = rte;

	if (!(rte->rflags & PROC_REST)) {
		SGT->_SGT_error_code = SGESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Attempt to take over registry table entry of non-restarting process")).str(SGLOCALE));
		return retval;
	}

	// 检查BB锁，直到被BBM清空
	if (SGT->_SGT_lock_type_by_me & EXCLUSIVE_BBLOCK) {
		sg_bboard_t *& bbp = SGC->_SGC_bbp;
		bi::interprocess_upgradable_mutex& mutex = bbp->bb_mutex;

		if (!mutex.try_lock()) {
			bool unlocked = false;

			if (SGT->_SGT_lock_type_by_me & EXCLUSIVE_SYSLOCK) {
				bbp->sys_mutex.unlock();
				unlocked = true;
			}

			while (!mutex.try_lock())
				boost::this_thread::yield();
			mutex.unlock();

			if (unlocked)
				bbp->sys_mutex.lock();
		} else {
			mutex.unlock();
		}
	}

	rte->pid = SGC->_SGC_proc.pid;
	rte->mid = SGC->_SGC_proc.mid;
	rte->rflags = SERVER;
	rte->qaddr = -1;
	rte->hndl.numshandle += rte->hndl.numghandle;
	rte->hndl.numghandle = 0;
	rte->hndl.gen++;
	reset_slot(rte, time(0));

	retval = true;
	return retval;
}

bool sg_rte::get_slot()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(350, __PRETTY_FUNCTION__, "", &retval);
#endif

	scoped_syslock syslock(SGT);

	if (bbmap->rte_free == -1)
		return retval;

	if ((SGC->_SGC_crte = get()) == NULL)
		return retval;

	retval = true;
	return retval;
}

void sg_rte::reset_slot(sg_rte_t *rte, long timestamp)
{
	if (timestamp == 0)
		rte->rt_timestamp++;
	else
		rte->rt_timestamp = timestamp;

	rte->rstatus = 0;
	rte->rreqmade = 0;
	rte->rt_svctimeout = 0;
	rte->hndl.qaddr = 0;
	rte->rt_release = SGPROTOCOL;
	rte->rt_svcexectime = 0;
	rte->rt_grpid = 0;
	rte->rt_svrid = 0;

 	for (int i = 0; i < MAXHANDLES; i++) {
		rte->hndl.bbscan_mtype[i] = 0;
		rte->hndl.bbscan_rplyiter[i] = 0;
		rte->hndl.bbscan_timeleft[i] = 0;
	}
}

// 从空闲列表获取一个节点
sg_rte_t * sg_rte::get()
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	sg_bbmeters_t& bbmeters = bbp->bbmeters;

	if (bbmap->rte_free == -1)
		return NULL;

	// 把节点从空闲列表移除
	sg_rte_t *rte = link2ptr(bbmap->rte_free);
	bbmap->rte_free = rte->nextreg;

	if (++bbmeters.caccsrs > bbmeters.maxaccsrs)
		bbmeters.maxaccsrs = bbmeters.caccsrs;

	// 加入到使用列表
	rte->nextreg = bbmap->rte_use;
	bbmap->rte_use = rte->slot;

	return rte;
}

// 释放节点到空闲列表
void sg_rte::put(sg_rte_t *rte)
{
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *& bbp = SGC->_SGC_bbp;
	sg_bbmap_t *bbmap = SGC->_SGC_bbmap;
	sg_bbmeters_t& bbmeters = bbp->bbmeters;

	// 把节点从使用列表移除
	sg_rte_t *up = link2ptr(bbmap->rte_use);
	if (up == rte) {
		bbmap->rte_use = rte->nextreg;
	} else {
		while (up && up->nextreg != rte->slot) {
			if (up->nextreg == -1)
				up = NULL;
			else
				up = link2ptr(up->nextreg);
		}
		if (up == NULL) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Tried to free non-alloced RTE")).str(SGLOCALE));
			return;
		}

		up->nextreg = rte->nextreg;
	}

	// 加入到空闲列表
	rte->nextreg = bbmap->rte_free;
	bbmap->rte_free = rte->slot;

	--bbmeters.caccsrs;
}

}
}

