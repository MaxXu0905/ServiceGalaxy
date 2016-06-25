#include "sg_internal.h"

namespace ai
{
namespace sg
{

int sg_hashlink_t::get_index() const
{
	return static_cast<int>(sgid & SGID_IDXMASK);
}

void sg_hashlink_t::make_sgid(long type, const void *entry)
{
	sgt_ctx *SGT = sgt_ctx::instance();
	sgc_ctx *SGC = SGT->SGC();
	const sg_hashlink_t *hashlink = reinterpret_cast<const sg_hashlink_t *>(entry);
	int idx = hashlink->rlink;
	boost::crc_32_type crc;

	switch (type) {
	case SGID_QUET:
		if (idx < SGC->MAXQUES()) {
			const sg_qte_t *qte = reinterpret_cast<const sg_qte_t *>(entry);
			const char *value = qte->parms.saddr;
			int len = strlen(value);
			crc = std::for_each(value, value + len, crc);
		}
		break;
	case SGID_SGT:
		if (idx < SGC->MAXSGT()) {
			const sg_sgte_t *sgte = reinterpret_cast<const sg_sgte_t *>(entry);
			const char *value = sgte->parms.sgname;
			int len = strlen(value);
			crc = std::for_each(value, value + len, crc);
		}
		break;
	case SGID_SVRT:
		if (idx < SGC->MAXSVRS()) {
			const sg_ste_t *ste = reinterpret_cast<const sg_ste_t *>(entry);
			const char *value = reinterpret_cast<const char *>(&ste->grpid());
			crc = std::for_each(value, value + sizeof(ste->grpid()), crc);
			value = reinterpret_cast<const char *>(&ste->svrid());
			crc = std::for_each(value, value + sizeof(ste->svrid()), crc);
		}
		break;
	case SGID_SVCT:
		if (idx < SGC->MAXSVCS()) {
			const sg_scte_t *scte = reinterpret_cast<const sg_scte_t *>(entry);
			const char *value = scte->parms.svc_name;
			int len = strlen(value);
			crc = std::for_each(value, value + len, crc);
		}
		break;
	}

	sgid = (static_cast<long>(crc()) << SGID_CRCSHIFT) | type | static_cast<long>(idx);
}

int sg_hashlink_t::get_index(sgid_t sgid)
{
	return static_cast<int>(sgid & SGID_IDXMASK);
}

ostream& operator<<(ostream& os, const sg_proc_t& proc)
{
	os << "proc: [" << proc.mid
		<< ", " << proc.grpid
		<< ", " << proc.svrid
		<< ", " << proc.pid
		<< ", " << proc.rqaddr
		<< ", " << proc.rpaddr
		<< "]";

	return os;
}

void sg_bbmeters_t::refresh_data(const sg_bbmeters_t& bbmeters)
{
	status = bbmeters.status;
	currload = bbmeters.currload;
	cques = bbmeters.cques;
	csvrs = bbmeters.csvrs;
	csvcs = bbmeters.csvcs;
	csgt = bbmeters.csgt;
	maxmachines = bbmeters.maxmachines;
	maxques = bbmeters.maxques;
	maxsvrs = bbmeters.maxsvrs;
	maxsvcs = bbmeters.maxsvcs;
	maxsgt = bbmeters.maxsgt;
	lanversion = bbmeters.lanversion;
}

sg_bboard_t::sg_bboard_t(const sg_bbparms_t& bbparms_)
{

	bbparms = bbparms_;
	bblock_pid = 0;
	syslock_pid = 0;
	memset(&bbmeters, '\0', sizeof(bbmeters));

	bbmap.pte_root = sg_roundup(sizeof(sg_bboard_t), LONG_ALIGN);
	bbmap.nte_root = sg_roundup(bbmap.pte_root + sizeof(sg_pte_t) * bbparms.maxpes, LONG_ALIGN);
	bbmap.qte_root = sg_roundup(bbmap.nte_root + sizeof(sg_nte_t) * bbparms.maxnodes, LONG_ALIGN);
	bbmap.sgte_root = sg_roundup(bbmap.qte_root + sizeof(sg_qte_t) * bbparms.maxques, LONG_ALIGN);
	bbmap.ste_root = sg_roundup(bbmap.sgte_root + sizeof(sg_sgte_t) * bbparms.maxsgt, LONG_ALIGN);
	bbmap.scte_root = sg_roundup(bbmap.ste_root + sizeof(sg_ste_t) * bbparms.maxsvrs, LONG_ALIGN);

	bbmap.qte_hash = sg_roundup(bbmap.scte_root + sizeof(sg_scte_t) * bbparms.maxsvcs, LONG_ALIGN);
	bbmap.sgte_hash = sg_roundup(bbmap.qte_hash + sizeof(int) * bbparms.quebkts, LONG_ALIGN);
	bbmap.ste_hash = sg_roundup(bbmap.sgte_hash + sizeof(int) * bbparms.sgtbkts, LONG_ALIGN);
	bbmap.scte_hash = sg_roundup(bbmap.ste_hash + sizeof(int) * bbparms.svrbkts, LONG_ALIGN);

	bbmap.rte_root = sg_roundup(bbmap.scte_hash + sizeof(int) * bbparms.svcbkts, LONG_ALIGN);
	bbmap.misc_root = sg_roundup(bbmap.rte_root + sizeof(sg_rte_t) * (bbparms.maxaccsrs + MAXADMIN), LONG_ALIGN);

	bbmap.qte_free = 0;
	bbmap.sgte_free = 0;
	bbmap.ste_free = 0;
	bbmap.scte_free = 0;
	bbmap.rte_free = 0;
	bbmap.rte_use = -1;
}

sg_bboard_t::~sg_bboard_t()
{
}

size_t sg_bboard_t::bbsize(const sg_bbparms_t& bbparms)
{
	return sg_roundup(sizeof(sg_bboard_t), LONG_ALIGN)
		+ sg_roundup(sizeof(sg_pte_t) * bbparms.maxpes, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_nte_t) * bbparms.maxnodes, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_qte_t) * bbparms.maxques, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_sgte_t) * bbparms.maxsgt, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_ste_t) * bbparms.maxsvrs, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_scte_t) * bbparms.maxsvcs, LONG_ALIGN)
		+ sg_roundup(sizeof(int) * bbparms.quebkts, LONG_ALIGN)
 		+ sg_roundup(sizeof(int) * bbparms.sgtbkts, LONG_ALIGN)
		+ sg_roundup(sizeof(int) * bbparms.svrbkts, LONG_ALIGN)
		+ sg_roundup(sizeof(int) * bbparms.svcbkts, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_rte_t) * (bbparms.maxaccsrs + MAXADMIN), LONG_ALIGN)
		+ sg_roundup(sizeof(sg_misc_t) + sizeof(sg_qitem_t)
			* (bbparms.maxsvrs * 2 + bbparms.maxaccsrs + MAXADMIN - 1), LONG_ALIGN);
}

size_t sg_bboard_t::pbbsize(const sg_bbparms_t& bbparms)
{
	return sg_roundup(sizeof(sg_bboard_t), LONG_ALIGN) - offsetof(sg_bboard_t, bbmeters)
		+ sg_roundup(sizeof(sg_pte_t) * bbparms.maxpes, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_nte_t) * bbparms.maxnodes, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_qte_t) * bbparms.maxques, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_sgte_t) * bbparms.maxsgt, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_ste_t) * bbparms.maxsvrs, LONG_ALIGN)
		+ sg_roundup(sizeof(sg_scte_t) * bbparms.maxsvcs, LONG_ALIGN)
		+ sg_roundup(sizeof(int) * bbparms.quebkts, LONG_ALIGN)
 		+ sg_roundup(sizeof(int) * bbparms.sgtbkts, LONG_ALIGN)
		+ sg_roundup(sizeof(int) * bbparms.svrbkts, LONG_ALIGN)
		+ sg_roundup(sizeof(int) * bbparms.svcbkts, LONG_ALIGN);
}

sg_lqueue_t::sg_lqueue_t()
{
	nqueued = 0;
	wkqueued = 0;
}

sg_gqueue_t::sg_gqueue_t()
{
	status = IDLE;
	clink = -1;
	svrcnt = 0;
	spawn_state = 0;
}

sg_qte_t::sg_qte_t()
{
}

sg_qte_t::sg_qte_t(int rlink)
{
	hashlink.rlink = rlink;
}

sg_qte_t::sg_qte_t(const sg_queparms_t& parms_)
{
	parms = parms_;
}

sg_qte_t::~sg_qte_t()
{
}

sg_sgte_t::sg_sgte_t()
{
}

sg_sgte_t::sg_sgte_t(int rlink)
{
	hashlink.rlink = rlink;
}

sg_sgte_t::sg_sgte_t(const sg_sgparms_t& parms_)
{
	parms = parms_;
}

sg_sgte_t::~sg_sgte_t()
{
}

sg_lserver_t::sg_lserver_t()
{
	total_idle = 0;
	total_busy = 0;
	ncompleted = 0;
	wkcompleted = 0;
}

sg_gserver_t::sg_gserver_t()
{
	ctime = time(0);
	status &= SVCTIMEDOUT;
	ctime = time(0);
	srelease = 0;
	gen = 1;
}

sg_ste_t::sg_ste_t()
{
}

sg_ste_t::sg_ste_t(int rlink)
{
	hashlink.rlink = rlink;
}

sg_ste_t::sg_ste_t(const sg_svrparms_t& parms)
{
	global.maxmtype = MAXBCSTMTYP;
	global.ctime = parms.ctime;
	global.mtime = parms.ctime;
	global.svridmax = parms.svridmax;
	global.status &= SVCTIMEDOUT;
	global.status |= IDLE;
	global.gen = 1;
}

sg_ste_t::~sg_ste_t()
{
}

bool sg_ste_t::operator==(const sg_ste_t& rhs) const
{
	return svrproc() == rhs.svrproc();
}

sg_lservice_t::sg_lservice_t()
{
	ncompleted = 0;
	nqueued = 0;
}

sg_gservice_t::sg_gservice_t()
{
}

sg_scte_t::sg_scte_t()
{
}

sg_scte_t::sg_scte_t(int rlink)
{
	global.status = IDLE;
	hashlink.rlink = rlink;
}

sg_scte_t::sg_scte_t(const sg_svcparms_t& parms_)
{
	parms = parms_;
}

sg_scte_t::~sg_scte_t()
{
}

sg_rte_t::sg_rte_t(int slot_)
{
	slot = slot_;
	pid = 0;
	rstatus = 0;
	lock_type = UNLOCK;
}

sg_rte_t::~sg_rte_t()
{
}

size_t hash_value(const sg_queue_key_t& item)
{
	size_t seed = boost::hash_value(item.mid);
	boost::hash_combine(seed, item.qid);

	return seed;
}

sg_init_t::sg_init_t()
{
	usrname[0] = '\0';
	cltname[0] = '\0';
	passwd[0] = '\0';
	grpname[0] = '\0';
	flags = 0;
}

}
}

