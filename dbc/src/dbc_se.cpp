#include "dbc_internal.h"

namespace ai
{
namespace sg
{

void dbc_se_t::init()
{
	seq_name[0] = '\0';
	flags = 0;
	accword = 0;
	minval = 0;
	maxval = numeric_limits<long>::max();
	currval = 0;
	increment = 1;
	cache = 1;
	cycle = false;
	order = false;
	prev = -1;
	next = -1;
}

dbc_se_t& dbc_se_t::operator=(const dbc_se_t& rhs)
{
	strcpy(seq_name, rhs.seq_name);
	minval = rhs.minval;
	maxval = rhs.maxval;
	currval = rhs.currval;
	increment = rhs.increment;
	cache = rhs.cache;
	cycle = rhs.cycle;
	order = rhs.order;

	return *this;
}

dbc_se& dbc_se::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<dbc_se *>(DBCT->_DBCT_mgr_array[DBC_SE_MANAGER]);
}

void dbc_se::set_ctx(dbc_se_t *se_)
{
	se = se_;
}

dbc_se_t * dbc_se::get_se()
{
	return se;
}

long dbc_se::get_currval(const string& seq_name)
{
	dbct_ctx *DBCT = dbct_ctx::instance();
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(100, __PRETTY_FUNCTION__, (_("seq_name={1}") % seq_name).str(SGLOCALE), &retval);
#endif

	boost::unordered_map<string, seq_cache_t>::const_iterator iter = DBCT->_DBCT_seq_map.find(seq_name);
	if (iter == DBCT->_DBCT_seq_map.end()) {
		DBCT->_DBCT_error_code = DBCESEQNOTDEFINE;
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: sequence {1} is not yet defined in this session.") % seq_name).str(SGLOCALE));
	}

	retval = iter->second.currval;
	return retval;
}

long dbc_se::get_nextval(const string& seq_name)
{
	dbct_ctx *DBCT = dbct_ctx::instance();
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(100, __PRETTY_FUNCTION__, (_("seq_name={1}") % seq_name).str(SGLOCALE), &retval);
#endif

	dbc_se& se_mgr = dbc_se::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_se_t *se = ue_mgr.get_seq(seq_name);
	if (se == NULL) {
		DBCT->_DBCT_error_code = DBCENOSEQ;
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: sequence {1} does not exist.") % seq_name).str(SGLOCALE));
	}

	se_mgr.set_ctx(se);

	boost::unordered_map<string, seq_cache_t>::iterator iter = DBCT->_DBCT_seq_map.find(seq_name);
	if (iter == DBCT->_DBCT_seq_map.end() || (iter->second.cache == 1 && iter->second.refetch)) {
		seq_cache_t seq_cache;

		seq_cache.currval = se_mgr.get_nextval();
		seq_cache.cache = se->cache;
		seq_cache.refetch = false;
		DBCT->_DBCT_seq_map[seq_name] = seq_cache;
		retval = seq_cache.currval;
	} else {
		seq_cache_t& seq_cache = iter->second;

		if (seq_cache.refetch) {
			long nextval = seq_cache.currval + se->increment;
			if (nextval > se->maxval) {
				if (!se->cycle) {
					DBCT->_DBCT_error_code = DBCESEQOVERFLOW;
					throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: sequence {1} overflow.") % seq_name).str(SGLOCALE));
				}
				seq_cache.currval = se->minval + ((nextval - se->minval) % (se->maxval - se->minval));
			} else {
				seq_cache.currval = nextval;
			}
			seq_cache.cache--;
		}

		retval = seq_cache.currval;
	}

	return retval;
}

long dbc_se::get_nextval()
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif
	ipc_sem::tas(&se->accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&se)) {
		ipc_sem::semclear(&se->accword);
	} BOOST_SCOPE_EXIT_END

	long currval = se->currval;
	long nextval = se->currval + se->cache * se->increment;
	if (nextval > se->maxval) {
		if (!se->cycle) {
			DBCT->_DBCT_error_code = DBCESEQOVERFLOW;
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: sequence {1} overflow.") % se->seq_name).str(SGLOCALE));
		}
		se->currval = se->minval + ((nextval - se->minval) % (se->maxval - se->minval));
	} else {
		se->currval = nextval;
	}

	if (DBCT->_DBCT_seq_fd == -2) { // First time
		dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
		dbc_bbparms_t& bbparms = bbp->bbparms;
		string file_name = bbparms.sys_dir;
		file_name += "sequence.dat";
		DBCT->_DBCT_seq_fd = open(file_name.c_str(), O_RDWR);
		if (DBCT->_DBCT_seq_fd == -1) {
			DBCT->_DBCT_error_code = DBCEOS;
			DBCT->_DBCT_native_code = UOPEN;
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't open file {1} - {2}") % file_name % ::strerror(errno)).str(SGLOCALE));
		} else {
			long offset = sizeof(long) * (se -DBCT->_DBCT_ses);
			if (pwrite(DBCT->_DBCT_seq_fd, &se->currval, sizeof(long), offset) == -1) {
				DBCT->_DBCT_error_code = DBCEOS;
				DBCT->_DBCT_native_code = UWRITE;
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't write file {1} - {2}") % file_name % ::strerror(errno)).str(SGLOCALE));
			}
		}
	} else if (DBCT->_DBCT_seq_fd >= 0) {
		long offset = sizeof(long) * (se -DBCT->_DBCT_ses);
		if (pwrite(DBCT->_DBCT_seq_fd, &se->currval, sizeof(long), offset) == -1) {
			DBCT->_DBCT_error_code = DBCEOS;
			DBCT->_DBCT_native_code = UWRITE;
			dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
			dbc_bbparms_t& bbparms = bbp->bbparms;
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't write file {1}sequence.dat - {2}") % bbparms.sys_dir % ::strerror(errno)).str(SGLOCALE));
		}
	}

	retval = currval;
	return retval;
}

dbc_se::dbc_se()
{
}

dbc_se::~dbc_se()
{
}

}
}

