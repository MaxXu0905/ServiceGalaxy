#include "dbc_internal.h"

namespace bp = boost::posix_time;

namespace ai
{
namespace sg
{

cursor_compare::cursor_compare(char *data_start_, compiler::func_type order_func_)
	: data_start(data_start_),
	  order_func(order_func_)
{
}

bool cursor_compare::operator()(long left, long right) const
{
	char *global[MAX_GI];
	global[GI_ROW_DATA] = data_start + left;
	global[GI_ROW_DATA2] = data_start + right;

	return (order_func(NULL, global, NULL, NULL) < 0);
}

dbc_cursor::dbc_cursor(dbct_ctx *DBCT_, vector<long> *rowids_, dbc_te_t *te_)
	: DBCT(DBCT_),
	  rowids(rowids_),
	  te(te_)
{
	cursor = -1;

	if (!te->in_flags(TE_VARIABLE))
		unmarshal_buf = NULL;
	else
		unmarshal_buf = new char[te->te_meta.struct_size];
}

dbc_cursor::~dbc_cursor()
{
	for (vector<long>::const_iterator iter = rowids->begin(); iter != rowids->end(); ++iter) {
		row_link_t *link = DBCT->rowid2link(*iter);
		link->dec_ref();
	}
	delete rowids;
	delete []unmarshal_buf;
}

bool dbc_cursor::next()
{
	if (++cursor == rowids->size())
		return false;

	return true;
}

bool dbc_cursor::prev()
{
	if (cursor < 0)
		return false;

	cursor--;
	return true;
}

bool dbc_cursor::status()
{
	return (cursor >= 0 && cursor < rowids->size());
}

bool dbc_cursor::seek(int cursor_)
{
	if (cursor_ < 0 || cursor_ >= rowids->size())
		return false;

	cursor = cursor_;
	return true;
}

const char * dbc_cursor::data() const
{
	char *row_data = DBCT->rowid2data((*rowids)[cursor]);
	if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal) {
		return row_data;
	} else {
		dbc_te& te_mgr = dbc_te::instance(DBCT);
		te_mgr.set_ctx(te);
		te_mgr.unmarshal(unmarshal_buf, row_data);
		return unmarshal_buf;
	}
}

long dbc_cursor::rowid() const
{
	return (*rowids)[cursor];
}

long dbc_cursor::size() const
{
	return rowids->size();
}

void dbc_cursor::order(compiler::func_type order_func)
{
	std::sort(rowids->begin(), rowids->end(), cursor_compare(DBCT->_DBCT_data_start, order_func));
}

dbc_api& dbc_api::instance()
{
	dbct_ctx *DBCT = dbct_ctx::instance();
	return *reinterpret_cast<dbc_api *>(DBCT->_DBCT_mgr_array[DBC_API_MANAGER]);
}

dbc_api& dbc_api::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<dbc_api *>(DBCT->_DBCT_mgr_array[DBC_API_MANAGER]);
}

bool dbc_api::login()
{
	scoped_usignal sig;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (DBCT->_DBCT_login) {
		retval = true;
		return retval;
	}

	if (!DBCT->attach_shm())
		return retval;

	DBCT->init_globals();

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;

	// Wait for server initialization.
	{
		dbc_rte_t *rte = DBCT->_DBCT_rtes + RTE_SLOT_SERVER;
		bi::scoped_lock<bi::interprocess_mutex> lock(bbmeters.load_mutex);

		while (!bbmeters.load_ready) {
			if (!(rte->flags & RTE_IN_USE) || kill(rte->pid, 0) == -1) {
				DBCT->_DBCT_error_code = DBCENOSERVER;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: dbc_server isn't alive.")).str(SGLOCALE));
				return retval;
			}

			bp::ptime expire(bp::second_clock::universal_time() + bp::seconds(1));
			bbmeters.load_cond.timed_wait(lock, expire);
		}

		if (!(rte->flags & RTE_IN_USE) || kill(rte->pid, 0) == -1) {
			DBCT->_DBCT_error_code = DBCENOSERVER;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: dbc_server isn't alive.")).str(SGLOCALE));
			return retval;
		}
	}

	if (!rte_mgr.get_slot()) {
		if (DBCP->_DBCP_is_sync) {
			if (DBCT->_DBCT_error_code == DBCEDUPENT)
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: RTE is owned by another process.")).str(SGLOCALE));
			else if (DBCT->_DBCT_error_code == DBCENOSERVER)
				GPP->write_log(__FILE__, DBCESYSTEM, (_("ERROR: dbc_sync is not alive.")).str(SGLOCALE));
		}
		return retval;
	}

	DBCT->_DBCT_login = true;

	signal(bbparms.post_signo, dbct_ctx::bb_change);

	if (!DBCT->_DBCT_atexit_registered) {
		atexit(logout_atexit);
		DBCT->_DBCT_atexit_registered = true;
	}

	GPP->write_log((_("INFO: client application login DBC successfully.")).str(SGLOCALE));
	retval = true;
	return retval;
}

void dbc_api::logout()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!DBCT->_DBCT_login)
		return;

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	signal(bbparms.post_signo, SIG_DFL);

	if (connected()) {
		rollback();
		disconnect();
	}

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	rte_mgr.set_flags(RTE_MARK_REMOVED);
	rte_mgr.put_slot();

	DBCT->detach_shm();
	DBCT->_DBCT_login = false;

	GPP->write_log((_("INFO: client application logout DBC successfully.")).str(SGLOCALE));
}

bool dbc_api::connect(const string& user_name, const string& password)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, (_("user_name={1}, password={2}") % user_name % password).str(SGLOCALE), &retval);
#endif
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);

	if (!ue_mgr.connect(user_name, password)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: permission denied, user name or password not correct.")).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

bool dbc_api::connected() const
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", &retval);
#endif
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);

	retval = ue_mgr.connected();
	return retval;
}

void dbc_api::disconnect()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", NULL);
#endif
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);

	ue_mgr.disconnect();
}

bool dbc_api::set_user(int64_t user_id)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(600, __PRETTY_FUNCTION__, (_("user_id={1}") % user_id).str(SGLOCALE), &retval);
#endif
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);

	int user_idx = (user_id & UE_ID_MASK);
	if (user_idx < 0 || user_idx >= bbparms.maxusers) {
		DBCT->_DBCT_error_code = SGEINVAL;
		return retval;
	}

	dbc_ue_t *ue = DBCT->_DBCT_ues + user_idx;
	if (!ue->in_flags(UE_IN_USE)) {
		DBCT->_DBCT_error_code = SGENOENT;
		return retval;
	}

	if (ue->user_id != user_id) {
		DBCT->_DBCT_error_code = SGEPERM;
		return retval;
	}

	ue_mgr.set_ctx(ue);
	retval = true;
	return retval;
}

int64_t dbc_api::get_user() const
{
	int64_t retval = -1;
#if defined(DEBUG)
	scoped_debug<int64_t> debug(600, __PRETTY_FUNCTION__, "", &retval);
#endif

	retval = DBCT->_DBCT_user_id;
	return retval;
}

void dbc_api::set_ctx(dbc_rte_t *rte)
{
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *crte = rte_mgr.get_rte();
#if defined(DEBUG)
	scoped_debug<bool> debug(600, __PRETTY_FUNCTION__, (_("rte={1}") % rte).str(SGLOCALE), NULL);
#endif

	crte->set_flags(RTE_IN_SUSPEND);
	rte->clear_flags(RTE_IN_SUSPEND);
	rte_mgr.set_ctx(rte);
}

int dbc_api::get_table(const string& table_name)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("table_name={1}") % table_name).str(SGLOCALE), &retval);
#endif

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.find_te(table_name);
	if (te == NULL) {
		DBCT->_DBCT_error_code = DBCENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: table {1} not exist.") % table_name).str(SGLOCALE));
		return retval;
	}

	retval = te->table_id;
	return retval;
}

int dbc_api::get_index(const string& table_name, const string& index_name)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(300, __PRETTY_FUNCTION__, (_("table_name={1}, index_name={2}") % table_name % index_name).str(SGLOCALE), &retval);
#endif

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.find_te(table_name);
	if (te == NULL) {
		DBCT->_DBCT_error_code = DBCENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: table {1} not exist.") % table_name).str(SGLOCALE));
		return retval;
	}

	te_mgr.set_ctx(te);

	for (int i = 0; i <= te->max_index; i++) {
		dbc_ie_t *ie = te_mgr.get_index(i);
		if (ie == NULL)
			continue;

		if (strcasecmp(ie->ie_meta.index_name, index_name.c_str()) == 0) {
			retval = ie->index_id;
			return retval;
		}
	}

	DBCT->_DBCT_error_code = DBCENOENT;
	GPP->write_log(__FILE__, __LINE__, (_("ERROR: index {1} on table {2} not exist.") % index_name % table_name).str(SGLOCALE));
	return retval;
}

void dbc_api::set_rownum(long rownum)
{
	DBCT->_DBCT_rownum = rownum;
}

long dbc_api::get_rownum() const
{
	return DBCT->_DBCT_rownum;
}

long dbc_api::find(int table_id, const void *data, void *result, long max_rows)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_data_t& te_meta = te->te_meta;
	dbc_stat stat(bbparms.stat_level, te->stat_array[TE_FIND_STAT]);
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}, result={3}, max_rows={4}") % table_id % data % result % max_rows).str(SGLOCALE), &retval);
#endif

	// 设置最大返回记录数
	long saved_max_rows = DBCT->_DBCT_rownum;
	BOOST_SCOPE_EXIT((&DBCT)(&saved_max_rows)) {
		DBCT->_DBCT_rownum = saved_max_rows;
	} BOOST_SCOPE_EXIT_END
	DBCT->_DBCT_rownum = max_rows;

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_SELECT)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	te_mgr.set_ctx(te);

	DBCT->_DBCT_rowids->clear();
	DBCT->clear_for_update();
	te_mgr.find_row(data);

	for (vector<long>::const_iterator iter = DBCT->_DBCT_rowids->begin(); iter != DBCT->_DBCT_rowids->end(); ++iter) {
		long rowid = *iter;
		char *row_data = DBCT->rowid2data(rowid);

		if (!te->in_flags(TE_VARIABLE))
			memcpy(result, row_data, te_meta.struct_size);
		else
			te_mgr.unmarshal(result, row_data);

		result = reinterpret_cast<char *>(result) + te_meta.struct_size;
		te_mgr.unref_row(rowid);
	}

	retval = DBCT->_DBCT_rowids->size();
	return retval;
}

long dbc_api::find(int table_id, int index_id, const void *data, void *result, long max_rows)
{
	scoped_usignal sig;
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, data={3}, result={4}, max_rows={5}") % table_id % index_id % data % result % max_rows).str(SGLOCALE), &retval);
#endif

	// 设置最大返回记录数
	long saved_max_rows = DBCT->_DBCT_rownum;
	BOOST_SCOPE_EXIT((&DBCT)(&saved_max_rows)) {
		DBCT->_DBCT_rownum = saved_max_rows;
	} BOOST_SCOPE_EXIT_END
	DBCT->_DBCT_rownum = max_rows;

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_SELECT)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_data_t& te_meta = te->te_meta;
	te_mgr.set_ctx(te);

	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_ie_t *ie = te_mgr.get_index(index_id);
	if (ie == NULL) {
		DBCT->_DBCT_error_code = DBCENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: wrong index_id {1} given.") % index_id).str(SGLOCALE));
		return retval;
	}

	dbc_stat stat(bbparms.stat_level, ie->stat_array[(ie->ie_meta.method_type & METHOD_TYPE_HASH) ? IE_FIND_HASH_STAT : IE_FIND_STAT]);
	ie_mgr.set_ctx(ie);

	const void *data_buf;
	if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal || data == NULL)
		data_buf = data;
	else
		data_buf = te_mgr.marshal(data);

	DBCT->_DBCT_rowids->clear();
	DBCT->clear_for_update();
	ie_mgr.find_row(data_buf);

	for (vector<long>::const_iterator iter = DBCT->_DBCT_rowids->begin(); iter != DBCT->_DBCT_rowids->end(); ++iter) {
		long rowid = *iter;
		char *row_data = DBCT->rowid2data(rowid);

		if (!te->in_flags(TE_VARIABLE))
			memcpy(result, row_data, te_meta.struct_size);
		else
			te_mgr.unmarshal(result, row_data);

		result = reinterpret_cast<char *>(result) + te_meta.struct_size;
		te_mgr.unref_row(rowid);
	}

	retval = DBCT->_DBCT_rowids->size();
	return retval;
}

auto_ptr<dbc_cursor> dbc_api::find(int table_id, const void *data)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_stat stat(bbparms.stat_level, te->stat_array[TE_FIND_STAT]);
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}") % table_id % data).str(SGLOCALE), NULL);
#endif

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_SELECT)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		throw sg_exception(__FILE__, __LINE__, DBCEPERM, 0, (_("ERROR: permission denied, current user is not allowed to select table.")).str(SGLOCALE));
	}

	te_mgr.set_ctx(te);

	// 分配的内存有dbc_cursor释放
	DBCT->_DBCT_rowids = new vector<long>();
	auto_ptr<dbc_cursor> cursor(new dbc_cursor(DBCT, DBCT->_DBCT_rowids, te));
	DBCT->clear_for_update();
	te_mgr.find_row(data);
	DBCT->_DBCT_rowids = &DBCT->_DBCT_rowids_buf;
	return cursor;
}

auto_ptr<dbc_cursor> dbc_api::find(int table_id, int index_id, const void *data)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, data={3}") % table_id % index_id % data).str(SGLOCALE), NULL);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_SELECT)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		throw sg_exception(__FILE__, __LINE__, DBCEPERM, 0, (_("ERROR: permission denied, current user is not allowed to select table.")).str(SGLOCALE));
	}

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	te_mgr.set_ctx(te);

	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_ie_t *ie = te_mgr.get_index(index_id);
	if (ie == NULL) {
		DBCT->_DBCT_error_code = DBCENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: wrong index_id {1} given.") % index_id).str(SGLOCALE));
	}

	dbc_stat stat(bbparms.stat_level, ie->stat_array[(ie->ie_meta.method_type & METHOD_TYPE_HASH) ? IE_FIND_HASH_STAT : IE_FIND_STAT]);
	ie_mgr.set_ctx(ie);

	// 分配的内存有dbc_cursor释放
	const void *data_buf;
	if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal || data == NULL)
		data_buf = data;
	else
		data_buf = te_mgr.marshal(data);

	DBCT->_DBCT_rowids = new vector<long>();
	auto_ptr<dbc_cursor> cursor(new dbc_cursor(DBCT, DBCT->_DBCT_rowids, te));
	DBCT->clear_for_update();
	ie_mgr.find_row(data_buf);

	DBCT->_DBCT_rowids = &DBCT->_DBCT_rowids_buf;
	return cursor;
}

long dbc_api::find_by_rowid(int table_id, long rowid, void *result, long max_rows)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_data_t& te_meta = te->te_meta;
	dbc_stat stat(bbparms.stat_level, te->stat_array[TE_FIND_ROWID_STAT]);
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, rowid={2}, result={3}, max_rows={4}") % table_id % rowid % result % max_rows).str(SGLOCALE), &retval);
#endif

	// 设置最大返回记录数
	long saved_max_rows = DBCT->_DBCT_rownum;
	BOOST_SCOPE_EXIT((&DBCT)(&saved_max_rows)) {
		DBCT->_DBCT_rownum = saved_max_rows;
	} BOOST_SCOPE_EXIT_END
	DBCT->_DBCT_rownum = max_rows;

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_SELECT)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	te_mgr.set_ctx(te);

	DBCT->_DBCT_rowids->clear();
	DBCT->clear_for_update();
	te_mgr.find_row(rowid);

	for (vector<long>::const_iterator iter = DBCT->_DBCT_rowids->begin(); iter != DBCT->_DBCT_rowids->end(); ++iter) {
		long rowid = *iter;
		char *row_data = DBCT->rowid2data(rowid);

		if (!te->in_flags(TE_VARIABLE))
			memcpy(result, row_data, te_meta.struct_size);
		else
			te_mgr.unmarshal(result, row_data);

		result = reinterpret_cast<char *>(result) + te_meta.struct_size;
		te_mgr.unref_row(rowid);
	}

	retval = DBCT->_DBCT_rowids->size();
	return retval;
}

auto_ptr<dbc_cursor> dbc_api::find_by_rowid(int table_id, long rowid)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_stat stat(bbparms.stat_level, te->stat_array[TE_FIND_ROWID_STAT]);
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, rowid={2}") % table_id % rowid).str(SGLOCALE), NULL);
#endif

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_SELECT)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		throw sg_exception(__FILE__, __LINE__, DBCEPERM, 0, (_("ERROR: permission denied, current user is not allowed to select table.")).str(SGLOCALE));
	}

	te_mgr.set_ctx(te);

	// 分配的内存由dbc_cursor释放
	DBCT->_DBCT_rowids = new vector<long>();
	auto_ptr<dbc_cursor> cursor(new dbc_cursor(DBCT, DBCT->_DBCT_rowids, te));
	DBCT->clear_for_update();
	te_mgr.find_row(rowid);
	DBCT->_DBCT_rowids = &DBCT->_DBCT_rowids_buf;
	return cursor;
}

long dbc_api::erase(int table_id, const void *data)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_stat stat(bbparms.stat_level, te->stat_array[TE_ERASE_STAT]);
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}") % table_id % data).str(SGLOCALE), &retval);
#endif

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_DELETE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	if (te->in_flags(TE_READONLY)) {
		DBCT->_DBCT_error_code = DBCEREADONLY;
		return retval;
	}

	te_mgr.set_ctx(te);

	// 查找所有数据区中的数据
	DBCT->_DBCT_rowids->clear();
	DBCT->set_for_update();
	te_mgr.find_row(data);

	if (DBCT->_DBCT_rowids->empty()) {
		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		if (rte_mgr.in_flags(RTE_ABORT_ONLY)) {
			DBCT->_DBCT_error_code = DBCETIME;
			return retval;
		}
		retval = 0;
		return retval;
	}

	// 删除所有数据区中的数据
	long rows = 0;
	for (vector<long>::const_iterator iter = DBCT->_DBCT_rowids->begin(); iter != DBCT->_DBCT_rowids->end(); ++iter) {
		if (erase_by_rowid_internal(table_id, *iter))
			rows++;
	}

	if (get_autocommit() && !commit()) {
		rollback();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: failed to commit transaction.")).str(SGLOCALE));
		return retval;
	}

	retval = rows;
	return retval;
}

long dbc_api::erase(int table_id, int index_id, const void *data)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, data={3}") % table_id % index_id % data).str(SGLOCALE), &retval);
#endif

	// 设置最大返回记录数
	long saved_max_rows = DBCT->_DBCT_rownum;
	BOOST_SCOPE_EXIT((&DBCT)(&saved_max_rows)) {
		DBCT->_DBCT_rownum = saved_max_rows;
	} BOOST_SCOPE_EXIT_END
	DBCT->_DBCT_rownum = std::numeric_limits<long>::max();

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_DELETE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	if (te->in_flags(TE_READONLY)) {
		DBCT->_DBCT_error_code = DBCEREADONLY;
		return retval;
	}

	te_mgr.set_ctx(te);

	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_ie_t *ie = te_mgr.get_index(index_id);
	if (ie == NULL) {
		DBCT->_DBCT_error_code = DBCENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: wrong index_id {1} given.") % index_id).str(SGLOCALE));
		return retval;
	}

	dbc_stat stat(bbparms.stat_level, ie->stat_array[(ie->ie_meta.method_type & METHOD_TYPE_HASH) ? IE_ERASE_HASH_STAT : IE_ERASE_STAT]);
	ie_mgr.set_ctx(ie);

	// 查找所有数据区中的数据
	DBCT->_DBCT_rowids->clear();
	DBCT->set_for_update();

	const void *data_buf;
	if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal || data == NULL)
		data_buf = data;
	else
		data_buf = te_mgr.marshal(data);
	ie_mgr.find_row(data_buf);

	if (DBCT->_DBCT_rowids->empty()) {
		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		if (rte_mgr.in_flags(RTE_ABORT_ONLY)) {
			DBCT->_DBCT_error_code = DBCETIME;
			return retval;
		}
		retval = 0;
		return retval;
	}

	// 删除所有数据区中的数据
	long rows = 0;
	for (vector<long>::const_iterator iter = DBCT->_DBCT_rowids->begin(); iter != DBCT->_DBCT_rowids->end(); ++iter) {
		if (erase_by_rowid_internal(table_id, *iter))
			rows++;
	}

	if (get_autocommit() && !commit()) {
		rollback();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: failed to commit transaction.")).str(SGLOCALE));
		return retval;
	}

	retval = rows;
	return retval;
}

bool dbc_api::erase_by_rowid(int table_id, long rowid)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_stat stat(bbparms.stat_level, te->stat_array[TE_ERASE_ROWID_STAT]);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, rowid={2}") % table_id % rowid).str(SGLOCALE), &retval);
#endif

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_DELETE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	if (te->in_flags(TE_READONLY)) {
		DBCT->_DBCT_error_code = DBCEREADONLY;
		return retval;
	}

	te_mgr.set_ctx(te);

	if (!te_mgr.check_rowid(rowid)) {
		DBCT->_DBCT_error_code = DBCEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: staled rowid {1} given.") % rowid).str(SGLOCALE));
		return retval;
	}

	bool status = erase_by_rowid_internal(table_id, rowid);

	if (get_autocommit() && !commit()) {
		rollback();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: failed to commit transaction.")).str(SGLOCALE));
		return retval;
	}

	retval = status;
	return retval;
}

bool dbc_api::insert(int table_id, const void *data)
{
	scoped_usignal sig;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}") % table_id % data).str(SGLOCALE), &retval);
#endif
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	bool status;

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_INSERT)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	status = insert(table_id, data, -1);

	if (get_autocommit() && !commit()) {
		rollback();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: failed to commit transaction.")).str(SGLOCALE));
		return retval;
	}

	retval = status;
	return retval;
}

long dbc_api::update(int table_id, const void *old_data, const void *new_data)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_stat stat(bbparms.stat_level, te->stat_array[TE_UPDATE_STAT]);
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, old_data={2}, new_data={3}") % table_id % old_data % new_data).str(SGLOCALE), &retval);
#endif

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_UPDATE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	if (te->in_flags(TE_READONLY)) {
		DBCT->_DBCT_error_code = DBCEREADONLY;
		return retval;
	}

	te_mgr.set_ctx(te);

	// 查找所有数据区中的数据
	DBCT->_DBCT_rowids->clear();
	DBCT->set_for_update();
	te_mgr.find_row(old_data);

	if (DBCT->_DBCT_rowids->empty()) {
		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		if (rte_mgr.in_flags(RTE_ABORT_ONLY)) {
			DBCT->_DBCT_error_code = DBCETIME;
			return retval;
		}
		retval = 0;
		return retval;
	}

	vector<long>::const_iterator iter;
	long redo_sequence = DBCT->_DBCT_redo_sequence;

	// 删除所有数据区中的数据
	DBCT->_DBCT_erase_rowids.clear();
	for (iter = DBCT->_DBCT_rowids->begin(); iter != DBCT->_DBCT_rowids->end(); ++iter) {
		if (erase_by_rowid_internal(table_id, *iter)) { // 如果数据有变动，导致不能删除，则跳过后续的插入操作
			row_link_t *link = DBCT->rowid2link(*iter);
			link->flags |= ROW_TRAN_UPDATE;
			DBCT->_DBCT_erase_rowids.push_back(*iter);
		}
	}

	// 插入新数据
	for (iter = DBCT->_DBCT_erase_rowids.begin(); iter != DBCT->_DBCT_erase_rowids.end(); ++iter) {
		bool result;
		if (DBCT->_DBCT_update_func_internal) {
			row_link_t *link = DBCT->rowid2link(*iter);
			char *cur_data = link->data();

			clone_data(cur_data);

			char *global[3];
			global[0] = cur_data;
			global[1] = DBCT->_DBCT_data_start;
			global[2] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);

			char *out[1] = { DBCT->_DBCT_row_buf };

			(*DBCT->_DBCT_update_func_internal)(NULL, global, const_cast<const char **>(DBCT->_DBCT_input_binds), out);
			result = insert(table_id, DBCT->_DBCT_row_buf, *iter);
		} else if (DBCT->_DBCT_update_func) {
			char *cur_data = DBCT->rowid2data(*iter);

			if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal) {
				(*DBCT->_DBCT_update_func)(DBCT->_DBCT_row_buf, cur_data, new_data);
			} else {
				const char *data_buf = te_mgr.marshal(new_data);
				(*DBCT->_DBCT_update_func)(DBCT->_DBCT_row_buf, cur_data, data_buf);
			}
			result = insert(table_id, DBCT->_DBCT_row_buf, *iter);
		} else {
			result = insert(table_id, new_data, *iter);
		}

		if (!result) {
			// 恢复删除的数据
			recover(table_id, redo_sequence);
			return retval;
		}
	}

	if (get_autocommit() && !commit()) {
		rollback();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: failed to commit transaction.")).str(SGLOCALE));
		return retval;
	}

	retval = DBCT->_DBCT_erase_rowids.size();
	return retval;
}

long dbc_api::update(int table_id, int index_id, const void *old_data, const void *new_data)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, old_data={3}, new_data={4}") % table_id % index_id % old_data % new_data).str(SGLOCALE), &retval);
#endif

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_UPDATE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	if (te->in_flags(TE_READONLY)) {
		DBCT->_DBCT_error_code = DBCEREADONLY;
		return retval;
	}

	te_mgr.set_ctx(te);

	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_ie_t *ie = te_mgr.get_index(index_id);
	if (ie == NULL) {
		DBCT->_DBCT_error_code = DBCENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: wrong index_id {1} given.") % index_id).str(SGLOCALE));
		return retval;
	}

	dbc_stat stat(bbparms.stat_level, ie->stat_array[(ie->ie_meta.method_type & METHOD_TYPE_HASH) ? IE_UPDATE_HASH_STAT : IE_UPDATE_STAT]);
	ie_mgr.set_ctx(ie);

	// 查找所有数据区中的数据
	const void *data_buf;
	if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal || old_data == NULL)
		data_buf = old_data;
	else
		data_buf = te_mgr.marshal(old_data);
	DBCT->_DBCT_rowids->clear();
	DBCT->set_for_update();
	ie_mgr.find_row(data_buf);

	if (DBCT->_DBCT_rowids->empty()) {
		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		if (rte_mgr.in_flags(RTE_ABORT_ONLY)) {
			DBCT->_DBCT_error_code = DBCETIME;
			return retval;
		}
		retval = 0;
		return retval;
	}

	vector<long>::const_iterator iter;
	long redo_sequence = DBCT->_DBCT_redo_sequence;

	// 删除所有数据区中的数据
	DBCT->_DBCT_erase_rowids.clear();
	for (iter = DBCT->_DBCT_rowids->begin(); iter != DBCT->_DBCT_rowids->end(); ++iter) {
		if (erase_by_rowid_internal(table_id, *iter)) { // 如果数据有变动，导致不能删除，则跳过后续的插入操作
			row_link_t *link = DBCT->rowid2link(*iter);
			link->flags |= ROW_TRAN_UPDATE;
			DBCT->_DBCT_erase_rowids.push_back(*iter);
		}
	}

	// 插入新数据
	for (iter = DBCT->_DBCT_erase_rowids.begin(); iter != DBCT->_DBCT_erase_rowids.end(); ++iter) {
		bool result;
		if (DBCT->_DBCT_update_func_internal) {
			row_link_t *link = DBCT->rowid2link(*iter);
			char *cur_data = link->data();

			clone_data(cur_data);

			char *global[3];
			global[0] = cur_data;
			global[1] = DBCT->_DBCT_data_start;
			global[2] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);

			char *out[1] = { DBCT->_DBCT_row_buf };

			(*DBCT->_DBCT_update_func_internal)(NULL, global, const_cast<const char **>(DBCT->_DBCT_input_binds), out);
			result = insert(table_id, DBCT->_DBCT_row_buf, *iter);
		} else if (DBCT->_DBCT_update_func) {
			char *cur_data = DBCT->rowid2data(*iter);

			if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal) {
				(*DBCT->_DBCT_update_func)(DBCT->_DBCT_row_buf, cur_data, new_data);
			} else {
				const char *data_buf = te_mgr.marshal(new_data);
				(*DBCT->_DBCT_update_func)(DBCT->_DBCT_row_buf, cur_data, data_buf);
			}
			result = insert(table_id, DBCT->_DBCT_row_buf, *iter);
		} else {
			result = insert(table_id, new_data, *iter);
		}

		if (!result) {
			// 恢复删除的数据
			recover(table_id, redo_sequence);
			return retval;
		}
	}

	if (get_autocommit() && !commit()) {
		rollback();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: failed to commit transaction.")).str(SGLOCALE));
		return retval;
	}

	retval = DBCT->_DBCT_erase_rowids.size();
	return retval;
}

long dbc_api::update_by_rowid(int table_id, long rowid, const void *new_data)
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_stat stat(bbparms.stat_level, te->stat_array[TE_UPDATE_ROWID_STAT]);
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, rowid={2}, new_data={3}") % table_id % rowid % new_data).str(SGLOCALE), &retval);
#endif

	dbc_ue_t *ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_UPDATE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		return retval;
	}

	if (te->in_flags(TE_READONLY)) {
		DBCT->_DBCT_error_code = DBCEREADONLY;
		return retval;
	}

	te_mgr.set_ctx(te);

	// 查找所有数据区中的数据
	DBCT->_DBCT_rowids->clear();
	DBCT->set_for_update();
	te_mgr.find_row(rowid);

	if (DBCT->_DBCT_rowids->empty()) {
		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		if (rte_mgr.in_flags(RTE_ABORT_ONLY)) {
			DBCT->_DBCT_error_code = DBCETIME;
			return retval;
		}
		retval = 0;
		return retval;
	}

	vector<long>::const_iterator iter;
	long redo_sequence = DBCT->_DBCT_redo_sequence;

	// 删除所有数据区中的数据
	DBCT->_DBCT_erase_rowids.clear();
	for (iter = DBCT->_DBCT_rowids->begin(); iter != DBCT->_DBCT_rowids->end(); ++iter) {
		if (erase_by_rowid_internal(table_id, *iter)) { // 如果数据有变动，导致不能删除，则跳过后续的插入操作
			row_link_t *link = DBCT->rowid2link(*iter);
			link->flags |= ROW_TRAN_UPDATE;
			DBCT->_DBCT_erase_rowids.push_back(*iter);
		}
	}

	// 插入新数据
	for (iter = DBCT->_DBCT_erase_rowids.begin(); iter != DBCT->_DBCT_erase_rowids.end(); ++iter) {
		bool result;
		if (DBCT->_DBCT_update_func_internal) {
			row_link_t *link = DBCT->rowid2link(*iter);
			char *cur_data = link->data();

			clone_data(cur_data);

			char *global[3];
			global[0] = cur_data;
			global[1] = DBCT->_DBCT_data_start;
			global[2] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);

			char *out[1] = { DBCT->_DBCT_row_buf };

			(*DBCT->_DBCT_update_func_internal)(NULL, global, const_cast<const char **>(DBCT->_DBCT_input_binds), out);
			result = insert(table_id, DBCT->_DBCT_row_buf, *iter);
		} else if (DBCT->_DBCT_update_func) {
			char *cur_data = DBCT->rowid2data(*iter);

			if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal) {
				(*DBCT->_DBCT_update_func)(DBCT->_DBCT_row_buf, cur_data, new_data);
			} else {
				const char *data_buf = te_mgr.marshal(new_data);
				(*DBCT->_DBCT_update_func)(DBCT->_DBCT_row_buf, cur_data, data_buf);
			}
			result = insert(table_id, DBCT->_DBCT_row_buf, *iter);
		} else {
			result = insert(table_id, new_data, *iter);
		}

		if (!result) {
			// 恢复删除的数据
			recover(table_id, redo_sequence);
			return retval;
		}
	}

	if (get_autocommit() && !commit()) {
		rollback();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: failed to commit transaction.")).str(SGLOCALE));
		return retval;
	}

	retval = DBCT->_DBCT_erase_rowids.size();
	return retval;
}

long dbc_api::get_rowid(int seq_no)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(100, __PRETTY_FUNCTION__, (_("seq_no={1}") % seq_no).str(SGLOCALE), &retval);
#endif

	retval = (*DBCT->_DBCT_rowids)[seq_no];
	return retval;
}

long dbc_api::get_currval(const string& seq_name)
{
	dbc_se& se_mgr = dbc_se::instance(DBCT);
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("seq_name={1}") % seq_name).str(SGLOCALE), &retval);
#endif

	retval = se_mgr.get_currval(seq_name);
	return retval;
}

long dbc_api::get_nextval(const string& seq_name)
{
	dbc_se& se_mgr = dbc_se::instance(DBCT);
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("seq_name={1}") % seq_name).str(SGLOCALE), &retval);
#endif

	retval = se_mgr.get_nextval(seq_name);
	return retval;
}

bool dbc_api::precommit()
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_stat stat(bbparms.stat_level, bbmeters.stat_array[PRECOMMIT_STAT]);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	if (!rte_mgr.in_flags(RTE_IN_TRANS)) {
		retval = true;
		return retval;
	}

	dbc_rte_t *rte = rte_mgr.get_rte();
	if (rte->redo_root == -1) {
		retval = true;
		return retval;
	}

	redo_row_t *root = DBCT->long2redorow(rte->redo_root);

	// 把修改记录保存到文件中
	set<redo_prow_t> row_set;

	commit_save(root, row_set);

	dbc_sqlite& sqlite_mgr = dbc_sqlite::instance(DBCT);
	if (!sqlite_mgr.save(row_set, SQLITE_IN_PRECOMMIT))
		return retval;

	rte_mgr.set_flags(RTE_IN_PRECOMMIT);
	retval = true;
	return retval;
}

bool dbc_api::postcommit()
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_stat stat(bbparms.stat_level, bbmeters.stat_array[POSTCOMMIT_STAT]);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	if (!rte_mgr.in_flags(RTE_IN_PRECOMMIT)) {
		DBCT->_DBCT_error_code = DBCEINVAL;
		return retval;
	}

	dbc_rte_t *rte = rte_mgr.get_rte();
	if (rte->redo_root == -1) {
		retval = true;
		return retval;
	}

	dbc_sqlite& sqlite_mgr = dbc_sqlite::instance(DBCT);
	if (!sqlite_mgr.persist(rte->tran_id))
		return retval;

	rte_mgr.clear_flags(RTE_IN_PRECOMMIT);

	// 更新内存记录
	redo_row_t *root = DBCT->long2redorow(rte->redo_root);
	int status = commit_internal(root);

	rte_mgr.clear_flags(RTE_IN_TRANS);

	// 释放节点
	dbc_redo& redo_mgr = dbc_redo::instance(DBCT);
	rte->redo_root = -1;
 	redo_mgr.free_buckets(rte->redo_full);
	redo_mgr.free_buckets(rte->redo_free);

	retval = status;
	return retval;
}

bool dbc_api::commit()
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_stat stat(bbparms.stat_level, bbmeters.stat_array[COMMIT_STAT]);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	if (!rte_mgr.in_flags(RTE_IN_TRANS)) {
		retval = true;
		return retval;
	}

	dbc_rte_t *rte = rte_mgr.get_rte();
	if (rte->redo_root == -1) {
		rte_mgr.reset_tran();
		retval = true;
		return retval;
	}

	if (rte_mgr.in_flags(RTE_ABORT_ONLY)) {
		rollback();
		DBCT->_DBCT_error_code = DBCETIME;
		return retval;
	}

	redo_row_t *root = DBCT->long2redorow(rte->redo_root);

	// 把修改记录保存到文件中
	set<redo_prow_t> row_set;

	commit_save(root, row_set);

	dbc_sqlite& sqlite_mgr = dbc_sqlite::instance(DBCT);
	if (!sqlite_mgr.save(row_set, 0))
		return retval;

	// 更新内存记录
	int status = commit_internal(root);

	rte_mgr.reset_tran();

	// 释放节点
	dbc_redo& redo_mgr = dbc_redo::instance(DBCT);
	rte->redo_root = -1;
 	redo_mgr.free_buckets(rte->redo_full);
	redo_mgr.free_buckets(rte->redo_free);

	retval = status;
	return retval;
}

bool dbc_api::rollback()
{
	scoped_usignal sig;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_stat stat(bbparms.stat_level, bbmeters.stat_array[ROLLBACK_STAT]);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	if (!rte_mgr.in_flags(RTE_IN_TRANS)) {
		retval = true;
		return retval;
	}

	dbc_rte_t *rte = rte_mgr.get_rte();
	if (rte->redo_root == -1) {
		rte_mgr.reset_tran();
		retval = true;
		return retval;
	}

	redo_row_t *root = DBCT->long2redorow(rte->redo_root);
	int status = rollback_internal(root);

	rte_mgr.reset_tran();

	// 释放节点
	dbc_redo& redo_mgr = dbc_redo::instance(DBCT);
	rte->redo_root = -1;
 	redo_mgr.free_buckets(rte->redo_full);
	redo_mgr.free_buckets(rte->redo_free);

	retval = status;
	return retval;
}

size_t dbc_api::size(int table_id)
{
	size_t retval = -1;
#if defined(DEBUG)
	scoped_debug<size_t> debug(100, __PRETTY_FUNCTION__, (_("table_id={1}") % table_id).str(SGLOCALE), &retval);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	retval = te->cur_count;
	return retval;
}

int dbc_api::struct_size(int table_id)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, (_("table_id={1}") % table_id).str(SGLOCALE), &retval);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	retval = te->te_meta.struct_size;
	return retval;
}

bool dbc_api::set_autocommit(bool autocommit)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("autocommit={1}") % autocommit).str(SGLOCALE), &retval);
#endif

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	rte_mgr.set_autocommit(autocommit);
	retval = true;
	return retval;
}

bool dbc_api::get_autocommit()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	const dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	retval = rte_mgr.get_autocommit();
	return retval;
}

bool dbc_api::set_timeout(long timeout)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("timeout={1}") % timeout).str(SGLOCALE), &retval);
#endif

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	rte_mgr.set_timeout(timeout);
	retval = true;
	return retval;
}

long dbc_api::get_timeout()
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	const dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	retval = rte_mgr.get_timeout();
	return retval;
}

void dbc_api::set_update_func(DBCC_UPDATE_FUNC update_func)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("update_func={1}") % update_func).str(SGLOCALE), NULL);
#endif

	DBCT->set_update_func(update_func);
}

void dbc_api::clear_update_func()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	DBCT->set_update_func(NULL);
}

void dbc_api::set_where_func(compiler::func_type where_func)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("where_func={1}") % where_func).str(SGLOCALE), NULL);
#endif

	DBCT->set_where_func(where_func);
}

void dbc_api::clear_where_func()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	DBCT->set_where_func(NULL);
}

bool dbc_api::truncate(const string& table_name)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_te_t *te;
	scoped_usignal sig;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("table_name={1}") % table_name).str(SGLOCALE), &retval);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();

	if (ue == NULL || !(ue->perm & DBC_PERM_TRUNCATE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: permission denied, current user is not allowed to truncate table.")).str(SGLOCALE));
		return retval;
	}

	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	// Check if given table name exists
	for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
		if (!te->in_active() || te->in_flags(TE_SYS_RESERVED))
			continue;

		dbc_data_t& te_meta = te->te_meta;
		if (strcasecmp(te_meta.table_name, table_name.c_str()) == 0)
			break;
	}

	if (te > DBCT->_DBCT_tes + bbmeters.curmaxte) {
		DBCT->_DBCT_error_code = DBCENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: table {1} doesn't exist.") % table_name).str(SGLOCALE));
		return retval;
	}

	te_mgr.set_ctx(te);
	bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

	for (int i = 0; i <= te->max_index; i++) {
		dbc_ie_t *ie = te_mgr.get_index(i);
		if (ie == NULL)
			continue;

		ie_mgr.set_ctx(ie);
		ie_mgr.free_mem();
	}

	for (int i = 0; i < MAX_VSLOTS; i++) {
		bi::scoped_lock<bi::interprocess_mutex> lock(te->mutex_free[i]);

		te->row_free[i] = -1;
	}

	{
		bi::scoped_lock<bi::interprocess_mutex> lock(te->mutex_ref);

		te->row_ref = -1;
	}

	te_mgr.free_mem();
	te->cur_count = 0;
	te->row_used = -1;

	dbc_sqlite& sqlite_mgr = dbc_sqlite::instance(DBCT);
	if (!sqlite_mgr.truncate(te->table_id)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to truncate table {1}") % table_name).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

bool dbc_api::create(dbc_te_t& te_)
{
	int i;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te;
	dbc_te_t *unused_te = NULL;
	scoped_usignal sig;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("table_id={1}") % te_.table_id).str(SGLOCALE), &retval);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();

	if (ue == NULL || !(ue->perm & DBC_PERM_CREATE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: permission denied, current user is not allowed to create table.")).str(SGLOCALE));
		return retval;
	}

	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	// Check if given table name exists
	for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
		if (!te->in_flags(TE_IN_USE)) {
			if (unused_te == NULL)
				unused_te = te;
			continue;
		}

		dbc_data_t& te_meta = te->te_meta;
		if (strcasecmp(te_meta.table_name, te_.te_meta.table_name) == 0) {
			DBCT->_DBCT_error_code = DBCEDUPENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: table creation conflict, table {1} exists.") % te_.te_meta.table_name).str(SGLOCALE));
			return retval;
		}
	}

	if (unused_te == NULL) {
		if (bbmeters.curmaxte + 1 == bbparms.maxtes) {
			DBCT->_DBCT_error_code = DBCENOENT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: TE is full for table {1}") % te_.te_meta.table_name).str(SGLOCALE));
			return retval;
		}

		bbmeters.curmaxte++;
	} else {
		te = unused_te;
	}

	// go through tes to get an unused table_id
	for (i = 0; i < MAX_DBC_ENTITIES; i++) {
		if (ue->thash[i] == -1) {
			te_.table_id = i;
			te->user_id = ue->user_id;
			te_mgr.set_ctx(te);
			if (!te_mgr.init(te, te_, vector<dbc_ie_t>()))
				return retval;

			te->set_flags(TE_IN_MEM);
			ue->thash[i] = reinterpret_cast<char *>(te) - reinterpret_cast<char *>(DBCT->_DBCT_tes);
			bbmeters.curtes++;
			bbparms.bbversion++;
			retval = true;
			return retval;
		}
	}

	DBCT->_DBCT_error_code = DBCENOENT;
	GPP->write_log(__FILE__, __LINE__, (_("ERROR: No free thash entry is found for table {1}") % te_.te_meta.table_name).str(SGLOCALE));
	return retval;
}

bool dbc_api::drop(const string& table_name)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_te_t *te;
	scoped_usignal sig;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("table_name={1}") % table_name).str(SGLOCALE), &retval);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();

	if (ue == NULL || !(ue->perm & DBC_PERM_DROP)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: permission denied, current user is not allowed to drop table.")).str(SGLOCALE));
		return retval;
	}

	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	// Check if given table name exists
	for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
		if (!te->in_active() || te->in_flags(TE_SYS_RESERVED))
			continue;

		dbc_data_t& te_meta = te->te_meta;
		if (strcasecmp(te_meta.table_name, table_name.c_str()) == 0)
			break;
	}

	if (te > DBCT->_DBCT_tes + bbmeters.curmaxte) {
		DBCT->_DBCT_error_code = DBCENOENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: table {1} doesn't exist.") % table_name).str(SGLOCALE));
		return retval;
	}

	te_mgr.set_ctx(te);
	bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

	for (int i = 0; i <= te->max_index; i++) {
		dbc_ie_t *ie = te_mgr.get_index(i);
		if (ie == NULL)
			continue;

		ie_mgr.set_ctx(ie);
		ie_mgr.free_mem();
		te->index_hash[i] = -1;
	}

	te_mgr.free_mem();
	te->flags = 0;

	ue->thash[te->table_id] = -1;
	bbmeters.curtes--;

	if (te == DBCT->_DBCT_tes + bbmeters.curmaxte) {
		for (te--; te >= DBCT->_DBCT_tes; te--) {
			if (te->in_use())
				break;
		}
		bbmeters.curmaxte = te - DBCT->_DBCT_tes;
	}

	dbc_sqlite& sqlite_mgr = dbc_sqlite::instance(DBCT);
	if (!sqlite_mgr.truncate(te->table_id)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to truncate table {1}") % table_name).str(SGLOCALE));
		return retval;
	}

	bbparms.bbversion++;
	retval = true;
	return retval;
}

bool dbc_api::create_index(const dbc_ie_t& ie_node)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("index_id={1}") % ie_node.index_id).str(SGLOCALE), &retval);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();

	if (ue == NULL || !(ue->perm & DBC_PERM_CREATE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: permission denied, current user is not allowed to create index.")).str(SGLOCALE));
		return retval;
	}

	if (te->index_hash[ie_node.index_id] != -1) {
		DBCT->_DBCT_error_code = DBCEDUPENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to create index {1}, index_id conflicts.") % ie_node.index_id).str(SGLOCALE));
		return retval;
	}

	long offset = bbp->alloc(sizeof(dbc_ie_t) + ie_node.ie_meta.buckets * sizeof(row_bucket_t));
	if (offset == -1) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = USBRK;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't allocate rows for index {1}") % ie_node.ie_meta.index_name).str(SGLOCALE));
		return retval;
	}

	dbc_ie_t *ie = reinterpret_cast<dbc_ie_t *>(reinterpret_cast<char *>(bbp) + offset);
	*ie = ie_node;

	// 对表加写锁，以保证只有一个会话操作该表
	bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

	te->index_hash[ie->index_id] = offset;
	te->max_index = std::max(te->max_index, ie->index_id);

	ie_mgr.set_ctx(ie);
	ie_mgr.init();

	if (te->row_used != -1) {
		long rowid = te->row_used;
		char *row_data = DBCT->rowid2data(rowid);
		while (1) {
			if (!ie_mgr.create_row(rowid)) {
				bbp->free(offset);
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: index {1}: Can't create index row, {2}") % ie->ie_meta.index_name % DBCT->strerror()).str(SGLOCALE));
				return retval;
			}

			row_link_t *link = DBCT->rowid2link(rowid);
			if (link->next == -1)
				break;

			rowid = link->next;
			row_data = DBCT->rowid2data(rowid);
		}
	}

	bbparms.bbversion++;
	retval = true;
	return retval;
}

bool dbc_api::create_sequence(const dbc_se_t& se_node)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("seq_name={1}") % se_node.seq_name).str(SGLOCALE), &retval);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();

	if (ue == NULL || !(ue->perm & DBC_PERM_CREATE)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: permission denied, current user is not allowed to create sequence.")).str(SGLOCALE));
		return retval;
	}

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	string full_name = bbparms.sys_dir;
	string seq_name = se_node.seq_name;
	std::transform(seq_name.begin(), seq_name.end(), seq_name.begin(), tolower_predicate());
	full_name += seq_name;
	full_name += ".";
	full_name += boost::lexical_cast<string>(ue->get_user_idx());
	full_name += ".s";

	if (access(full_name.c_str(), R_OK) != -1) {
		DBCT->_DBCT_error_code = DBCEDUPENT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: sequence {1} exists.") % se_node.seq_name).str(SGLOCALE));
		return retval;
	}

	ofstream ofs(full_name.c_str());
	if (!ofs) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = UOPEN;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	shash_t *shash = ue->shash;
	int hash_value = DBCT->get_hash(se_node.seq_name, bbparms.maxses);

	dbc_se_t *se;
	for (se = DBCT->_DBCT_ses; se < DBCT->_DBCT_ses + bbparms.maxses; se++) {
		if (!(se->flags & SE_IN_USE))
			break;
	}
	if (se == DBCT->_DBCT_ses + bbparms.maxses) {
		DBCT->_DBCT_error_code = DBCELIMIT;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to create sequence {1}, too many sequences.") % se_node.seq_name).str(SGLOCALE));
		return retval;
	}

	*se = se_node;
	se->flags = SE_IN_USE;
	se->accword = 0;
	se->prev = -1;
	se->next = shash[hash_value];

	int offset = se - DBCT->_DBCT_ses;
	if (shash[hash_value] != -1) {
		dbc_se_t *head = DBCT->_DBCT_ses + shash[hash_value];
		head->prev = offset;
	}
	shash[hash_value] = offset;

	ofs << "user_idx=" << ue->get_user_idx() << "\n"
		<< "slot_id=" << (se - DBCT->_DBCT_ses) << "\n"
		<< "seq_name=" << se_node.seq_name << "\n"
		<< "minval=" << se_node.minval << "\n"
		<< "maxval=" << se_node.maxval << "\n"
		<< "currval=" << se_node.currval << "\n"
		<< "increment=" << se_node.increment << "\n"
		<< "cache=" << se_node.cache << "\n"
		<< "cycle=" << (se_node.cycle ? 1 : 0) << "\n"
		<< "order=" << (se_node.order ? 1 : 0) << "\n";

	ofs.close();

	full_name = bbparms.sys_dir;
	full_name += "sequence.dat";
	ofs.open(full_name.c_str(), ios_base::in | ios_base::out);
	if (!ofs) {
		ofs.open(full_name.c_str(), ios_base::in | ios_base::out | ios_base::trunc);
		if (!ofs) {
			DBCT->_DBCT_error_code = DBCEOS;
			DBCT->_DBCT_native_code = UOPEN;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
			return retval;
		}
	}

	ofs.seekp(offset * sizeof(long), ios_base::beg);
	ofs.write(reinterpret_cast<char *>(&se->currval), sizeof(long));

	bbparms.bbversion++;
	retval = true;
	return retval;
}

// Locked by caller
bool dbc_api::alter_sequence(dbc_se_t *se)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("seq_name={1}") % se->seq_name).str(SGLOCALE), &retval);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();

	if (ue == NULL || !(ue->perm & DBC_PERM_ALTER)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: permission denied, current user is not allowed to alter sequence.")).str(SGLOCALE));
		return retval;
	}

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	string full_name = bbparms.sys_dir;
	string seq_name = se->seq_name;
	std::transform(seq_name.begin(), seq_name.end(), seq_name.begin(), tolower_predicate());
	full_name += seq_name;
	full_name += ".s";

	ofstream ofs(full_name.c_str());
	if (!ofs) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = UOPEN;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	ofs << "slot_id=" << (se - DBCT->_DBCT_ses) << "\n"
		<< "seq_name=" << se->seq_name << "\n"
		<< "minval=" << se->minval << "\n"
		<< "maxval=" << se->maxval << "\n"
		<< "currval=" << se->currval << "\n"
		<< "increment=" << se->increment << "\n"
		<< "cache=" << se->cache << "\n"
		<< "cycle=" << (se->cycle ? 1 : 0) << "\n"
		<< "order=" << (se->order ? 1 : 0) << "\n";

	ofs.close();

	full_name = bbparms.sys_dir;
	full_name += "sequence.dat";
	ofs.open(full_name.c_str(), ios_base::in | ios_base::out);
	if (!ofs) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = UOPEN;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	int offset = se - DBCT->_DBCT_ses;
	ofs.seekp(offset * sizeof(long), ios_base::beg);
	ofs.write(reinterpret_cast<char *>(&se->currval), sizeof(long));

	bbparms.bbversion++;
	retval = true;
	return retval;
}

int dbc_api::get_error_code() const
{
	return DBCT->_DBCT_error_code;
}

int dbc_api::get_native_code() const
{
	return DBCT->_DBCT_native_code;
}

const char * dbc_api::strerror() const
{
	return DBCT->strerror();
}

const char * dbc_api::strnative() const
{
	return uunix::strerror(DBCT->_DBCT_native_code);
}

int dbc_api::get_ur_code() const
{
	return SGT->_SGT_ur_code;
}

void dbc_api::set_ur_code(int ur_code)
{
	SGT->_SGT_ur_code = ur_code;
}

bool dbc_api::drop_sequence(const string& seq_name)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("seq_name={1}") % seq_name).str(SGLOCALE), &retval);
#endif

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *ue = ue_mgr.get_ue();

	if (ue == NULL || !(ue->perm & DBC_PERM_DROP)) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: permission denied, current user is not allowed to drop sequence.")).str(SGLOCALE));
		return retval;
	}

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	string full_name = bbparms.sys_dir;
	full_name += seq_name;
	full_name += ".s";
	if (unlink(full_name.c_str()) == -1) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = ULINK;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't remove {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	shash_t *shash = ue->shash;
	int hash_value = DBCT->get_hash(seq_name.c_str(), bbparms.maxses);

	int offset = shash[hash_value];
	while (offset != -1) {
		dbc_se_t *se = DBCT->_DBCT_ses + offset;
		if (strcasecmp(se->seq_name, seq_name.c_str()) == 0) {
			if (se->prev != -1) {
				dbc_se_t *prev = DBCT->_DBCT_ses + se->prev;
				prev->next = se->next;
			} else {
				shash[hash_value] = se->next;
			}
			if (se->next != -1) {
				dbc_se_t *next = DBCT->_DBCT_ses + se->next;
				next->prev = se->prev;
			}
			se->flags &= ~SE_IN_USE;
			bbparms.bbversion++;
			retval = true;
			return retval;
		}

		offset = se->next;
	}

	DBCT->_DBCT_error_code = DBCENOENT;
	GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to drop sequence {1}, no such sequence.") % seq_name).str(SGLOCALE));
	return retval;
}

int dbc_api::get_partition_id(int partition_type, int partitions, const char *key)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, (_("partition_type={1}, partitions={2}, key={3}") % partition_type % partitions % key).str(SGLOCALE), &retval);
#endif

	if (partition_type == PARTITION_TYPE_NUMBER) {
		retval = static_cast<int>(::atol(key) % partitions);
	} else {
		size_t len = ::strlen(key);
		retval = static_cast<int>(boost::hash_range(key, key + len) % partitions);
	}

	return retval;
}

dbc_api::dbc_api()
{
}

dbc_api::~dbc_api()
{
}

bool dbc_api::erase_by_rowid_internal(int table_id, long rowid)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("table_id={1}, rowid={2}") % table_id % rowid).str(SGLOCALE), &retval);
#endif

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	if (!te->in_flags(TE_IN_MEM)) {
		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		if (!rte_mgr.set_tran())
			return retval;
	}

	row_link_t *link = DBCT->rowid2link(rowid);

	// 对数据进行加锁
	if (!te_mgr.lock_row(link)) {
		rollback();
		DBCT->_DBCT_error_code = DBCESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't lock row for rowid {1}") % rowid).str(SGLOCALE));
		return retval;
	}

	if (!(link->flags & ROW_IN_USE)) {
		te_mgr.unlock_row(link);
		DBCT->_DBCT_error_code = DBCEINVAL;
		return retval;
	}

	if (!te->in_flags(TE_IN_MEM)) {
		// 记录变更数据
		int row_size = te->te_meta.internal_size + link->extra_size;
		if (!DBCT->change_insert(table_id, row_size, 0, rowid, -1)) {
			te_mgr.unlock_row(link);
			// ERROR is already set.
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert row into redo tree, {1} - {2}, table_id {3}, rowid {4}")
				% DBCT->strerror() % DBCT->strnative() % table_id % rowid).str(SGLOCALE));
			return retval;
		}

		// 设置删除标识
		link->flags |= ROW_TRAN_DELETE;
	} else {
		// 删除数据区索引
		te_mgr.erase_row_indexes(rowid);

		// 删除数据区记录
		if (!te_mgr.erase_row(rowid))
			return retval;
	}

	retval = true;
	return retval;
}

// 插入一行记录，data为外部格式
bool dbc_api::insert(int table_id, const void *data, long old_rowid)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	dbc_stat stat(bbparms.stat_level, te->stat_array[TE_INSERT_STAT]);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}, old_rowid={3}") % table_id % data % old_rowid).str(SGLOCALE), &retval);
#endif

	if (te->in_flags(TE_READONLY)) {
		DBCT->_DBCT_error_code = DBCEREADONLY;
		return retval;
	}

	if (!te->in_flags(TE_IN_MEM)) {
		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		if (!rte_mgr.set_tran())
			return retval;
	}

	te_mgr.set_ctx(te);

	// 创建记录区数据
	long rowid = te_mgr.create_row(data);
	if (rowid == -1)
		return retval;

	// 创建记录区索引
	if (!te_mgr.create_row_indexes(rowid)) {
		te_mgr.erase_row(rowid);
		return retval;
	}

	if (!te->in_flags(TE_IN_MEM)) {
		// 记录变更数据
		row_link_t *link = DBCT->rowid2link(rowid);
		int row_size = te->te_meta.internal_size + link->extra_size;
		int old_row_size;
		if (old_rowid >= 0) {
			link = DBCT->rowid2link(old_rowid);
			old_row_size = te->te_meta.internal_size + link->extra_size;
		} else {
			old_row_size = 0;
		}

		if (!DBCT->change_insert(table_id, row_size, old_row_size, rowid, old_rowid)) {
			te_mgr.erase_row_indexes(rowid);
			te_mgr.erase_row(rowid);
			return retval;
		}
	} else {
		// 清除事务标识
		row_link_t *link = DBCT->rowid2link(rowid);
		link->flags &= ~ROW_TRAN_MASK;

		// 对数据区记录进行解锁
		te_mgr.unlock_row(link);
	}

	retval = true;
	return retval;
}

// 回滚删除
void dbc_api::erase_undo(int table_id, long rowid, long redo_sequence)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("table_id={1}, rowid= {2}, redo_sequence={3}") % table_id % rowid % redo_sequence).str(SGLOCALE), NULL);
#endif
	dbc_te& te_mgr = dbc_te::instance(DBCT);

	// 清除删除/更新标识
	row_link_t *link = DBCT->rowid2link(rowid);
	link->flags &= ~(ROW_TRAN_DELETE | ROW_TRAN_UPDATE);

	// 对数据进行加锁
	te_mgr.unlock_row(link);

	// 记录变更数据
	DBCT->change_erase(table_id, rowid, redo_sequence);
}

// 回滚插入
void dbc_api::insert_undo(int table_id, long rowid, long redo_sequence)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("table_id={1}, rowid= {2}, redo_sequence={3}") % table_id % rowid % redo_sequence).str(SGLOCALE), NULL);
#endif
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te_t *te = ue_mgr.get_table(table_id);
	te_mgr.set_ctx(te);

	// 删除记录区索引
	te_mgr.erase_row_indexes(rowid);

	// 删除记录区数据
	te_mgr.erase_row(rowid);

	// 记录变更数据
	DBCT->change_erase(table_id, rowid, redo_sequence);
}

// 提交前的保存数组
void dbc_api::commit_save(const redo_row_t *root, set<redo_prow_t>& row_set)
{
	stack<const redo_row_t *> redo_stack;
	const redo_row_t *current = root;
	redo_prow_t item;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("root={1}, row_set.size()={2}") % root % row_set.size()).str(SGLOCALE), NULL);
#endif

	while (1) {
		while (current->left != -1) {
			redo_stack.push(current);
			current = DBCT->long2redorow(current->left);
		}

ENTRY:
		item.redo_row = current;
		row_set.insert(item);

		if (current->right != -1) {
			current = DBCT->long2redorow(current->right);
		} else {
			if (redo_stack.empty())
				break;
			current = redo_stack.top();
			redo_stack.pop();
			goto ENTRY;
		}
	}
}

// 内部提交
bool dbc_api::commit_internal(redo_row_t *root)
{
	stack<redo_row_t *> redo_stack;
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	redo_row_t *current = root;
	int user_idx = -1;
	int table_id = -1;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("root={1}") % root).str(SGLOCALE), &retval);
#endif

	dbc_ue_t *saved_ue = ue_mgr.get_ue();
	BOOST_SCOPE_EXIT((&ue_mgr)(&saved_ue)) {
		ue_mgr.set_ctx(saved_ue);
	} BOOST_SCOPE_EXIT_END

	while (1) {
		while (current->left != -1) {
			redo_stack.push(current);
			current = DBCT->long2redorow(current->left);
		}

ENTRY:
		if (user_idx != current->user_idx || table_id != current->table_id) {
			user_idx = current->user_idx;
			table_id = current->table_id;
			ue_mgr.set_ctx(user_idx);
			dbc_te_t *te = ue_mgr.get_table(table_id);
			te_mgr.set_ctx(te);
		}

		row_link_t *link = DBCT->rowid2link(current->rowid);
		if (link->flags & ROW_TRAN_DELETE) {
			// 删除数据区索引
			te_mgr.erase_row_indexes(current->rowid);

			// 删除数据区记录
			if (!te_mgr.erase_row(current->rowid))
				return retval;
		} else if (link->flags & ROW_TRAN_INSERT) {
			// 清除事务标识
			link->flags &= ~ROW_TRAN_MASK;

			// 对数据区记录进行解锁
			te_mgr.unlock_row(link);
		}

		if (current->right != -1) {
			current = DBCT->long2redorow(current->right);
		} else {
			if (redo_stack.empty())
				break;
			current = redo_stack.top();
			redo_stack.pop();
			goto ENTRY;
		}
	}

	retval = true;
	return retval;
}

// 内部回滚
bool dbc_api::rollback_internal(redo_row_t *root)
{
	stack<redo_row_t *> redo_stack;
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	redo_row_t *current = root;
	int user_idx = -1;
	int table_id = -1;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("root={1}") % root).str(SGLOCALE), &retval);
#endif

	dbc_ue_t *saved_ue = ue_mgr.get_ue();
	BOOST_SCOPE_EXIT((&ue_mgr)(&saved_ue)) {
		ue_mgr.set_ctx(saved_ue);
	} BOOST_SCOPE_EXIT_END

	while (1) {
		while (current->left != -1) {
			redo_stack.push(current);
			current = DBCT->long2redorow(current->left);
		}

ENTRY:
		if (user_idx != current->user_idx || table_id != current->table_id) {
			user_idx = current->user_idx;
			table_id = current->table_id;
			ue_mgr.set_ctx(user_idx);
			dbc_te_t *te = ue_mgr.get_table(table_id);
			te_mgr.set_ctx(te);
		}


		row_link_t *link = DBCT->rowid2link(current->rowid);
		if (link->flags & ROW_TRAN_INSERT) {
			// 删除数据区索引
			te_mgr.erase_row_indexes(current->rowid);

			// 删除数据区记录
			if (!te_mgr.erase_row(current->rowid))
				return retval;
		} else if (link->flags & ROW_TRAN_DELETE) {
			// 清除事务标识
			link->flags &= ~ROW_TRAN_MASK;

			// 对数据区记录进行解锁
			te_mgr.unlock_row(current->rowid);
		}


		if (current->right != -1) {
			current = DBCT->long2redorow(current->right);
		} else {
			if (redo_stack.empty())
				break;
			current = redo_stack.top();
			redo_stack.pop();
			goto ENTRY;
		}
	}

	retval = true;
	return retval;
}

// 恢复删除的数据，不小于指定序列号的都需要恢复
void dbc_api::recover(int table_id, long redo_sequence)
{
	scoped_usignal sig;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("table_id={1}, redo_sequence={2}") % table_id % redo_sequence).str(SGLOCALE), NULL);
#endif

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();
	if (rte->redo_root == -1)
		return;

	redo_row_t *root = DBCT->long2redorow(rte->redo_root);

	// 把修改记录保存到文件中
	set<redo_prow_t> row_set;

	commit_save(root, row_set);

	for (set<redo_prow_t>::const_iterator iter = row_set.begin(); iter != row_set.end(); ++iter) {
		const redo_row_t *redo_row = iter->redo_row;
		if (redo_row->sequence < redo_sequence)
			continue;

		if (redo_row->old_rowid == -1)
			erase_undo(table_id, redo_row->rowid, redo_row->sequence);
		else
			insert_undo(table_id, redo_row->rowid, redo_row->sequence);
	}
}

// 复制一行记录，data为内部格式，结果为外部格式
void dbc_api::clone_data(const char *data)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("data={1}") % data).str(SGLOCALE), NULL);
#endif
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	dbc_data_t& te_meta = te->te_meta;
	int& data_size = te_meta.struct_size;

	if (DBCT->_DBCT_row_buf_size < data_size) {
		DBCT->_DBCT_row_buf_size = data_size;
		delete []DBCT->_DBCT_row_buf;
		DBCT->_DBCT_row_buf = new char[DBCT->_DBCT_row_buf_size];
	}

	if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal)
		memcpy(DBCT->_DBCT_row_buf, data, data_size);
	else
		te_mgr.unmarshal(DBCT->_DBCT_row_buf, data);
}

// 注销前的执行函数
void dbc_api::logout_atexit()
{
	dbct_ctx *DBCT = dbct_ctx::instance();
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	try {
		dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
		if (ue_mgr.connected()) {
			dbc_api& api_mgr = dbc_api::instance(DBCT);

			DBCT->_DBCT_where_func = dbc_api::match_myself;
			auto_ptr<dbc_cursor> cursor = api_mgr.find(TE_SYS_INDEX_LOCK, NULL);
			while (cursor->next())
				api_mgr.erase_by_rowid(TE_SYS_INDEX_LOCK, cursor->rowid());
			cursor.reset();
			DBCT->_DBCT_where_func = NULL;

			api_mgr.disconnect();
			api_mgr.logout();
		}
	} catch (exception& ex) {
		gpp_ctx *GPP = gpp_ctx::instance();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: exception caught on logout_atexit(), {1}") % ex.what()).str(SGLOCALE));
	}
}

// 当前进程的锁
int dbc_api::match_myself(void *client, char **global, const char **in, char **out)
{
	dbcp_ctx *DBCP = dbcp_ctx::instance();
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(1000, __PRETTY_FUNCTION__, "", &retval);
#endif

	// global[0] represents row data of table.
	t_sys_index_lock *data = reinterpret_cast<t_sys_index_lock *>(global[0]);
	if (data->pid == DBCP->_DBCP_current_pid) {
		retval = 0;
		return retval;
	}

	return retval;
}

}
}

