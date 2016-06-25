#include "dbbm_rte.h"

namespace ai
{
namespace sg
{

bool dbbm_rte::venter()
{
	int i;
	sg_proc_t *proc;
	sg_ste_t *ste;
	sgc_ctx *SGC = SGT->SGC();
	sg_bboard_t *sbbp = SGC->_SGC_bbp;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", &retval);
#endif

	SGP->_SGP_amdbbm = true;
	SGC->_SGC_proc.mid = SGC->getmid();
	SGP->setids(SGC->_SGC_proc);

	int local = -1;
	int midx = SGC->midnidx(SGC->_SGC_proc.mid);
	int master[MAX_MASTERS];
	for (i = 0; i < MAX_MASTERS; i++) {
		master[i] = SGC->lmid2mid(sbbp->bbparms.master[i]);
		if (midx == SGC->midnidx(master[0]))
			local = i;
	}

	if (local == -1 || SGC->fmaster(master[local], NULL) != NULL){
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to register - another sgclst already exists on {1}")
			% sbbp->bbparms.master[local]).str(SGLOCALE));
		SGP->_SGP_amdbbm = false;
		return retval;
	}

	// Save pid to let other process find

	/* We want to avoid checking to see if there is a sgclst on the original master if the
	 * master node is down. Since the sgclst doesn't have access to the BB - (_SGC_bbp is pointing
	 * to its own table in memory), we have to attach to the shared memory to find out if the
	 * sgmngr in the master node is Partitioned.
	 */
	bool suspend = false;
	string shm_name = boost::lexical_cast<string>(SGC->midkey(local));
	try {
		bi::shared_memory_object mapping_mgr(bi::open_only, shm_name.c_str(), bi::read_write);

		size_t bbsize = sg_bboard_t::bbsize(sbbp->bbparms);
		bi::mapped_region region_mgr(mapping_mgr, bi::read_write, 0, bbsize, NULL);

		sg_bboard_t *real_bbp = reinterpret_cast<sg_bboard_t *>(region_mgr.get_address());
		sg_ste& ste_mgr = sg_ste::instance(SGT);
		int bucket = ste_mgr.hash_value(MNGR_PRCID);
		int *ste_hash = reinterpret_cast<int *>(reinterpret_cast<char *>(real_bbp) + real_bbp->bbmap.ste_hash);
		for (int index = ste_hash[bucket]; index != -1; index = ste->hashlink.fhlink) {
			ste = reinterpret_cast<sg_ste_t *>(reinterpret_cast<char *>(real_bbp) + real_bbp->bbmap.ste_root) + index;
			if (ste->grpid() == CLST_PGID && ste->mid() != master[local] && (ste->global.status & PSUSPEND)) {
				suspend = true;
				break;
			}
		}
	} catch (bi::interprocess_exception& ex) {
		// ºöÂÔ´íÎó
	}

	if (!suspend) {
		for (i = 0; i < MAX_MASTERS; i++) {
			if (local == i)
				continue;

			if ((proc = SGC->fmaster(master[i], NULL)) != NULL
				&& proc->pid != SGC->_SGC_proc.pid) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unable to register - another sgclst already exists on {1}")
					% sbbp->bbparms.master[i]).str(SGLOCALE));
				SGP->_SGP_amdbbm = false;
				return retval;
			}
		}
	}

	// register in newly aquired BB using reserved slot
	SGC->_SGC_slot = SGC->MAXACCSRS() + AT_BBM;
	smenter();

	retval = true;
	return retval;
}

dbbm_rte::dbbm_rte()
{
	BBM = bbm_ctx::instance();
}

dbbm_rte::~dbbm_rte()
{
}

}
}

