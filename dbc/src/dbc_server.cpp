#include "dbc_server.h"
#include "dbc_internal.h"
#include "sftp.h"

using namespace ai::scci;

namespace ai
{
namespace sg
{

int const PER_BATCH = 10;

struct dbc_change_t
{
	dbc_te_t *te;
	bool readonly;
};

struct dbc_scp_t
{
	int mid;
	vector<string> pmids;
	string data_dir;
	string filename;

	bool operator<(const dbc_scp_t& rhs) const {
		if (mid < rhs.mid)
			return true;
		else if (mid > rhs.mid)
			return false;

		if (data_dir < rhs.data_dir)
			return true;
		else if (data_dir > rhs.data_dir)
			return false;

		return filename < rhs.filename;
	}
};

dbc_partition_t::dbc_partition_t()
{
	db = NULL;
	stmt = NULL;
	GPP = gpp_ctx::instance();
}

dbc_partition_t::~dbc_partition_t()
{
	close();
}

bool dbc_partition_t::open(const string& data_dir, const string& table_name, int partition_id, bool refresh)
{
	string sql;

	db_name = (boost::format("%1%%2%-%3%") % data_dir % table_name % partition_id).str();
	if (!refresh) {
		db_name += ".b";
		// 首先删除已经存在的文件
		unlink(db_name.c_str());
	} else {
		db_name += ".q";
	}

	if (DBCP->_DBCP_proc_type != DBC_STANDALONE) {
		sgc_ctx *SGC = SGT->SGC();

		db_name += ".";
		db_name += SGC->mid2lmid(SGC->_SGC_proc.mid);
	}

	if (sqlite3_open_v2(db_name.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open {1} to create sqlite3 database, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	sql = string("insert into ") + table_name;
	if (!refresh)
		sql += "(data) values(:1)";
	else
		sql += "(type, data) values(:1, :2)";
	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
		string csql = string("create table ") + table_name;
		if (!refresh)
			csql += "(data text)";
		else
			csql += "(type integer, data text)";
		if (sqlite3_exec(db, csql.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create table {1} on sqlite3 database, {2}") % table_name % sqlite3_errmsg(db)).str(SGLOCALE));
			return false;
		}

		int retcode = sqlite3_reset(stmt);
		if (retcode != SQLITE_OK && retcode != SQLITE_CONSTRAINT) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't reset insert statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return false;
		}

		if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't prepare {1}, {2}") % sql % sqlite3_errmsg(db)).str(SGLOCALE));
			return false;
		}
	}

	if (sqlite3_exec(db, "begin", NULL, NULL, NULL) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't execute begin statement for table {1}, {2}") % table_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	return true;
}

void dbc_partition_t::close()
{
	if (stmt) {
		sqlite3_exec(db, "commit", NULL, NULL, NULL);
		sqlite3_finalize(stmt);
		stmt = NULL;
	}

	if (db) {
		sqlite3_close(db);
		db = NULL;
	}
}

bool dbc_partition_t::insert_field(const string& fields_string)
{
	string sql = string("create table if not exists t_fields (data text) ");
	if (sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create table t_fields on sqlite3 database, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	sql = string("insert into t_fields (data) values('");
	sql += fields_string;
	sql += "')";
	if (sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert table t_fields on sqlite3 database, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	return true;
}

bool dbc_partition_t::insert(const char *data, int size)
{
	if (sqlite3_bind_text(stmt, 1, data, size, SQLITE_STATIC) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind data for db_name {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert data to db_name {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	if (sqlite3_reset(stmt) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't reset for db_name {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	return true;
}

bool dbc_partition_t::insert(int type, const char *data, int size)
{
	if (sqlite3_bind_int(stmt, 1, type) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind data for db_name {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	if (sqlite3_bind_text(stmt, 2, data, size, SQLITE_STATIC) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind data for db_name {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert data to db_name {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return false;
	}

	return true;
}

void dbc_partition_t::remove()
{
	if (!db_name.empty())
		unlink(db_name.c_str());
}

dbc_server::dbc_server()
{
	orow_buf = NULL;
	row_buf = NULL;
	orow_buf_size = 0;
	row_buf_size = 0;
	login_time = time(0);
}

dbc_server::~dbc_server()
{
	logout();

	delete []orow_buf;
	delete []row_buf;
}

void dbc_server::run()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", NULL);
#endif

	usignal::signal(SIGINT, stdsigterm);
	usignal::signal(SIGHUP, stdsigterm);
	usignal::signal(SIGTERM, stdsigterm);

	if (!login()) {
		cout << (_("ERROR: login to domain failed, ")).str(SGLOCALE) << DBCT->strerror() << endl;
		::exit(1);
	}

	watch();
}

bool dbc_server::safe_load(dbc_te_t *te)
{
	int i;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_ue_t *ue;
	dbc_te_t *curte;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}") % te->table_id).str(SGLOCALE), &retval);
#endif

	if (!DBCP->_DBCP_is_server && !DBCP->_DBCP_is_admin) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function can only be called by dbc_server or dbc_admin.")).str(SGLOCALE));
		return retval;
	}

	{
		// Initialize new te structure
		bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

		// Check if given table name exists
		for (curte = DBCT->_DBCT_tes; curte <= DBCT->_DBCT_tes + bbmeters.curmaxte; curte++) {
			if (!curte->in_flags(TE_IN_USE))
				break;
		}

		if (curte > DBCT->_DBCT_tes + bbmeters.curmaxte) {
			if (bbmeters.curmaxte + 1 == bbparms.maxtes)
				return retval;

			bbmeters.curmaxte++;
		}

		// go through thash to get an unused table_id
		ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
		for (i = 0; i < MAX_DBC_ENTITIES; i++) {
			if (ue->thash[i] == -1)
				break;
		}

		if (i == MAX_DBC_ENTITIES) {
			DBCT->_DBCT_error_code = DBCELIMIT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Out of table id.")).str(SGLOCALE));
			return retval;
		}

		curte->user_id = te->user_id;
		curte->table_id = i;
		curte->flags = TE_IN_TEMPORARY | TE_IN_USE;
		curte->max_index = te->max_index;
		curte->cur_count = 0;
		(void)new (&curte->rwlock) bi::interprocess_upgradable_mutex();
		curte->te_meta = te->te_meta;
		curte->mem_used = -1;
		curte->row_used = -1;

		for (i = 0; i < MAX_VSLOTS; i++) {
			(void)new (&curte->mutex_free[i]) bi::interprocess_mutex();
			curte->row_free[i] = -1;
		}

		(void)new (&curte->mutex_ref) bi::interprocess_mutex();
		curte->row_ref = -1;

		for (i = 0; i < MAX_INDEXES; i++)
			curte->index_hash[i] = -1;

		for (i = 0; i < TE_TOTAL_STAT; i++)
			curte->stat_array[i] = te->stat_array[i];
	}

	for (i = 0; i <= te->max_index; i++) {
		te_mgr.set_ctx(te);

		dbc_ie_t *old_ie = te_mgr.get_index(i);
		if (old_ie == NULL)
			continue;

		long offset = bbp->alloc(sizeof(dbc_ie_t) + old_ie->ie_meta.buckets * sizeof(row_bucket_t));
		if (offset == -1) {
			DBCT->_DBCT_error_code = DBCEOS;
			DBCT->_DBCT_native_code = USBRK;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't allocate rows for index {1}") % old_ie->ie_meta.index_name).str(SGLOCALE));
			return retval;
		}

		dbc_ie_t *curie = reinterpret_cast<dbc_ie_t *>(reinterpret_cast<char *>(bbp) + offset);
		te_mgr.set_ctx(curte);
		ie_mgr.set_ctx(curie);
		curie->index_id = i;
		ie_mgr.init();
		curie->ie_meta = old_ie->ie_meta;
		curte->index_hash[i] = offset;
	}

	{
		// No update is allowed for original table while reloading
		bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);
		te->set_flags(TE_NO_UPDATE);
	}

	{
		ue->thash[curte->table_id] = reinterpret_cast<char *>(curte) - reinterpret_cast<char *>(DBCT->_DBCT_tes);
		DBCP->_DBCP_once_switch.set_switch(curte->table_id, DBCP->_DBCP_once_switch.get_switch(te->table_id));
		sdc_config& config_mgr = sdc_config::instance(DBCT);
		set<sdc_config_t>& config_set = config_mgr.get_config_set();

		sgc_ctx *SGC = SGT->SGC();
		sdc_config_t item;

		item.mid = SGC->_SGC_proc.mid;
		item.table_id = te->table_id;
		item.partition_id = 0;

		for (set<sdc_config_t>::iterator iter = config_set.lower_bound(item); iter != config_set.end(); ++iter) {
			if (iter->mid != item.mid || iter->table_id != item.table_id)
				break;

			sdc_config_t value = *iter;
			value.table_id = curte->table_id;
			config_set.insert(value);
		}

		BOOST_SCOPE_EXIT((&SGC)(&DBCP)(&ue)(&curte)(&config_set)) {
			sdc_config_t item;

			item.mid = SGC->_SGC_proc.mid;
			item.table_id = curte->table_id;
			item.partition_id = 0;
			for (set<sdc_config_t>::iterator iter = config_set.lower_bound(item); iter != config_set.end();) {
				if (iter->mid != item.mid || iter->table_id != item.table_id)
					break;

				set<sdc_config_t>::iterator tmp_iter = iter++;
				config_set.erase(tmp_iter);
			}

			DBCP->_DBCP_once_switch.set_switch(curte->table_id, NULL);
			ue->thash[curte->table_id] = -1;
		} BOOST_SCOPE_EXIT_END

		// Load data from db
		if (!load_table(curte))
			return retval;
	}

	// Switch to new table
	curte->table_id = te->table_id;
	curte->flags = TE_IN_USE;
	ue->thash[curte->table_id] = reinterpret_cast<char *>(curte) - reinterpret_cast<char *>(DBCT->_DBCT_tes);

	{
		// Mark removed, so dbc_server will clean it later
		bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);
		te->set_flags(TE_MARK_REMOVED);
	}

	retval = true;
	return retval;
}

bool dbc_server::unsafe_load(dbc_te_t *te)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}") % te->table_id).str(SGLOCALE), &retval);
#endif

	if (!DBCP->_DBCP_is_server && !DBCP->_DBCP_is_admin) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function can only be called by dbc_server or dbc_admin.")).str(SGLOCALE));
		return retval;
	}

	dbc_api& api_mgr = dbc_api::instance(DBCT);
	if (!api_mgr.truncate(te->te_meta.table_name)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't truncate table {1} - {2}.") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
		return retval;
	}

	// Load data from db
	if (!load_table(te))
		return retval;

	retval = true;
	return retval;
}

bool dbc_server::save_to_file()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	dbc_data_t& te_meta = te->te_meta;
	int i;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (!DBCP->_DBCP_is_server && !DBCP->_DBCP_is_admin) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function can only be called by dbc_server or dbc_admin.")).str(SGLOCALE));
		return retval;
	}

	boost::shared_array<dbc_partition_t> auto_partitions(new dbc_partition_t[te_meta.partitions]);
	dbc_partition_t *partitions = auto_partitions.get();

	dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
	string data_dir = string(bbparms.data_dir) + ue->user_name + "/";

	for (i = 0; i < te_meta.partitions; i++) {
		if (!partitions[i].open(data_dir, te_meta.table_name, i, false))
			return retval;
		if (!partitions[i].insert_field(get_fields_str(te_meta)))
				return retval;
	}

	// 异常情况下，清除创建的文件
	BOOST_SCOPE_EXIT((&retval)(&partitions)(&te_meta)) {
		if (!retval) {
			for (int i = 0; i < te_meta.partitions; i++)
				partitions[i].remove();
		}
	} BOOST_SCOPE_EXIT_END

	dbc_switch_t *dbc_switch = DBCT->_DBCT_dbc_switch;
	char key[1024];

	bi::sharable_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

	if (te->row_used != -1) {
		long rowid = te->row_used;
		row_link_t *link = DBCT->rowid2link(rowid);
		char *row_data = link->data();

		while (1) {
			(*dbc_switch->get_partition)(key, row_data);
			int partition_id = dbc_api::get_partition_id(te_meta.partition_type, te_meta.partitions, key);

			dbc_partition_t& partition = partitions[partition_id];
			if (!partition.insert(row_data, te_meta.internal_size + link->extra_size))
				return retval;

			if (link->next == -1)
				break;

			rowid = link->next;
			link = DBCT->rowid2link(rowid);
			row_data = link->data();
		}
	}

	retval = true;
	return retval;
}

void dbc_server::clean()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!DBCP->_DBCP_is_server && !DBCP->_DBCP_is_admin) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function can only be called by dbc_server or dbc_admin.")).str(SGLOCALE));
		return;
	}

	rte_clean();
	te_clean();
	chk_tran_timeout();
}

bool dbc_server::extend_shm(int add_count)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, (_("add_count={1}") % add_count).str(SGLOCALE), &retval);
#endif

	retval = DBCT->extend_shm(add_count);
	return retval;
}

bool dbc_server::login()
{
	scoped_usignal sig;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (DBCT->_DBCT_login) {
		retval = true;
		return retval;
	}

	if (!DBCP->_DBCP_is_server) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function can only be called by dbc_server.")).str(SGLOCALE));
		return retval;
	}

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);

	bool reuse_status;
	if (!DBCT->attach_shm()) {
		reuse_status = false;
	} else {
		DBCT->init_globals();
		if (!rte_mgr.get_slot())
			reuse_status = false;
		else
			reuse_status = true;
	}

	if (!reuse_status) { // Slot used by another process
		if (DBCT->_DBCT_error_code != DBCEOS || DBCT->_DBCT_native_code != USHMGET) {
			// Error is already handled in login().
			if (DBCT->_DBCT_error_code == DBCEDUPENT) { // Slot owner alive
				return retval;
			} else if (DBCP->_DBCP_reuse) { // reuse old shared memory
				dbc_rte_t *rte = DBCT->_DBCT_rtes + RTE_SLOT_SERVER;
				rte->flags = 0;

				if (!rte_mgr.get_slot()) {
					if (DBCT->_DBCT_error_code == DBCEDUPENT)
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: RTE is owned by another process.")).str(SGLOCALE));
					else if (DBCT->_DBCT_error_code == DBCENOSERVER)
						GPP->write_log(__FILE__, DBCESYSTEM, (_("ERROR: dbc_server is not alive.")).str(SGLOCALE));

					return retval;
				}

				DBCT->_DBCT_login = true;
				retval = true;
				return retval;
			}
		}
	} else { // Slot free
		if (DBCP->_DBCP_reuse) { // reuse old shared memory
			DBCT->_DBCT_login = true;
			retval = true;
			return retval;
		}
	}

	DBCT->_DBCT_login = false;

	if (!DBCT->create_shm())
		return retval;

	DBCT->init_globals();

	dbc_config& config_mgr = dbc_config::instance();
	const vector<data_index_t>& meta = config_mgr.get_meta();
	const vector<dbc_user_t>& users = config_mgr.get_users();

	if (!fill_shm(meta, users))
		return retval;

	if (!rte_mgr.get_slot())
		return retval;

	DBCT->_DBCT_login = true;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;

	(void)common::mkdir(NULL, bbparms.sys_dir);
	(void)common::mkdir(NULL, bbparms.sync_dir);
	(void)common::mkdir(NULL, bbparms.data_dir);

	if (DBCP->_DBCP_proc_type == DBC_STANDALONE) {
		if (!load())
			return retval;
	}

	if (DBCP->_DBCP_enable_sync) {
		// Startup dbc_sync
		string cmd;
		char *ptr = ::getenv("DBCROOT");
		if (ptr == NULL) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: DBCROOT environment variable not set")).str(SGLOCALE));
			return retval;
		}
		cmd += ptr;
		if (*cmd.rbegin() != '/')
			cmd += '/';
		cmd += "dbc_sync -b";

		sys_func& func_mgr = sys_func::instance();
		if (func_mgr.gp_status(::system(cmd.c_str()))) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't startup dbc_sync - {1}") % strerror(errno)).str(SGLOCALE));
			return retval;
		}

		GPP->write_log((_("INFO: dbc_server started successfully.")).str(SGLOCALE));
	}

	signal(bbparms.post_signo, dbct_ctx::bb_change);

	retval = true;
	return retval;
}

void dbc_server::logout()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!DBCT->_DBCT_login || !DBCP->_DBCP_is_server)
		return;

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmap_t& bbmap = bbp->bbmap;
	signal(bbparms.post_signo, SIG_DFL);

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	rte_mgr.set_flags(RTE_MARK_REMOVED);

	{
		bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);
		for (dbc_rte_t *rte = DBCT->_DBCT_rtes; rte < DBCT->_DBCT_rtes + bbparms.maxrtes; rte++) {
			if (!rte->in_flags(RTE_IN_USE) || rte->in_flags(RTE_MARK_REMOVED))
				continue;

			kill(rte->pid, bbparms.post_signo);
		}
	}

	// 如果可能，删除共享内存
	for (int i = 0; i < bbparms.shm_count; i++)
		::shmctl(bbmap.shmids[i], IPC_RMID, 0);

	rte_mgr.put_slot();
	DBCT->_DBCT_login = false;
}

bool dbc_server::load()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_te_t *te;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (!DBCP->_DBCP_is_server && !DBCP->_DBCP_is_admin) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function can only be called by dbc_server or dbc_admin.")).str(SGLOCALE));
		return retval;
	}

	GPP->write_log((_("INFO: Transfer source to internal format...")).str(SGLOCALE));

	// 加载序列
	load_sequences();

	int parallel_threads = 0;
	boost::thread_group thread_group;
	vector<int> retvals;
	for (int i = 0; i <= bbmeters.curmaxte; i++)
		retvals.push_back(0);

	int idx = 0;
	for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++, idx++) {
		if (!te->in_persist())
			continue;

		if (!te->in_flags(TE_READONLY)) {
			dbc_te& te_mgr = dbc_te::instance(DBCT);

			te_mgr.set_ctx(te);
			if (!te_mgr.primary_key() < 0) {
				DBCT->_DBCT_error_code = DBCESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't load data for table {1}, missing primary key") % te->te_meta.table_name).str(SGLOCALE));
				return retval;
			}
		}

		// 允许二进制方式加载时，才需要起单独的线程来加载数据
		if (!(te->te_meta.refresh_type & REFRESH_TYPE_BFILE))
			continue;

		// 从数据库或文件加载时，才需要起单独的线程来加载数据
		if (!(te->te_meta.refresh_type & (REFRESH_TYPE_DB | REFRESH_TYPE_FILE))
			|| (DBCP->_DBCP_proc_type & DBC_SLAVE))
			continue;

		dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
		string data_dir = string(bbparms.data_dir) + ue->user_name + "/";

		string filename = data_dir + te->te_meta.table_name + "-0.b";
		if (DBCP->_DBCP_proc_type == DBC_MASTER) {
			sgc_ctx *SGC = SGT->SGC();

			filename += ".";
			filename += SGC->mid2lmid(SGC->_SGC_proc.mid);
		}

		if (access(filename.c_str(), R_OK | W_OK) != -1) // file exists
			continue;

		dbc_switch_t *dbc_switch = DBCP->_DBCP_once_switch.get_switch(te->table_id);
		retvals[idx] = -1;
		thread_group.create_thread(boost::bind(&dbc_server::s_to_bfile, dbc_switch, te, boost::ref(retvals[idx])));

		if (++parallel_threads == PER_BATCH) {
			thread_group.join_all();
			parallel_threads = 0;
		}
	}

	thread_group.join_all();
	for (idx = 0; idx <= bbmeters.curmaxte; idx++) {
		if (retvals[idx] == -1) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			const dbc_te_t *err_te = DBCT->_DBCT_tes + idx;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't load data for table {1}") % err_te->te_meta.table_name).str(SGLOCALE));
			return retval;
		}
	}

	// Master节点需要分发文件、创建ready文件，这样Slave节点可以继续
	if (DBCP->_DBCP_proc_type == DBC_MASTER) {
		sgc_ctx *SGC = SGT->SGC();
		const int& local_mid = SGC->_SGC_proc.mid;
		string local_lmid = SGC->mid2lmid(local_mid);
		sdc_config& config_mgr = sdc_config::instance(DBCT);
		set<sdc_config_t>& config_set = config_mgr.get_config_set();
		set<dbc_scp_t> scp_set;
		map<int, vector<string> > nodes_map;
		sg_config& cfg_mgr = sg_config::instance(SGT);
		vector<string> commands;

		// 首先获取主机IP列表
		for (sg_config::net_iterator net_iter = cfg_mgr.net_begin(); net_iter != cfg_mgr.net_end(); ++net_iter) {
			string naddr = net_iter->naddr;
			if (memcmp(naddr.c_str(), "//", 2) != 0)
				continue;

			string::size_type pos = naddr.find(":");
			if (pos == string::npos)
				continue;

			string address = naddr.substr(2, pos - 2);
			nodes_map[SGC->lmid2mid(net_iter->lmid)].push_back(address);
		}

		for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++, idx++) {
			dbc_data_t& te_meta = te->te_meta;
			dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
			string data_dir = string(bbparms.data_dir) + ue->user_name + "/";

			if (!(te->te_meta.refresh_type & REFRESH_TYPE_BFILE))
				continue;

			if (te->te_meta.refresh_type & REFRESH_TYPE_DB)
				continue;

			if (te->in_flags(TE_IN_MEM))
				continue;

			for (int partition_id = 0; partition_id < te_meta.partitions; partition_id++) {
				string filename = (boost::format("%1%-%2%.b.%3%") % te_meta.table_name % partition_id % local_lmid).str();
				string db_name = data_dir + filename;

				if (access(db_name.c_str(), R_OK) == -1)
					continue;

				bool gzip_flag = false;
				for (set<sdc_config_t>::const_iterator iter = config_set.begin(); iter != config_set.end(); ++iter) {
					if (iter->table_id != te->table_id)
						continue;

					if (iter->partition_id != partition_id)
						continue;

					if (!SGC->remote(iter->mid))
						continue;

					struct stat statbuf;
					if (stat(db_name.c_str(), &statbuf) == -1) {
						DBCT->_DBCT_error_code = DBCESYSTEM;
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't stat file {1} - {2}") % db_name % strerror(errno)).str(SGLOCALE));
						return retval;
					}

					// 程序启动后没有更改过，就不需要拷贝
					if (statbuf.st_mtime < login_time)
						continue;

					dbc_scp_t item;
					map<int, vector<string> >::const_iterator nodes_iter = nodes_map.find(iter->mid);
					if (nodes_iter == nodes_map.end()) {
						DBCT->_DBCT_error_code = DBCESYSTEM;
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find address for hostid {1}") % iter->mid).str(SGLOCALE));
						return retval;
					}

					item.mid = iter->mid;
					item.pmids = nodes_iter->second;
					item.data_dir = data_dir;
					item.filename = filename;

					scp_set.insert(item);
					gzip_flag = true;
				}

				if (gzip_flag) {
					string cmd = string("gzip -f ") + db_name;
					commands.push_back(cmd);
				}
			}
		}

		for (int i = 0; i < commands.size(); i += PER_BATCH) {
			int batch = i + PER_BATCH <= commands.size() ? PER_BATCH : commands.size() - i;
			pid_t pids[PER_BATCH];
			int status = 0;
			pid_t ret_code;
			sig_action sigchld(SIGCHLD, SIG_DFL);
			sig_action sigpipe(SIGPIPE, SIG_IGN);

			for (idx = 0; idx < batch; idx++) {
				pid_t pid = ::fork();
				switch (pid) {
				case 0:
					ret_code = ::execl("/bin/sh", "/bin/sh", "-c", commands[i + idx].c_str(), (char *)0);
					::exit(ret_code);
				case -1:
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't execute command {1} - {2}") % commands[i + idx] % strerror(errno)).str(SGLOCALE));
					DBCT->_DBCT_error_code = DBCEOS;
					return retval;
				default:
					pids[idx] = pid;
					break;
				}
			}

			sys_func& func_mgr = sys_func::instance();
			int wait_count = batch;
			while (wait_count > 0 && (ret_code = wait(&status)) != -1) {
				for (idx = 0; idx < batch; idx++) {
					if (ret_code == pids[idx]) {
						int exit_code = func_mgr.gp_status(status);
						if (exit_code != 0 && exit_code != 2) {	// 0: 正确, 2: 警告
							GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't execute command {1}, retval = {2}") % commands[i + idx] % status).str(SGLOCALE));
							return retval;
						}
						pids[idx] = -1;
						wait_count--;
					}
				}

				status = 0;
			}
		}

		// 关闭压缩方式，采用自定义压缩
		sshp_ctx *SSHP = sshp_ctx::instance();
		SSHP->_SSHP_compress = false;

		gpenv& env_mgr = gpenv::instance();
		boost::random::rand48 rand48(local_mid);
		int pmid_idx = rand48();

		char *ptr = env_mgr.getenv("LOGNAME");
		if (ptr == NULL) {
			DBCT->_DBCT_error_code = DBCEOS;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: environment variable LOGNAME not set")).str(SGLOCALE));
			return retval;
		}
		string usrname = ptr;

		for (set<dbc_scp_t>::const_iterator siter = scp_set.begin(); siter != scp_set.end(); ++siter) {
			string pmid = siter->pmids[++pmid_idx % siter->pmids.size()];
			string full_name = siter->data_dir;
			full_name += siter->filename;
			full_name += ".gz";

			string sftp_prefix = usrname;
			sftp_prefix += "@";
			sftp_prefix += pmid;

			ifstream ifs(full_name.c_str());
			if (!ifs) {
				DBCT->_DBCT_error_code = DBCEOS;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1}") % full_name).str(SGLOCALE));
				return retval;
			}

			osftpstream osftp(sftp_prefix, full_name);
			if (!osftp) {
				DBCT->_DBCT_error_code = DBCEOS;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1} on node {2}") % full_name % pmid).str(SGLOCALE));
				return retval;
			}

			osftp << ifs.rdbuf();
		}

		string ready_file = DBCT->get_ready_file(SGC->mid2lmid(SGC->_SGC_proc.mid));
		for (vector<pair<int, string> >::const_iterator iter = DBCP->_DBCP_mlist.begin(); iter != DBCP->_DBCP_mlist.end(); ++iter) {
			boost::shared_ptr<ostream> os;

			if (!SGC->remote(iter->first)) {
				os.reset(new ofstream(ready_file.c_str()));
			} else {
				string sftp_prefix = usrname;
				sftp_prefix += "@";
				sftp_prefix += iter->second;
				os.reset(new osftpstream(sftp_prefix, ready_file));
			}

			if (!*os) {
				DBCT->_DBCT_error_code = DBCEOS;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open ready file {1} on node {2}") % ready_file % iter->second).str(SGLOCALE));
				return retval;
			}

			os->write(reinterpret_cast<char *>(&SGC->_SGC_proc.mid), sizeof(SGC->_SGC_proc.mid));
			os->write(reinterpret_cast<char *>(&DBCP->_DBCP_current_pid), sizeof(DBCP->_DBCP_current_pid));
			if (!*os) {
				DBCT->_DBCT_error_code = DBCEOS;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't write to ready file {1} on node {2}") % ready_file % iter->second).str(SGLOCALE));
				return retval;
			}
		}

		SSHP->clear();
	} else if (DBCP->_DBCP_proc_type == DBC_SLAVE) {
		// 等待文件可用
		wait_ready();
	}

	GPP->write_log((_("INFO: Transfer finished, Loading...")).str(SGLOCALE));

	parallel_threads = 0;
	if (!(DBCP->_DBCP_proc_type & DBC_MASTER)) {
		dbc_te& te_mgr = dbc_te::instance(DBCT);
		dbc_ie& ie_mgr = dbc_ie::instance(DBCT);

		retvals.clear();
		for (int i = 0; i <= bbmeters.curmaxte; i++)
			retvals.push_back(0);

		idx = 0;
		for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++, idx++) {
			if (te->in_flags(TE_IN_MEM)) {
				te_mgr.set_ctx(te);
				for (int i = 0; i <= te->max_index; i++) {
					dbc_ie_t *ie = te_mgr.get_index(i);
					if (ie == NULL)
						continue;

					ie_mgr.set_ctx(ie);
					ie_mgr.init();
				}

				te->version = 0;
				te->cur_count = 0;
				if (te->table_id == TE_DUAL) {
					// 插入表dual
					// 创建记录区数据
					long rowid = te_mgr.create_row(0);
					if (rowid == -1) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create row for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
						return retval;
					}

					char *row_data = DBCT->rowid2data(rowid);
					memcpy(row_data, "X", 2);
					te_mgr.unlock_row(rowid);
				}
			} else if (te->in_active()) {
				retvals[idx] = -1;
				thread_group.create_thread(boost::bind(&dbc_server::s_load_table, te, boost::ref(retvals[idx])));

				if (++parallel_threads == PER_BATCH) {
					thread_group.join_all();
					parallel_threads = 0;
				}
			}
		}

		thread_group.join_all();
		for (idx = 0; idx <= bbmeters.curmaxte; idx++) {
			if (retvals[idx] == -1) {
				DBCT->_DBCT_error_code = DBCESYSTEM;
				return retval;
			}
		}

		/* 加载变更日志，加载的日志不应该再做持久化操作，
		 * 因此，需要先把非内存表都标记为内存表，
		 * 等加载完成后，再恢复回来
		 */

		dbc_change_t item;
		vector<dbc_change_t> changes;
		for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
			if (!te->in_flags(TE_IN_MEM)) {
				item.te = te;
				item.readonly = (te->flags & TE_READONLY);
				changes.push_back(item);

				te->set_flags(TE_IN_MEM);
				if (item.readonly)
					te->clear_flags(TE_READONLY);
			}
		}

		dbc_sqlite& sqlite_mgr = dbc_sqlite::instance(DBCT);
		if (!sqlite_mgr.load()) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't apply change log to current DBC")).str(SGLOCALE));
			return retval;
		}

		BOOST_FOREACH(dbc_change_t& change, changes) {
			dbc_te_t *te = change.te;

			te->clear_flags(TE_IN_MEM);
			if (change.readonly)
				te->set_flags(TE_READONLY);

			dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
			GPP->write_log((_("INFO: table {1} on user {2} apply change log finished, table rows: {3}") % te->te_meta.table_name % ue->user_name % te->cur_count).str(SGLOCALE));
		}
	}

	GPP->write_log((_("INFO: Loading finished.")).str(SGLOCALE));

	// 设置加载完毕
	bbmeters.load_mutex.lock();
	bbmeters.load_ready = true;
	bbmeters.load_cond.notify_all();
	bbmeters.load_mutex.unlock();

	retval = true;
	return retval;
}

void dbc_server::watch()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(400, __PRETTY_FUNCTION__, "", NULL);
#endif

	// Only server is allowed to do clean staff
	if (!DBCP->_DBCP_is_server)
		return;

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	int sanity_scan = 0;

	while (!SGP->_SGP_shutdown) {
		{
			scoped_usignal sig;
			bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

			if (++sanity_scan == bbparms.sanity_scan) { // 检查是否有死掉的注册节点
				rte_clean();
				te_clean();
				sanity_scan = 0;
			}

			for (dbc_te_t *te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
				if (!te->in_persist())
					continue;

				refresh_table(te);
			}

			// 检查是否有超时的事务
			chk_tran_timeout();
		}

		sleep(bbparms.scan_unit);
		if (SGP->_SGP_shutdown)
			return;
	}
}

bool dbc_server::fill_shm(const vector<data_index_t>& meta, const vector<dbc_user_t>& users)
{
	int i;
	dbc_ue_t *ue;
	dbc_te_t *te;
	dbc_se_t *se;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_redo& redo_mgr = dbc_redo::instance(DBCT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", &retval);
#endif

	// 初始化UEs
	ue_mgr.set_ctx(0);
	for (i = 0, ue = DBCT->_DBCT_ues; i < bbparms.maxusers; i++, ue++) {
		ue->user_id = -1;
		ue->flags = 0;
	}

	// 初始化TEs
	for (i = 0, te = DBCT->_DBCT_tes; i < bbparms.maxtes; i++, te++) {
		te->table_id = i;
		te->flags = 0;
	}

	for (i = 0, se = DBCT->_DBCT_ses; i < bbparms.maxses; i++, se++)
		se->init();

	// 初始化RTEs
	for (i = 0; i < bbparms.maxrtes; i++) {
		rte_mgr.set_ctx(DBCT->_DBCT_rtes + i);
		rte_mgr.init();
	}

	redo_mgr.init();

	// 创建用户
	if (!create_users(users)) {
		// error_code already set
		return retval;
	}

	te = DBCT->_DBCT_tes;
	ue = DBCT->_DBCT_ues;
	int te_idx = 0;

	for (int i = 0; i < users.size(); i++, ue++) {
		BOOST_FOREACH(const data_index_t& index, meta) {
			bool require_create;
			const int& table_id = index.data.table_id;

			// 系统表对所有用户只有一个
			if (table_id >= TE_MIN_RESERVED)
				require_create = (i == 0);
			else
				require_create = (i != 0);

			if (require_create) {
				te->user_id = ue->user_id;
				te_mgr.set_ctx(te);
				if (!te_mgr.init(te, index.data, index.index_vector)) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: failed to initialize table {1}") % te->te_meta.table_name).str(SGLOCALE));
					return retval;
				}

				ue->thash[te->table_id] = reinterpret_cast<char *>(te) - reinterpret_cast<char *>(DBCT->_DBCT_tes);
				bbmeters.curtes++;
				if (bbmeters.curmaxte < te_idx)
					bbmeters.curmaxte = te_idx;

				te++;
				te_idx++;
			} else if (i > 0) {
				// 只需引用系统用户下的表
				ue->thash[table_id] = DBCT->_DBCT_ues->thash[table_id];
			}
		}
	}

	retval = true;
	return retval;
}

bool dbc_server::load_table(dbc_te_t *te)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}") % te->table_id).str(SGLOCALE), &retval);
#endif

	if (!DBCP->_DBCP_is_server && !DBCP->_DBCP_is_admin) {
		DBCT->_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function can only be called by dbc_server or dbc_admin.")).str(SGLOCALE));
		return retval;
	}

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	bool status;

	te_mgr.set_ctx(te);

	// 初始化索引区
	for (int i = 0; i <= te->max_index; i++) {
		dbc_ie_t *ie = te_mgr.get_index(i);
		if (ie == NULL)
			continue;

		ie_mgr.set_ctx(ie);
		ie_mgr.init();
	}

	int saved_flags = te->flags;
	te->flags |= TE_IN_MEM;
	BOOST_SCOPE_EXIT((&te)(&saved_flags)) {
		te->flags = saved_flags;
	} BOOST_SCOPE_EXIT_END

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	ue_mgr.set_ctx(te->user_id & UE_ID_MASK);

	// 优先从本地二进制文件加载
	if (te->te_meta.refresh_type & REFRESH_TYPE_BFILE) {
		status = load_from_bfile(!(te->te_meta.refresh_type & (REFRESH_TYPE_FILE | REFRESH_TYPE_DB)) || (DBCP->_DBCP_proc_type & DBC_SLAVE));
		if (usignal::get_pending())
			return retval;

		if (status) {
			goto SKIP_LOAD;
		} else {
			// Slave模式下只允许从二进制文件加载
			if (DBCP->_DBCP_proc_type & DBC_SLAVE)
				return retval;
		}
	}

	// 其次从本地文本文件加载
	if (te->te_meta.refresh_type & REFRESH_TYPE_FILE) {
		status = load_from_file(!(te->te_meta.refresh_type & REFRESH_TYPE_DB));
		if (usignal::get_pending())
			return retval;

		if (status)
			goto SKIP_LOAD;
	}

	// 最后从数据库加载
	if (te->te_meta.refresh_type & REFRESH_TYPE_DB) {
		status = load_from_db();
		if (usignal::get_pending())
			return retval;

		if (status)
			goto SKIP_LOAD;
	}

	DBCT->_DBCT_error_code = DBCESYSTEM;
	GPP->write_log(__FILE__, __LINE__, (_("ERROR: table {1}: unsupported refresh_type given.") % te->te_meta.table_name).str(SGLOCALE));
	return retval;

SKIP_LOAD:
	GPP->write_log((_("INFO: table {1} load finished, table rows: {2}") % te->te_meta.table_name % te->cur_count).str(SGLOCALE));

	if (te->te_meta.refresh_type & (REFRESH_TYPE_AGG | REFRESH_TYPE_DISCARD)) {
		// 加载完成，而且已经创建索引
		retval = true;
		return retval;
	}

	for (int i = 0; i <= te->max_index; i++) {
		dbc_ie_t *ie = te_mgr.get_index(i);
		if (ie == NULL)
			continue;

		ie_mgr.set_ctx(ie);

		if (te->row_used != -1) {
			long rowid = te->row_used;
			char *row_data = DBCT->rowid2data(rowid);
			while (1) {
				if (!ie_mgr.create_row(rowid)) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: index {1}: Can't create index row - {2}") % ie->ie_meta.index_name % DBCT->strerror()).str(SGLOCALE));
					return retval;
				}

				row_link_t *link = DBCT->rowid2link(rowid);
				if (link->next == -1)
					break;

				rowid = link->next;
				row_data = DBCT->rowid2data(rowid);
			}
		}

		GPP->write_log((_("INFO: index {1} create finished") % ie->ie_meta.index_name).str(SGLOCALE));
	}

	retval = true;
	return retval;
}

void dbc_server::refresh_table(dbc_te_t *te)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}") % te->table_id).str(SGLOCALE), NULL);
#endif

	if (!(te->te_meta.refresh_type & REFRESH_TYPE_INCREMENT))
		return;

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	te_mgr.set_ctx(te);

	if (te->te_meta.refresh_type & REFRESH_TYPE_DB)
		refresh_from_db();
	else if (te->te_meta.refresh_type & REFRESH_TYPE_BFILE)
		refresh_from_bfile();

	GPP->write_log((_("INFO: table {1} refresh finished, table rows: {2}") % te->te_meta.table_name % te->cur_count).str(SGLOCALE));
}

bool dbc_server::create_users(const vector<dbc_user_t>& users)
{
	bool retval = true;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);

	BOOST_FOREACH(const dbc_user_t& user, users) {
		if (ue_mgr.create_user(user) == -1) {
			retval = false;
			return retval;
		}
	}

	return retval;
}

void dbc_server::load_sequences()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	string full_name;
	vector<string> file_vector;
 	scan_file<> sfile(bbparms.sys_dir, "^.*\\.[0-9]+\\.s$", REG_ICASE);

	sfile.get_files(file_vector);
	if (file_vector.empty())
		return;

	full_name = bbparms.sys_dir;
	full_name += "sequence.dat";

	auto_ptr<long> auto_data(new long[bbparms.maxses]);
	long *init_data = auto_data.get();
	memset(init_data, 0xFF, sizeof(long) * bbparms.maxses);
	{
		ifstream ifs(full_name.c_str());
		if (!ifs) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't load definition for sequence file {1} - {2}.") % full_name % ::strerror(errno)).str(SGLOCALE));
			return;
		}

		ifs.read(reinterpret_cast<char *>(init_data), sizeof(long) * bbparms.maxses);
	}

	for (vector<string>::const_iterator iter = file_vector.begin(); iter != file_vector.end(); ++iter) {
		full_name = bbparms.sys_dir;
		full_name += *iter;
		ifstream ifs(full_name.c_str());
		if (!ifs) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't load definition for sequence file {1} - {2}.") % full_name % ::strerror(errno)).str(SGLOCALE));
			continue;
		}

		dbc_se_t se_node;
		se_node.init();

		int user_idx = -1;
		int slot_id = -1;
		string str;
		while (std::getline(ifs, str)) {
			string::size_type pos = str.find('=');
			if (pos == string::npos)
				continue;

			string key = str.substr(0, pos);
			string value = str.substr(pos + 1);

			if (key == "user_idx")
				user_idx = atoi(value.c_str());
			else if (key == "slot_id")
				slot_id = atoi(value.c_str());
			else if (key == "seq_name")
				strcpy(se_node.seq_name, value.c_str());
			else if (key == "minval")
				se_node.minval = atol(value.c_str());
			else if (key == "maxval")
				se_node.maxval = atol(value.c_str());
			else if (key == "currval")
				se_node.currval = atol(value.c_str());
			else if (key == "increment")
				se_node.increment = atol(value.c_str());
			else if (key == "cache")
				se_node.cache = atol(value.c_str());
			else if (key == "cycle")
				se_node.cycle = (atoi(value.c_str() )== 1);
			else if (key == "order")
				se_node.order = (atoi(value.c_str()) == 1);
		}

		if (user_idx < 0 || user_idx >= bbparms.maxusers
			|| se_node.seq_name[0] == '\0' || se_node.minval < 0 || se_node.maxval <= se_node.minval
			|| se_node.currval < se_node.minval || se_node.currval > se_node.maxval || slot_id < 0
			|| slot_id >= bbparms.maxses) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Corrupted definition for sequence file {1}, skip it.") % full_name).str(SGLOCALE));
			continue;
		}

		dbc_se_t *se = DBCT->_DBCT_ses + slot_id;
		if (se->flags & SE_IN_USE) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Corrupted definition for sequence file {1}, skip it.") % full_name).str(SGLOCALE));
			continue;
		}

		if (init_data[slot_id] != -1)
			se_node.currval = init_data[slot_id];

		*se = se_node;
		se->flags |= SE_IN_USE;
		se->accword = 0;
		se->prev = -1;

		dbc_ue_t *ue = DBCT->_DBCT_ues + user_idx;
		shash_t *shash = ue->shash;
		int hash_value = DBCT->get_hash(se_node.seq_name, bbparms.sebkts);

		se->next = shash[hash_value];

		if (shash[hash_value] != -1) {
			dbc_se_t *head = DBCT->_DBCT_ses + shash[hash_value];
			head->prev = slot_id;
		}
		shash[hash_value] = slot_id;
	}
}

bool dbc_server::load_from_db()
{
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	dbc_data_t& te_meta = te->te_meta;
	string sql;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	auto_ptr<Generic_Database> auto_db(connect(te->user_id & UE_ID_MASK, te_meta));
	Generic_Database *db = auto_db.get();

	auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(db->create_data(true));
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = USBRK;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failed")).str(SGLOCALE));
		return retval;
	}

	if (!get_sql(te_meta, sql, false))
		return retval;

	data->setSQL(sql);
	stmt->bind(data);

	auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
	Generic_ResultSet *rset = auto_rset.get();
	int i;

	// 设置表记录数
	te->cur_count = 0;

	if (DBCP->_DBCP_proc_type & DBC_STANDALONE) {
		dbc_api& api_mgr = dbc_api::instance(DBCT);
		boost::shared_array<char> auto_input_row(new char[te_meta.struct_size]);
		boost::shared_array<char> auto_output_row(new char[te_meta.struct_size]);
		char *input_row = auto_input_row.get();
		char *output_row = auto_output_row.get();
		int primary_key = te_mgr.primary_key();

		// 单机版加载
		while (rset->next()) {
			if (te_meta.refresh_type & REFRESH_TYPE_AGG) {
				get_struct(te_meta, input_row, rset);

				if (api_mgr.find(te->table_id, primary_key, input_row, output_row, 1) > 0) {
					agg_struct(input_row, output_row, te_meta);
					if (!api_mgr.update_by_rowid(te->table_id, api_mgr.get_rowid(0), input_row)) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't update row for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
						return retval;
					}
				} else {
					if (!api_mgr.insert(te->table_id, input_row)) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert row for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
						return retval;
					}
				}
			} else if (te_meta.refresh_type & REFRESH_TYPE_DISCARD) {
				get_struct(te_meta, input_row, rset);

				if (!api_mgr.insert(te->table_id, input_row))
					continue;
			} else {
				int extra_size;
				if (!te->in_flags(TE_VARIABLE))
					extra_size = 0;
				else
					extra_size = get_vsize(te_meta, rset);

				// 创建记录区数据
				long rowid = te_mgr.create_row(extra_size);
				if (rowid == -1) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create row for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
					return retval;
				}

				char *row_data = DBCT->rowid2data(rowid);
				get_record(te_meta, row_data, rset);

				te_mgr.unlock_row(rowid);
			}

			if (usignal::get_pending()) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: process terminated by signal")).str(SGLOCALE));
				return retval;
			}
		}

		// 允许从二进制文件加载，则从数据库加载后，需要保存
		if (te_meta.refresh_type & REFRESH_TYPE_BFILE) {
			if (!save_to_file()) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't save data to file for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				return retval;
			}
		}
	} else if (DBCP->_DBCP_proc_type & DBC_MASTER) {
		// Master节点上加载，只做从数据库到二进制文件的转换，
		// 不加载到内存中，防止主节点上的内存不足
		dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
		dbc_bbparms_t& bbparms = bbp->bbparms;
		boost::shared_array<char> auto_data(new char[te_meta.internal_size + te_meta.extra_size]);
		char *row_data = auto_data.get();

		boost::shared_array<dbc_partition_t> auto_partitions(new dbc_partition_t[te_meta.partitions]);
		dbc_partition_t *partitions = auto_partitions.get();

		dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
		string data_dir = string(bbparms.data_dir) + ue->user_name + "/";

		for (i = 0; i < te_meta.partitions; i++) {
			if (!partitions[i].open(data_dir, te_meta.table_name, i, false))
				return retval;
			if (!partitions[i].insert_field(get_fields_str(te_meta)))
				return retval;
		}

		// 关闭文件，异常情况下，清除创建的文件
		BOOST_SCOPE_EXIT((&retval)(&partitions)(&te_meta)) {
			if (!retval) {
				for (int i = 0; i < te_meta.partitions; i++)
					partitions[i].remove();
			}
		} BOOST_SCOPE_EXIT_END

		dbc_switch_t *dbc_switch = DBCT->_DBCT_dbc_switch;
		char key[1024];

		while (rset->next()) {
			int extra_size = get_record(te_meta, row_data, rset);

			(*dbc_switch->get_partition)(key, row_data);
			int partition_id = dbc_api::get_partition_id(te_meta.partition_type, te_meta.partitions, key);

			dbc_partition_t& partition = partitions[partition_id];
			if (!partition.insert(row_data, te_meta.internal_size + extra_size))
				return retval;

			if (usignal::get_pending()) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: process terminated by signal")).str(SGLOCALE));
				return retval;
			}
		}
	} else {
		// Slave节点上加载，不允许的操作
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error.")).str(SGLOCALE));
		SGT->_SGT_error_code = SGEINVAL;
		return retval;
	}

	te->version++;

	retval = true;
	return retval;
}

string dbc_server::get_fields_str(const dbc_data_t& te_meta)
{
	string fields_str;
	for (int i=0; i<te_meta.field_size; i++)
		fields_str += (boost::format("%1%(%2%)") % te_meta.fields[i].field_type % te_meta.fields[i].field_size).str();

	return fields_str;
}

bool dbc_server::load_from_file(bool log_error)
{
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	dbc_data_t& te_meta = te->te_meta;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	int i;

	// 设置表记录数
	te->cur_count = 0;

	dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
	string data_dir = string(bbparms.data_dir) + ue->user_name + "/";
	scan_file<> fscan(data_dir, string("^") + te_meta.table_name + "\\.seq\\.[0-9]+$", REG_ICASE);

	int record_size = 1;
	for (i = 0; i < te_meta.field_size; i++) {
		record_size += te_meta.fields[i].load_size;
		if (te_meta.delimiter != '\0') // 有分隔符
			record_size++;
	}
	boost::shared_array<char> auto_buf(new char[record_size]);
	char *buf = auto_buf.get();
	bool fixed = (te_meta.delimiter == '\0');
	string buf_str;

	vector<string> files;
	fscan.get_files(files);

	if (DBCP->_DBCP_proc_type & DBC_STANDALONE) {
		dbc_api& api_mgr = dbc_api::instance(DBCT);
		boost::shared_array<char> auto_input_row(new char[te_meta.struct_size]);
		boost::shared_array<char> auto_output_row(new char[te_meta.struct_size]);
		char *input_row = auto_input_row.get();
		char *output_row = auto_output_row.get();
		int primary_key = te_mgr.primary_key();

		BOOST_FOREACH(const string& filename, files) {
			string full_name = data_dir + filename;

			FILE *fp = fopen(full_name.c_str(), "r");
			if (fp == NULL) {
				if (log_error)
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
				return retval;
			}

			BOOST_SCOPE_EXIT((&fp)) {
				fclose(fp);
			} BOOST_SCOPE_EXIT_END

			while (!usignal::get_pending()) {
				int status;
				if (fixed)
					status = (fread(buf, record_size, 1, fp) == 1);
				else
					status = (fgets(buf, record_size - 1, fp) != NULL);

				if (!status) {
					if (feof(fp))
						break;

					if (log_error)
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't read record from file {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
					return retval;
				}

				if (!fixed) {
					buf_str = buf;
					if (!buf_str.empty() && *buf_str.rbegin() == '\n')
						buf_str.resize(buf_str.length() - 1);
				}

				if (te_meta.refresh_type & REFRESH_TYPE_AGG) {
					if (fixed)
						DBCT->fixed2struct(buf, input_row, te_meta);
					else
						DBCT->variable2struct(buf_str, input_row, te_meta);

					if (api_mgr.find(te->table_id, primary_key, input_row, output_row, 1) > 0) {
						agg_struct(input_row, output_row, te_meta);
						if (!api_mgr.update_by_rowid(te->table_id, api_mgr.get_rowid(0), input_row)) {
							if (log_error)
								GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't update row for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
							return retval;
						}
					} else {
						if (!api_mgr.insert(te->table_id, input_row)) {
							if (log_error)
								GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert row for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
							return retval;
						}
					}
				} else if (te_meta.refresh_type & REFRESH_TYPE_DISCARD) {
					if (fixed)
						DBCT->fixed2struct(buf, input_row, te_meta);
					else
						DBCT->variable2struct(buf_str, input_row, te_meta);

					if (!api_mgr.insert(te->table_id, input_row))
						continue;
				} else {
					int extra_size;
					if (!te->in_flags(TE_VARIABLE))
						extra_size = 0;
					else if (fixed)
						extra_size = get_vsize(buf, te_meta);
					else
						extra_size = get_vsize(buf_str, te_meta);

					// 创建记录区数据
					long rowid = te_mgr.create_row(extra_size);
					if (rowid == -1) {
						if (log_error)
							GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create row for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
						return retval;
					}

					char *row_data = DBCT->rowid2data(rowid);
					if (fixed)
						DBCT->fixed2record(buf, row_data, te_meta);
					else
						DBCT->variable2record(buf_str, row_data, te_meta);

					te_mgr.unlock_row(rowid);
				}
			}

			if (usignal::get_pending()) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: process terminated by signal")).str(SGLOCALE));
				return retval;
			}
		}

		if (te_meta.refresh_type & REFRESH_TYPE_BFILE) {
			if (!save_to_file()) {
				if (log_error)
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't save data to file for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				return retval;
			}
		}
	} else if (DBCP->_DBCP_proc_type & DBC_MASTER) {
		// Master节点上加载，只做从文本文件到二进制文件的转换，
		// 不加载到内存中，防止主节点上的内存不足
		boost::shared_array<char> auto_data(new char[te_meta.internal_size + te_meta.extra_size]);
		char *row_data = auto_data.get();

		boost::shared_array<dbc_partition_t> auto_partitions(new dbc_partition_t[te_meta.partitions]);
		dbc_partition_t *partitions = auto_partitions.get();

		dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
		string data_dir = string(bbparms.data_dir) + ue->user_name + "/";

		for (i = 0; i < te_meta.partitions; i++) {
			if (!partitions[i].open(data_dir, te_meta.table_name, i, false))
				return retval;
			if (!partitions[i].insert_field(get_fields_str(te_meta)))
				return retval;
		}

		// 关闭文件，异常情况下，清除创建的文件
		BOOST_SCOPE_EXIT((&retval)(&partitions)(&te_meta)) {
			if (!retval) {
				for (int i = 0; i < te_meta.partitions; i++)
					partitions[i].remove();
			}
		} BOOST_SCOPE_EXIT_END

		dbc_switch_t *dbc_switch = DBCT->_DBCT_dbc_switch;
		char key[1024];

		BOOST_FOREACH(const string& filename, files) {
			string full_name = data_dir + filename;

			FILE *fp = fopen(full_name.c_str(), "r");
			if (fp == NULL) {
				if (log_error)
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
				return retval;
			}

			BOOST_SCOPE_EXIT((&fp)) {
				fclose(fp);
			} BOOST_SCOPE_EXIT_END

			while (!usignal::get_pending()) {
				int status;
				if (fixed)
					status = (fread(buf, record_size, 1, fp) == 1);
				else
					status = (fgets(buf, record_size - 1, fp) != NULL);

				if (!status) {
					if (feof(fp))
						break;

					if (log_error)
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't read record from file {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
					return retval;
				}

				int extra_size;
				if (fixed) {
					extra_size = DBCT->fixed2record(buf, row_data, te_meta);
				} else {
					buf_str = buf;
					if (!buf_str.empty() && *buf_str.rbegin() == '\n')
						buf_str.resize(buf_str.length() - 1);
					extra_size = DBCT->variable2record(buf_str, row_data, te_meta);
				}

				(*dbc_switch->get_partition)(key, row_data);
				int partition_id = dbc_api::get_partition_id(te_meta.partition_type, te_meta.partitions, key);

				dbc_partition_t& partition = partitions[partition_id];
				if (!partition.insert(row_data, te_meta.internal_size + extra_size))
					return retval;
			}

			if (usignal::get_pending()) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: process terminated by signal")).str(SGLOCALE));
				return retval;
			}
		}
	} else {
		// Slave节点上加载，不允许的操作
		if (log_error) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error.")).str(SGLOCALE));
			SGT->_SGT_error_code = SGEINVAL;
		}
		return retval;
	}

	te->version++;

	retval = true;
	return retval;
}

bool dbc_server::load_from_bfile(bool log_error)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("log_error={1}") % log_error).str(SGLOCALE), &retval);
#endif

	if (DBCP->_DBCP_proc_type & DBC_STANDALONE) {
		retval = load_from_bfile(log_error, 0, -1, "");
		return retval;
	}

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	sdc_config& config_mgr = sdc_config::instance(DBCT);
	set<sdc_config_t>& config_set = config_mgr.get_config_set();
	sgc_ctx *SGC = SGT->SGC();
	sdc_config_t item;

	item.mid = SGC->_SGC_proc.mid;
	item.table_id = te->table_id;
	item.partition_id = 0;

	set<sdc_config_t>::iterator iter = config_set.lower_bound(item);
	for (; iter != config_set.end(); ++iter) {
		if (iter->mid != item.mid || iter->table_id != item.table_id)
			break;

		for (vector<pair<int, string> >::const_iterator piter = DBCP->_DBCP_mlist.begin(); piter != DBCP->_DBCP_mlist.end(); ++piter) {
			if (!load_from_bfile(log_error, iter->partition_id, piter->first, piter->second))
				return retval;
		}
	}

	te->version++;

	retval = true;
	return retval;
}

bool dbc_server::load_from_bfile(bool log_error, int partition_id, int mid, const string& pmid)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	dbc_data_t& te_meta = te->te_meta;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("log_error={1}, partition_id={2}, mid={3}, pmid={4}") % log_error % partition_id % mid % pmid).str(SGLOCALE), &retval);
#endif

	if (te->te_meta.refresh_type & REFRESH_TYPE_DB) {
		sgc_ctx *SGC = SGT->SGC();

		if (SGC->_SGC_proc.mid != mid) {
			retval = true;
			return retval;
		}
	}

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
	string data_dir = string(bbparms.data_dir) + ue->user_name + "/";

	string db_name = (boost::format("%1%%2%-%3%.b") % data_dir % te_meta.table_name % partition_id).str();
	if (mid != -1) {
		sgc_ctx *SGC = SGT->SGC();

		db_name += ".";
		db_name += SGC->mid2lmid(mid);
	}

	if (sqlite3_open_v2(db_name.c_str(), &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
		DBCT->_DBCT_error_code = DBCESYSTEM;
		if (log_error)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open database {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	BOOST_SCOPE_EXIT((&db)) {
		sqlite3_close(db);
	} BOOST_SCOPE_EXIT_END

	sqlite3_stmt *stmt_tmp = NULL;
	string sql_str = string("select data from t_fields");
	if (sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt_tmp, NULL) != SQLITE_OK) {
		DBCT->_DBCT_error_code = DBCESYSTEM;
		if (log_error)
			GPP->write_log(__FILE__, __LINE__,(_("ERROR: Can't prepare select sql")).str(SGLOCALE));
		return retval;
	}

	BOOST_SCOPE_EXIT((&stmt_tmp)) {
		sqlite3_finalize(stmt_tmp);
	} BOOST_SCOPE_EXIT_END

RETRY:
	switch (sqlite3_step(stmt_tmp)) {
	case SQLITE_ROW:
		{
			const char * row_data = reinterpret_cast<const char *>(sqlite3_column_text(stmt_tmp, 0));

			string dbc_fields_str = get_fields_str(te_meta);
			if (dbc_fields_str != row_data) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: table {1} structure is changed, exit") % te_meta.table_name ).str(SGLOCALE));
				exit(retval);
			}
		}
		break;
	case SQLITE_BUSY:
		boost::this_thread::yield();
		goto RETRY;
	case SQLITE_DONE:
		break;
	default:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't get data from table, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		exit(retval);
	}

	string sql = string("select data from ") + te_meta.table_name;
	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
		DBCT->_DBCT_error_code = DBCESYSTEM;
		if (log_error)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't prepare select sql")).str(SGLOCALE));
		return retval;
	}

	BOOST_SCOPE_EXIT((&stmt)) {
		sqlite3_finalize(stmt);
	} BOOST_SCOPE_EXIT_END

	dbc_api& api_mgr = dbc_api::instance(DBCT);
	boost::shared_array<char> auto_input_row(new char[te_meta.struct_size]);
	boost::shared_array<char> auto_output_row(new char[te_meta.struct_size]);
	char *input_row = auto_input_row.get();
	char *output_row = auto_output_row.get();
	int primary_key = te_mgr.primary_key();

	while (1) {
		switch (sqlite3_step(stmt)) {
		case SQLITE_ROW:
			{
				const char *row_data = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));

				if (te_meta.refresh_type & REFRESH_TYPE_AGG) {
					te_mgr.unmarshal(input_row, row_data);

					if (api_mgr.find(te->table_id, primary_key, input_row, output_row, 1) > 0) {
						agg_struct(input_row, output_row, te_meta);
						if (!api_mgr.update_by_rowid(te->table_id, api_mgr.get_rowid(0), input_row)) {
							if (log_error)
								GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't update row for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
							return retval;
						}
					} else {
						if (!api_mgr.insert(te->table_id, input_row)) {
							if (log_error)
								GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert row for table {1} - {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
							return retval;
						}
					}
				} else if (te_meta.refresh_type & REFRESH_TYPE_DISCARD) {
					te_mgr.unmarshal(input_row, row_data);

					if (!api_mgr.insert(te->table_id, input_row))
						continue;
				} else {
					// 创建记录区数据
					int bytes = sqlite3_column_bytes(stmt, 0);
					int extra_size = bytes - te_meta.internal_size;
					long rowid = te_mgr.create_row(extra_size);
					if (rowid == -1) {
						if (log_error)
							GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create row for table {1}, {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
						return retval;
					}

					char *new_data = DBCT->rowid2data(rowid);
					memcpy(new_data, row_data, bytes);

					te_mgr.unlock_row(rowid);
				}

				if (usignal::get_pending()) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: process terminated by signal")).str(SGLOCALE));
					return retval;
				}
			}
			break;
		case SQLITE_DONE:
			te->version++;

			retval = true;
			return retval;
		default:
			if (log_error)
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't get data from table, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}
	}
}

bool dbc_server::refresh_from_db()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	dbc_data_t& te_meta = te->te_meta;
	dbc_api& api_mgr = dbc_api::instance(DBCT);
	string sql;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	auto_ptr<Generic_Database> auto_db(connect(te->user_id & UE_ID_MASK, te_meta));
	Generic_Database *db = auto_db.get();

	// Create statement for select
	auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(db->create_data(true));
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = USBRK;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failed")).str(SGLOCALE));
		return retval;
	}

	if (!get_sql(te_meta, sql, true))
		return retval;

	data->setSQL(sql);
	stmt->bind(data);

	// Create statement for delete
	auto_ptr<Generic_Statement> auto_del_stmt(db->create_statement());
	Generic_Statement *del_stmt = auto_del_stmt.get();

	auto_ptr<struct_dynamic> auto_del_data(db->create_data());
	struct_dynamic *del_data = auto_del_data.get();
	if (del_data == NULL) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = USBRK;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failed")).str(SGLOCALE));
		return retval;
	}

	string q_table_name;
	if (strncasecmp(te_meta.table_name, "v_", 2) == 0)
		q_table_name = string("q") + (te_meta.table_name + 1);
	else
		q_table_name = string("q_") + te_meta.table_name;

	sql = string("delete from ") + q_table_name + " where rowid = :rowid{char[18]}";
	del_data->setSQL(sql);
	del_stmt->bind(del_data);

	auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
	Generic_ResultSet *rset = auto_rset.get();

	auto_ptr<char> auto_row_data(new char[te_meta.internal_size + te_meta.extra_size]);
	char *row_data = auto_row_data.get();
	int delete_count = 0;
	int primary_index = te_mgr.primary_key();
	int i;

	boost::shared_array<dbc_partition_t> auto_partitions;
	dbc_partition_t *partitions = NULL;

	if (te_meta.refresh_type & REFRESH_TYPE_BFILE) {
		auto_partitions.reset(new dbc_partition_t[te_meta.partitions]);
		partitions = auto_partitions.get();

		dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
		string data_dir = string(bbparms.data_dir) + ue->user_name + "/";

		for (i = 0; i < te_meta.partitions; i++) {
			if (!partitions[i].open(data_dir, te_meta.table_name, i, true))
				return retval;
			if (!partitions[i].insert_field(get_fields_str(te_meta)))
				return retval;
		}
	}

	// 异常情况下，清除创建的文件
	BOOST_SCOPE_EXIT((&retval)(&partitions)(&te_meta)) {
		if (!retval && (te_meta.refresh_type & REFRESH_TYPE_BFILE)) {
			for (int i = 0; i < te_meta.partitions; i++)
				partitions[i].remove();
		}
	} BOOST_SCOPE_EXIT_END

	dbc_switch_t *dbc_switch = DBCT->_DBCT_dbc_switch;
	char key[1024];

	DBCT->_DBCT_skip_marshal = true;
	BOOST_SCOPE_EXIT((&DBCT)) {
		DBCT->_DBCT_skip_marshal = false;
	} BOOST_SCOPE_EXIT_END

	BOOST_SCOPE_EXIT((&retval)(&api_mgr)(&db)) {
		if (!retval) {
			api_mgr.rollback();
			db->rollback();
		}
	} BOOST_SCOPE_EXIT_END

	while (rset->next()) {
		int extra_size = get_record(te_meta, row_data, rset);

		long affected_rows = 0;
		int q_type = rset->getInt(te_meta.field_size + 1);
		switch (q_type) {
		case Q_TYPE_INSERT:
			affected_rows = api_mgr.insert(te->table_id, row_data);
			break;
		case Q_TYPE_UPDATE:
			affected_rows = api_mgr.update(te->table_id, primary_index, row_data, row_data);
			break;
		case Q_TYPE_DELETE:
			affected_rows = api_mgr.erase(te->table_id, primary_index, row_data);
			break;
		default:
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: unknown type {1}") % q_type).str(SGLOCALE));
			return retval;
		}

		if (affected_rows <= 0) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: execute SQL for type {1} failed") % q_type).str(SGLOCALE));
			return retval;
		}

		del_stmt->setString(1, rset->getString(te_meta.field_size + 2));
		if (++delete_count == del_data->size()) {
			del_stmt->executeQuery();
			delete_count = 0;
		} else {
			del_stmt->addIteration();
		}

		if (te_meta.refresh_type & REFRESH_TYPE_BFILE) {
			(*dbc_switch->get_partition)(key, row_data);
			int partition_id = dbc_api::get_partition_id(te_meta.partition_type, te_meta.partitions, key);

			dbc_partition_t& partition = partitions[partition_id];
			if (!partition.insert(q_type, row_data, te_meta.internal_size + extra_size)) {
				DBCT->_DBCT_error_code = DBCESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert data to database")).str(SGLOCALE));
				return retval;
			}
		}
	}

	if (delete_count > 0)
		del_stmt->executeQuery();

	if (!api_mgr.commit())
		return retval;

	try {
		db->commit();
	} catch (exception& ex) {
		DBCT->_DBCT_error_code = DBCERMERR;
		GPP->write_log(__FILE__, __LINE__, ex.what());
		return retval;
	}

	te->version++;

	retval = true;
	return retval;
}

bool dbc_server::refresh_from_bfile()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (DBCP->_DBCP_proc_type & DBC_STANDALONE) {
		retval = refresh_from_bfile(0, -1);
		return retval;
	}

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	sdc_config& config_mgr = sdc_config::instance(DBCT);
	set<sdc_config_t>& config_set = config_mgr.get_config_set();
	sgc_ctx *SGC = SGT->SGC();
	sdc_config_t item;

	item.mid = SGC->_SGC_proc.mid;
	item.table_id = te->table_id;
	item.partition_id = 0;

	set<sdc_config_t>::iterator iter = config_set.lower_bound(item);
	for (; iter != config_set.end(); ++iter) {
		if (iter->mid != item.mid || iter->table_id != item.table_id)
			break;

		for (vector<pair<int, string> >::const_iterator piter = DBCP->_DBCP_mlist.begin(); piter != DBCP->_DBCP_mlist.end(); ++piter) {
			if (!refresh_from_bfile(iter->partition_id, piter->first))
				return retval;
		}
	}

	te->version++;

	retval = true;
	return retval;
}

bool dbc_server::refresh_from_bfile(int partition_id, int mid)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	dbc_data_t& te_meta = te->te_meta;
	dbc_api& api_mgr = dbc_api::instance(DBCT);
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("partition_id={1}, mid={2}") % partition_id % mid).str(SGLOCALE), &retval);
#endif

	if (te->te_meta.refresh_type & REFRESH_TYPE_DB) {
		sgc_ctx *SGC = SGT->SGC();

		if (SGC->_SGC_proc.mid != mid) {
			retval = true;
			return retval;
		}
	}

	int primary_index = te_mgr.primary_key();
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
	string data_dir = string(bbparms.data_dir) + ue->user_name + "/";

	string db_name = (boost::format("%1%%2%-%3%.q") % data_dir % te_meta.table_name % partition_id).str();
	if (mid != -1) {
		sgc_ctx *SGC = SGT->SGC();

		db_name += ".";
		db_name += SGC->mid2lmid(mid);
	}

	if (sqlite3_open_v2(db_name.c_str(), &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
		DBCT->_DBCT_error_code = DBCESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open database {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	BOOST_SCOPE_EXIT((&db)) {
		sqlite3_close(db);
	} BOOST_SCOPE_EXIT_END

	string sql = string("select type, data from ") + te_meta.table_name;
	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
		DBCT->_DBCT_error_code = DBCESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't prepare select sql, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	BOOST_SCOPE_EXIT((&stmt)) {
		sqlite3_finalize(stmt);
	} BOOST_SCOPE_EXIT_END

	BOOST_SCOPE_EXIT((&retval)(&api_mgr)) {
		if (!retval)
			api_mgr.rollback();
	} BOOST_SCOPE_EXIT_END

	while (1) {
		switch (sqlite3_step(stmt)) {
		case SQLITE_ROW:
			{
				int q_type = sqlite3_column_int(stmt, 0);
				const char *row_data = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
				long affected_rows = 0;

				switch (q_type) {
				case Q_TYPE_INSERT:
					affected_rows = api_mgr.insert(te->table_id, row_data);
					break;
				case Q_TYPE_UPDATE:
					affected_rows = api_mgr.update(te->table_id, primary_index, row_data, row_data);
					break;
				case Q_TYPE_DELETE:
					affected_rows = api_mgr.erase(te->table_id, primary_index, row_data);
					break;
				default:
					DBCT->_DBCT_error_code = DBCESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: unknown type {1}") % q_type).str(SGLOCALE));
					return retval;
				}

				if (affected_rows <= 0) {
					DBCT->_DBCT_error_code = DBCESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: execute SQL for type {1} failed.") % q_type).str(SGLOCALE));
					return retval;
				}

				if (usignal::get_pending()) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: process terminated by signal")).str(SGLOCALE));
					return retval;
				}
			}
			break;
		case SQLITE_DONE:
			retval = true;
			return retval;
		default:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't get data from table, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}
	}

	if (!api_mgr.commit())
		return retval;

	te->version++;

	retval = true;
	return retval;
}

bool dbc_server::get_sql(const dbc_data_t& te_meta, string& sql, bool refresh)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("table_name={1}, refresh={2}") % te_meta.table_name % refresh).str(SGLOCALE), &retval);
#endif

	sql = "select ";
	for (int i = 0; i < te_meta.field_size; i++) {
		if (i > 0)
			sql += ", ";

		const dbc_data_field_t& field = te_meta.fields[i];
		sql += field.fetch_name;
		switch (field.field_type) {
		case SQLTYPE_CHAR:
			sql += "{char}";
			break;
		case SQLTYPE_UCHAR:
			sql += "{uchar}";
			break;
		case SQLTYPE_SHORT:
			sql += "{short}";
			break;
		case SQLTYPE_USHORT:
			sql += "{ushort}";
			break;
		case SQLTYPE_INT:
			sql += "{int}";
			break;
		case SQLTYPE_UINT:
			sql += "{uint}";
			break;
		case SQLTYPE_LONG:
			sql += "{long}";
			break;
		case SQLTYPE_ULONG:
			sql += "{ulong}";
			break;
		case SQLTYPE_FLOAT:
			sql += "{float}";
			break;
		case SQLTYPE_DOUBLE:
			sql += "{double}";
			break;
		case SQLTYPE_STRING:
			sql += "{char[";
			sql += boost::lexical_cast<string>(te_meta.fields[i].field_size - 1);
			sql += "]}";
			break;
		case SQLTYPE_VSTRING:
			sql += "{vchar[";
			sql += boost::lexical_cast<string>(te_meta.fields[i].field_size - 1);
			sql += "]}";
			break;
		case SQLTYPE_DATE:
			sql += "{date}";
			break;
		case SQLTYPE_TIME:
			sql += "{time}";
			break;
		case SQLTYPE_DATETIME:
			sql += "{datetime}";
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			DBCT->_DBCT_error_code = DBCEINVAL;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Unknown field_type {1}") % te_meta.fields[i].field_type).str(SGLOCALE));
			return retval;
		}
	}

	if (refresh)
		sql += ", q_type{int}, rowid{char[18]}";

	sql += " from ";

	if (refresh) {
		string q_table_name;
		if (strncasecmp(te_meta.dbtable_name, "v_", 2) == 0)
			q_table_name = string("q") + (te_meta.dbtable_name + 1);
		else
			q_table_name = string("q_") + te_meta.dbtable_name;

		sql += q_table_name;
	} else {
		sql += te_meta.dbtable_name;
	}

	if (!refresh && te_meta.conditions[0] != '\0') {
		sql += " where ";
		sql += te_meta.conditions;
	}

	retval = true;
	return retval;
}

void dbc_server::get_struct(const dbc_data_t& te_meta, char *row_data, Generic_ResultSet *rset)
{
	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];
		int col_index = i + 1;
		char *field_addr = row_data + field.field_offset;

		switch (field.field_type) {
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR:
			if (rset->isNull(col_index))
				*field_addr = '\0';
			else
				*field_addr = rset->getChar(col_index);
			break;
		case SQLTYPE_SHORT:
		case SQLTYPE_USHORT:
			if (rset->isNull(col_index))
				*reinterpret_cast<short *>(field_addr) = 0;
			else
				*reinterpret_cast<short *>(field_addr) = rset->getShort(col_index);
			break;
		case SQLTYPE_INT:
		case SQLTYPE_UINT:
			if (rset->isNull(col_index))
				*reinterpret_cast<int *>(field_addr) = 0;
			else
				*reinterpret_cast<int *>(field_addr) = rset->getInt(col_index);
			break;
		case SQLTYPE_LONG:
		case SQLTYPE_ULONG:
			if (rset->isNull(col_index))
				*reinterpret_cast<long *>(field_addr) = 0;
			else
				*reinterpret_cast<long *>(field_addr) = rset->getLong(col_index);
			break;
		case SQLTYPE_FLOAT:
			if (rset->isNull(col_index))
				*reinterpret_cast<float *>(field_addr) = 0.0;
			else
				*reinterpret_cast<float *>(field_addr) = rset->getFloat(col_index);
			break;
		case SQLTYPE_DOUBLE:
			if (rset->isNull(col_index))
				*reinterpret_cast<double *>(field_addr) = 0.0;
			else
				*reinterpret_cast<double *>(field_addr) = rset->getDouble(col_index);
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			if (rset->isNull(col_index)) {
				field_addr[0] = '\0';
			} else {
				string str = rset->getString(col_index);
				memcpy(field_addr, str.c_str(), str.length());
				field_addr[str.length()] = '\0';
			}
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			if (rset->isNull(col_index))
				*reinterpret_cast<time_t *>(field_addr) = 0;
			else
				*reinterpret_cast<time_t *>(field_addr) = static_cast<time_t>(rset->getLong(col_index));
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Unknown field_type {1}") % field.field_type).str(SGLOCALE));
		}
	}
}

int dbc_server::get_record(const dbc_data_t& te_meta, char *row_data, Generic_ResultSet *rset)
{
	char *extra_buf = row_data + te_meta.internal_size;
	int extra_size = 0;
	int len;
	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];
		int col_index = i + 1;
		char *field_addr = row_data + field.internal_offset;

		switch (field.field_type) {
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR:
			if (rset->isNull(col_index))
				*field_addr = '\0';
			else
				*field_addr = rset->getChar(col_index);
			break;
		case SQLTYPE_SHORT:
		case SQLTYPE_USHORT:
			if (rset->isNull(col_index))
				*reinterpret_cast<short *>(field_addr) = 0;
			else
				*reinterpret_cast<short *>(field_addr) = rset->getShort(col_index);
			break;
		case SQLTYPE_INT:
		case SQLTYPE_UINT:
			if (rset->isNull(col_index))
				*reinterpret_cast<int *>(field_addr) = 0;
			else
				*reinterpret_cast<int *>(field_addr) = rset->getInt(col_index);
			break;
		case SQLTYPE_LONG:
		case SQLTYPE_ULONG:
			if (rset->isNull(col_index))
				*reinterpret_cast<long *>(field_addr) = 0;
			else
				*reinterpret_cast<long *>(field_addr) = rset->getLong(col_index);
			break;
		case SQLTYPE_FLOAT:
			if (rset->isNull(col_index))
				*reinterpret_cast<float *>(field_addr) = 0.0;
			else
				*reinterpret_cast<float *>(field_addr) = rset->getFloat(col_index);
			break;
		case SQLTYPE_DOUBLE:
			if (rset->isNull(col_index))
				*reinterpret_cast<double *>(field_addr) = 0.0;
			else
				*reinterpret_cast<double *>(field_addr) = rset->getDouble(col_index);
			break;
		case SQLTYPE_STRING:
			if (rset->isNull(col_index)) {
				field_addr[0] = '\0';
			} else {
				string str = rset->getString(col_index);
				memcpy(field_addr, str.c_str(), str.length());
				field_addr[str.length()] = '\0';
			}
			break;
		case SQLTYPE_VSTRING:
			*reinterpret_cast<int *>(field_addr) = extra_size;
			if (rset->isNull(col_index)) {
				extra_buf[extra_size] = '\0';
				extra_size++;
			} else {
				string str = rset->getString(col_index);
				len = str.length() + 1;
				memcpy(extra_buf + extra_size, str.c_str(), len);
				extra_size += len;
			}
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			if (rset->isNull(col_index))
				*reinterpret_cast<time_t *>(field_addr) = 0;
			else
				*reinterpret_cast<time_t *>(field_addr) = static_cast<time_t>(rset->getLong(col_index));
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Unknown field_type {1}") % field.field_type).str(SGLOCALE));
		}
	}

	return extra_size;
}

void dbc_server::agg_struct(char *dst_row, const char *src_row, const dbc_data_t& te_meta)
{
	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];

		if (field.is_primary)
			continue;

		char *dst_field = dst_row + field.field_offset;
		const char *src_field = src_row + field.field_offset;

		switch (field.field_type) {
		case SQLTYPE_CHAR:
			agg_number(*dst_field, *src_field, field.agg_type, field.field_name);
			break;
		case SQLTYPE_UCHAR:
			agg_number(*reinterpret_cast<unsigned char *>(dst_field), *reinterpret_cast<const unsigned char *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_SHORT:
			agg_number(*reinterpret_cast<short *>(dst_field), *reinterpret_cast<const short *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_USHORT:
			agg_number(*reinterpret_cast<unsigned short *>(dst_field), *reinterpret_cast<const unsigned short *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_INT:
			agg_number(*reinterpret_cast<int *>(dst_field), *reinterpret_cast<const int *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_UINT:
			agg_number(*reinterpret_cast<unsigned int *>(dst_field), *reinterpret_cast<const unsigned int *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_LONG:
			agg_number(*reinterpret_cast<long *>(dst_field), *reinterpret_cast<const long *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_ULONG:
			agg_number(*reinterpret_cast<unsigned long *>(dst_field), *reinterpret_cast<const unsigned long *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_FLOAT:
			agg_number(*reinterpret_cast<float *>(dst_field), *reinterpret_cast<const float *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_DOUBLE:
			agg_number(*reinterpret_cast<double *>(dst_field), *reinterpret_cast<const double *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			agg_string(dst_field, src_field, field.agg_type, field.field_name);
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			agg_number(*reinterpret_cast<time_t *>(dst_field), *reinterpret_cast<const time_t *>(src_field), field.agg_type, field.field_name);
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Unknown field_type {1}") % field.field_type).str(SGLOCALE));
		}
	}
}

Generic_Database * dbc_server::connect(int user_idx, const dbc_data_t& te_meta)
{
	dbc_ue_t *ue = DBCT->_DBCT_ues + user_idx;
	map<string, string> conn_info;

	string openinfo;
	string dbname;

	if (te_meta.openinfo[0] != '\0')
		openinfo = te_meta.openinfo;
	else
		openinfo = ue->openinfo;

	if (te_meta.dbname[0] != '\0')
		dbname = te_meta.dbname;
	else
		dbname = ue->dbname;

	database_factory& factory_mgr = database_factory::instance();
	factory_mgr.parse(openinfo, conn_info);

	Generic_Database *db = factory_mgr.create(dbname);
	db->connect(conn_info);
	return db;
}

void dbc_server::te_clean()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", NULL);
#endif

	for (dbc_te_t *te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
		if (te->in_flags(TE_MARK_REMOVED) && te->row_used == -1 && te->row_ref == -1) {
			// cleanup old table
			for (int i = 0; i <= te->max_index; i++) {
				dbc_ie_t *ie = te_mgr.get_index(i);
				if (ie == NULL)
					continue;

				ie_mgr.set_ctx(ie);
				ie_mgr.free_mem();
				te->index_hash[i] = -1;
			}

			te_mgr.set_ctx(te);
			te_mgr.free_mem();
			te->flags = 0;
		}
	}
}

void dbc_server::rte_clean()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_api& api_mgr = dbc_api::instance(DBCT);
	int insane = 0;
	bool can_revoke = true;
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", NULL);
#endif

	for (dbc_rte_t *rte = DBCT->_DBCT_rtes; rte < DBCT->_DBCT_rtes + bbparms.maxrtes; rte++) {
		if (!rte->in_flags(RTE_IN_USE) || rte->in_flags(RTE_MARK_REMOVED))
			continue;

		if (kill(rte->pid, 0) == -1) { // 进程异常终止
			rte_mgr.set_ctx(rte);
			GPP->write_log((_("WARN: Process {1} on slot {2} died; removing from DBC") % rte->pid % rte->rte_id).str(SGLOCALE));

			if (rte->in_flags(RTE_IN_TRANS)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: process {1} died in transaction, try to recover from crash.") % rte->pid).str(SGLOCALE));
				api_mgr.rollback();
				insane++;
			}

			// 进程可能会带锁终止，因此在更改RTE状态时，不要对其加锁
			// 恢复到初始状态，以保证锁等资源的正确释放
			rte_mgr.init();
			rte_mgr.set_ctx(DBCT->_DBCT_rtes + RTE_SLOT_SERVER);
			bbmeters.currtes--;
		} else {
			// 还有节点可能正在引用，则不能做回收
			if (DBCT->_DBCT_mark_ref_time >= rte->rtimestamp)
				can_revoke = false;
		}
	}

	if (can_revoke || insane) {
		dbc_te& te_mgr = dbc_te::instance(DBCT);

		for (dbc_te_t *te = DBCT->_DBCT_tes; te < DBCT->_DBCT_tes + bbparms.maxtes; te++) {
			if (!te->in_flags(TE_IN_USE))
				continue;

			te_mgr.set_ctx(te);
			if (can_revoke)
				te_mgr.revoke_ref();
			if (insane) // 标记引用数据可回收
				te_mgr.mark_ref();
		}

		if (insane) {
			DBCT->_DBCT_mark_ref_time = time(0);
			check_index_lock();
		}
	}
}

void dbc_server::chk_tran_timeout()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_api& api_mgr = dbc_api::instance(DBCT);
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", NULL);
#endif

	for (dbc_rte_t *rte = DBCT->_DBCT_rtes; rte < DBCT->_DBCT_rtes + bbparms.maxrtes; rte++) {
		if (!rte->in_flags(RTE_IN_USE) || !rte->in_flags(RTE_IN_TRANS))
			continue;

		/*
		 * If transaction is timeout, we'll wait a scan_unit to give client a chance to
		 * rollback transaction, but on the next scan, it will be terminated forcefully.
		 * That means if transaction is timeout, and no additional operation is done,
		 * the process will be terminated.
		 */
		if (rte->in_flags(RTE_ABORT_ONLY)) {
			if (kill(rte->pid, bbparms.post_signo) == -1) {
				api_mgr.rollback();
				rte_mgr.set_ctx(rte);
				rte_mgr.init();
				rte_mgr.set_ctx(DBCT->_DBCT_rtes + RTE_SLOT_SERVER);
				bbmeters.currtes--;
			}
		} else if (rte->timeout > 0 && rte->rtimestamp + rte->timeout < time(0)) {
			rte->set_flags(RTE_ABORT_ONLY);
			kill(rte->pid, bbparms.post_signo);
		}
	}
}

void dbc_server::check_index_lock()
{
	dbc_api& api_mgr = dbc_api::instance(DBCT);
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", NULL);
#endif

	DBCT->_DBCT_where_func = dbc_api::match_myself;
	auto_ptr<dbc_cursor> cursor = api_mgr.find(TE_SYS_INDEX_LOCK, NULL);
	while (cursor->next())
		api_mgr.erase_by_rowid(TE_SYS_INDEX_LOCK, cursor->rowid());
	DBCT->_DBCT_where_func = NULL;
}

bool dbc_server::db_to_bfile(dbc_switch_t *dbc_switch, dbc_te_t *te)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}") % te->table_id).str(SGLOCALE), &retval);
#endif
	const dbc_data_t& te_meta = te->te_meta;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;

	boost::shared_array<dbc_partition_t> auto_partitions(new dbc_partition_t[te_meta.partitions]);
	dbc_partition_t *partitions = auto_partitions.get();

	dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
	string data_dir = string(bbparms.data_dir) + ue->user_name + "/";

	for (int i = 0; i < te_meta.partitions; i++) {
		if (!partitions[i].open(data_dir, te_meta.table_name, i, false))
			return retval;
		if (!partitions[i].insert_field(get_fields_str(te_meta)))
				return retval;
	}

	// 关闭文件，异常情况下，清除创建的文件
	BOOST_SCOPE_EXIT((&retval)(&partitions)(&te_meta)) {
		if (!retval) {
			for (int i = 0; i < te_meta.partitions; i++)
				partitions[i].remove();
		}
	} BOOST_SCOPE_EXIT_END

	auto_ptr<Generic_Database> auto_db(connect(te->user_id & UE_ID_MASK, te_meta));
	Generic_Database *db = auto_db.get();

	auto_ptr<Generic_Statement> auto_stmt(db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(db->create_data(true));
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		DBCT->_DBCT_error_code = DBCEOS;
		DBCT->_DBCT_native_code = USBRK;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Memory allocation failed")).str(SGLOCALE));
		return retval;
	}

	string sql;
	if (!get_sql(te_meta, sql, false))
		return retval;

	data->setSQL(sql);
	stmt->bind(data);

	auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
	Generic_ResultSet *rset = auto_rset.get();

	char key[1024];
	boost::shared_array<char> auto_row_data(new char[te_meta.internal_size + te_meta.extra_size]);
	char *row_data = auto_row_data.get();

	while (rset->next()) {
		int extra_size = get_record(te_meta, row_data, rset);

		(*dbc_switch->get_partition)(key, row_data);
		int partition_id = dbc_api::get_partition_id(te_meta.partition_type, te_meta.partitions, key);

		dbc_partition_t& partition = partitions[partition_id];
		if (!partition.insert(row_data, te_meta.internal_size + extra_size))
			return retval;

		if (usignal::get_pending()) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: process terminated by signal")).str(SGLOCALE));
			return retval;
		}
	}

	retval = true;
	return retval;
}

bool dbc_server::file_to_bfile(dbc_switch_t *dbc_switch, dbc_te_t *te)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}") % te->table_id).str(SGLOCALE), &retval);
#endif
	const dbc_data_t& te_meta = te->te_meta;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;

	int record_size = 1;
	for (int i = 0; i < te_meta.field_size; i++) {
		record_size += te_meta.fields[i].load_size;
		if (te_meta.delimiter != '\0') // 有分隔符
			record_size++;
	}
	boost::shared_array<char> auto_buf(new char[record_size]);
	char *buf = auto_buf.get();
	bool fixed = (te_meta.delimiter == '\0');
	string buf_str;

	boost::shared_array<char> auto_data(new char[te_meta.internal_size + te_meta.extra_size]);
	char *row_data = auto_data.get();

	boost::shared_array<dbc_partition_t> auto_partitions(new dbc_partition_t[te_meta.partitions]);
	dbc_partition_t *partitions = auto_partitions.get();

	dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
	string data_dir = string(bbparms.data_dir) + ue->user_name + "/";
	scan_file<> fscan(data_dir, string("^") + te_meta.table_name + "\\.seq\\.[0-9]+$", REG_ICASE);

	vector<string> files;
	fscan.get_files(files);

	for (int i = 0; i < te_meta.partitions; i++) {
		if (!partitions[i].open(data_dir, te_meta.table_name, i, false))
			return retval;
		if (!partitions[i].insert_field(get_fields_str(te_meta)))
				return retval;
	}

	// 关闭文件，异常情况下，清除创建的文件
	BOOST_SCOPE_EXIT((&retval)(&partitions)(&te_meta)) {
		if (!retval) {
			for (int i = 0; i < te_meta.partitions; i++)
				partitions[i].remove();
		}
	} BOOST_SCOPE_EXIT_END

	char key[1024];

	BOOST_FOREACH(const string& filename, files) {
		string full_name = data_dir + filename;

		FILE *fp = fopen(full_name.c_str(), "r");
		if (fp == NULL) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
			return retval;
		}

		BOOST_SCOPE_EXIT((&fp)) {
			fclose(fp);
		} BOOST_SCOPE_EXIT_END

		while (1) {
			int status;
			if (fixed)
				status = (fread(buf, record_size, 1, fp) == 1);
			else
				status = (fgets(buf, record_size - 1, fp) != NULL);

			if (!status) {
				if (feof(fp))
					break;

				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't read record from file {1} - {2}") % full_name % ::strerror(errno)).str(SGLOCALE));
				return retval;
			}

			int extra_size;
			if (fixed) {
				extra_size = DBCT->fixed2record(buf, row_data, te_meta);
			} else {
				buf_str = buf;
				if (!buf_str.empty() && *buf_str.rbegin() == '\n')
					buf_str.resize(buf_str.length() - 1);
				extra_size = DBCT->variable2record(buf_str, row_data, te_meta);
			}

			(*dbc_switch->get_partition)(key, row_data);
			int partition_id = dbc_api::get_partition_id(te_meta.partition_type, te_meta.partitions, key);

			dbc_partition_t& partition = partitions[partition_id];
			if (!partition.insert(row_data, te_meta.internal_size + extra_size))
				return retval;

			if (usignal::get_pending()) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: process terminated by signal")).str(SGLOCALE));
				return retval;
			}
		}
	}

	retval = true;
	return retval;
}

int dbc_server::get_vsize(const dbc_data_t& te_meta, Generic_ResultSet *rset)
{
	int vsize = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, (_("table_name={1}") % te_meta.table_name).str(SGLOCALE), &vsize);
#endif

	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];

		if (field.field_type == SQLTYPE_VSTRING) {
			if (rset->isNull(i + 1)) {
				vsize += 1;
			} else {
				string str = rset->getString(i + 1);
				vsize += str.length() + 1;
			}
		}
	}

	return vsize;
}

int dbc_server::get_vsize(const char *src_buf, const dbc_data_t& te_meta)
{
	int vsize = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, (_("table_name={1}") % te_meta.table_name).str(SGLOCALE), &vsize);
#endif

	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];

		if (field.field_type == SQLTYPE_VSTRING) {
			int size;
			for (size = field.load_size; size > 0; size--) {
				if (src_buf[size] != ' ')
					break;
			}

			vsize += size + 1;
		}

		src_buf += field.load_size;
	}

	return vsize;
}

int dbc_server::get_vsize(const string& src_buf, const dbc_data_t& te_meta)
{
	int vsize = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, (_("table_name={1}") % te_meta.table_name).str(SGLOCALE), &vsize);
#endif
	int i = 0;

	if (te_meta.escape == '\0') {
		char sep_str[2];
		sep_str[0] = te_meta.delimiter;
		sep_str[1] = '\0';
		boost::char_separator<char> sep(sep_str, "", boost::keep_empty_tokens);
		boost::tokenizer<boost::char_separator<char> > tok(src_buf, sep);

		for (boost::tokenizer<boost::char_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter, i++) {
			if (i == te_meta.field_size)
				break;

			const dbc_data_field_t& field = te_meta.fields[i];
			if (field.field_type == SQLTYPE_VSTRING) {
				string str = *iter;
				if (field.load_size <= str.length())
					vsize += field.load_size + 1;
				else
					vsize += str.length() + 1;
			}
		}
	} else {
		boost::escaped_list_separator<char> sep(te_meta.escape, te_meta.delimiter, '\"');
		boost::tokenizer<boost::escaped_list_separator<char> > tok(src_buf, sep);

		for (boost::tokenizer<boost::escaped_list_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter, i++) {
			if (i == te_meta.field_size)
				break;

			const dbc_data_field_t& field = te_meta.fields[i];
			if (field.field_type == SQLTYPE_VSTRING) {
				string str = *iter;
				if (field.load_size <= str.length())
					vsize += field.load_size + 1;
				else
					vsize += str.length() + 1;
			}
		}
	}

	return vsize;
}

void dbc_server::wait_ready()
{
#if defined(DEBUG)
	scoped_debug<int> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	sg_proc& proc_mgr = sg_proc::instance(SGT);
	sgc_ctx *SGC = SGT->SGC();
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	sys_func& func_mgr = sys_func::instance();

	for (vector<pair<int, string> >::const_iterator iter = DBCP->_DBCP_mlist.begin(); iter != DBCP->_DBCP_mlist.end(); ++iter) {
		const char *lmid = SGC->mid2lmid(iter->first);
		string ready_file = DBCT->get_ready_file(lmid);

		for (;; sleep(1)) {
			ifstream ifs(ready_file.c_str());
			if (!ifs)
				continue;

			sg_proc_t proc;
			ifs.read(reinterpret_cast<char *>(&proc.mid), sizeof(proc.mid));
			ifs.read(reinterpret_cast<char *>(&proc.pid), sizeof(proc.pid));
			if (!ifs)
				continue;

			if (!proc_mgr.nwkill(proc, 0, 60))
				continue;

			break;
		}

		// 解压文件，跳过系统用户
		dbc_ue_t *ue;
		for (ue = DBCT->_DBCT_ues + 1; ue <= DBCT->_DBCT_ues + bbparms.maxusers; ue++) {
			if (!ue->in_use())
				continue;

			// 忽略错误
			string cmd = string("gunzip -f ") + bbparms.data_dir + ue->user_name + "/*.b." + lmid + ".gz >/dev/null 2>&1";
			(void)func_mgr.new_task(cmd.c_str(), NULL, 0);
		}
	}
}

void dbc_server::s_to_bfile(dbc_switch_t *dbc_switch, dbc_te_t *te, int& retval)
{
	retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}") % te->table_id).str(SGLOCALE), &retval);
#endif
	boost::shared_ptr<dbc_server> auto_server(new dbc_server());
	dbc_server *server_mgr = auto_server.get();
	gpp_ctx *GPP = gpp_ctx::instance();
	dbct_ctx *DBCT = dbct_ctx::instance();

	if (!DBCT->attach_shm()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't attach to shared memory")).str(SGLOCALE));
		return;
	}

	DBCT->init_globals();

	BOOST_SCOPE_EXIT((&DBCT)) {
		DBCT->detach_shm();
	} BOOST_SCOPE_EXIT_END

	try {
		if (te->te_meta.refresh_type & REFRESH_TYPE_DB)
			retval = (server_mgr->db_to_bfile(dbc_switch, te) ? 0 : -1);
		else
			retval = (server_mgr->file_to_bfile(dbc_switch, te) ? 0 : -1);
	} catch (exception& ex) {
		GPP->write_log(__FILE__, __LINE__, ex.what());
		return;
	}
}

void dbc_server::s_load_table(dbc_te_t *te, int& retval)
{
	retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}") % te->table_id).str(SGLOCALE), &retval);
#endif
	boost::shared_ptr<dbc_server> auto_server(new dbc_server());
	dbc_server *server_mgr = auto_server.get();
	gpp_ctx *GPP = gpp_ctx::instance();
	dbct_ctx *DBCT = dbct_ctx::instance();

	if (!DBCT->attach_shm()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't attach to shared memory")).str(SGLOCALE));
		return;
	}

	DBCT->init_globals();

	BOOST_SCOPE_EXIT((&DBCT)) {
		DBCT->detach_shm();
	} BOOST_SCOPE_EXIT_END

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	if (!rte_mgr.get_slot()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't register to DBC")).str(SGLOCALE));
		return;
	}

	BOOST_SCOPE_EXIT((&rte_mgr)) {
		rte_mgr.put_slot();
	} BOOST_SCOPE_EXIT_END

	try {
		retval = (server_mgr->load_table(te) ? 0 : -1);
	} catch (exception& ex) {
		GPP->write_log(__FILE__, __LINE__, ex.what());
	}
}

void dbc_server::stdsigterm(int signo)
{
	sgp_ctx *SGP = sgp_ctx::instance();

	SGP->_SGP_shutdown++;
	SGP->_SGP_shutdown_due_to_sigterm = true;
}

}
}

