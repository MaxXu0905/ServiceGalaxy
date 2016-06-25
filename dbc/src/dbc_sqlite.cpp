#include "dbc_internal.h"

namespace ai
{
namespace sg
{

dbc_sqlite& dbc_sqlite::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<dbc_sqlite *>(DBCT->_DBCT_mgr_array[DBC_SQLITE_MANAGER]);
}

bool dbc_sqlite::save(const set<redo_prow_t>& row_set, int flags)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, (_("row_set.size()={1}, flags={2}") % row_set.size() % flags).str(SGLOCALE), &retval);
#endif

	dbc_sqlite *_this = this;
	int sqlite_id = get_sqlite();
	BOOST_SCOPE_EXIT((&_this)(&sqlite_id)) {
		_this->put_sqlite(sqlite_id);
	} BOOST_SCOPE_EXIT_END

	dbc_sqlite_t& sqlite = DBCP->_DBCP_sqlites[sqlite_id];

	if (sqlite.db == NULL) {
		dbc_bbparms_t& bbparms = bbp->bbparms;
		string db_name = (boost::format("%1%sqlite_dbc.%2%") % bbparms.sync_dir % sqlite_id).str();

		if (!open(db_name, sqlite.db)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open database {1}") % db_name).str(SGLOCALE));
			return retval;
		}
	}

	if (sqlite.insert_stmt == NULL) {
		if (!prepare(sqlite.db, sqlite.insert_stmt, DBC_SQLITE_INSERT)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't prepare statement for insert")).str(SGLOCALE));
			return retval;
		}
	}

	sqlite3 *db = sqlite.db;
	sqlite3_stmt *stmt = sqlite.insert_stmt;

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();
	int64_t& tran_id = rte->tran_id;

	if (sqlite3_exec(db, "begin", NULL, NULL, NULL) != SQLITE_OK) {
		DBCT->_DBCT_error_code = DBCESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't begin transaction, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	BOOST_SCOPE_EXIT((&retval)(&db)(&GPP)(&DBCT)) {
		if (!retval && sqlite3_exec(db, "rollback", NULL, NULL, NULL) != SQLITE_OK) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't rollback transaction, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		}
	} BOOST_SCOPE_EXIT_END

	for (set<redo_prow_t>::const_iterator iter = row_set.begin(); iter != row_set.end(); ++iter) {
		const redo_row_t *redo_row = iter->redo_row;
		row_link_t *link = DBCT->rowid2link(redo_row->rowid);

		switch (link->flags & ROW_TRAN_MASK) {
		case ROW_TRAN_INSERT:
			if (redo_row->old_rowid != -1) { // 实际上是更新
				row_link_t *old_link = DBCT->rowid2link(redo_row->old_rowid);
				if ((old_link->flags & (ROW_TRAN_INSERT | ROW_TRAN_DELETE)) == (ROW_TRAN_INSERT | ROW_TRAN_DELETE)) {
					if (!insert(db, stmt, tran_id, redo_row->sequence, UPDATE_TYPE_INSERT, redo_row->user_idx, redo_row->table_id,
						redo_row->row_size, 0, DBCT->rowid2data(redo_row->rowid), NULL)) {
						DBCT->_DBCT_error_code = DBCESYSTEM;
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert data to sqlite")).str(SGLOCALE));
						return retval;
					}
				} else {
					if (!insert(db, stmt, tran_id, redo_row->sequence, UPDATE_TYPE_UPDATE, redo_row->user_idx, redo_row->table_id,
						redo_row->row_size, redo_row->old_row_size, DBCT->rowid2data(redo_row->rowid),
						DBCT->rowid2data(redo_row->old_rowid))) {
						DBCT->_DBCT_error_code = DBCESYSTEM;
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert data to sqlite")).str(SGLOCALE));
						return retval;
					}
				}
			} else {
				if (!insert(db, stmt, tran_id, redo_row->sequence, UPDATE_TYPE_INSERT, redo_row->user_idx, redo_row->table_id,
					redo_row->row_size, 0, DBCT->rowid2data(redo_row->rowid), NULL)) {
					DBCT->_DBCT_error_code = DBCESYSTEM;
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert data to sqlite")).str(SGLOCALE));
					return retval;
				}
			}
			break;
		case ROW_TRAN_DELETE:
			if (!insert(db, stmt, tran_id, redo_row->sequence, UPDATE_TYPE_DELETE, redo_row->user_idx, redo_row->table_id,
				redo_row->row_size, 0, DBCT->rowid2data(redo_row->rowid), NULL)) {
				DBCT->_DBCT_error_code = DBCESYSTEM;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert data to sqlite")).str(SGLOCALE));
				return retval;
			}
			break;
		case (ROW_TRAN_DELETE | ROW_TRAN_UPDATE):
			// 该记录被更新，不需要保存到文件中
			break;
		case (ROW_TRAN_INSERT | ROW_TRAN_DELETE | ROW_TRAN_UPDATE):
			// 该记录插入后被更新，不需要保存到文件中
			break;
		case (ROW_TRAN_INSERT | ROW_TRAN_DELETE):
			// 该记录插入后又删除了，不需要保存到文件中
			break;
		default:
			break;
		}
	}

	if (sqlite3_exec(db, "commit", NULL, NULL, NULL) != SQLITE_OK) {
		DBCT->_DBCT_error_code = DBCESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't commit transaction, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

bool dbc_sqlite::load()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, "", &retval);
#endif

	// 加载后关闭所有句柄，server进程不再需要
	dbc_sqlite *_this = this;
	DBCT->_DBCT_skip_marshal = true;
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t *saved_ue = ue_mgr.get_ue();
	BOOST_SCOPE_EXIT((&_this)(&DBCT)(&ue_mgr)(&saved_ue)) {
		ue_mgr.set_ctx(saved_ue);
		DBCT->_DBCT_skip_marshal = false;
		_this->cleanup();
	} BOOST_SCOPE_EXIT_END

	sqlite_load_map_t load_map;

	for (int sqlite_id = 0; sqlite_id < MAX_SQLITES; sqlite_id++) {
		dbc_sqlite_t& sqlite = DBCP->_DBCP_sqlites[sqlite_id];
		sqlite3_stmt *& stmt = sqlite.select_stmt;
		sqlite3 *& db = sqlite.db;

		if (db == NULL) {
			dbc_bbparms_t& bbparms = bbp->bbparms;
			string db_name = (boost::format("%1%sqlite_dbc.%2%") % bbparms.sync_dir % sqlite_id).str();

			if (!open(db_name, db)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open database {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
				return retval;
			}
		}

		if (stmt == NULL) {
			if (!prepare(db, stmt, DBC_SQLITE_LOAD)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't prepare statement for select")).str(SGLOCALE));
				return retval;
			}
		}

		if (sqlite3_reset(stmt) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't reset statement for select, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}

		if (!get_row(sqlite_id, load_map)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't get row from sqlite")).str(SGLOCALE));
			return retval;
		}
	}

	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_api& api_mgr = dbc_api::instance(DBCT);
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = NULL;
	int orig_table_id = -1;
	int index_id = -1;

	while (!load_map.empty()) {
		sqlite_load_map_t::iterator iter = load_map.begin();

		const int64_t& tran_id = iter->first;
		if (tran_id >= bbmeters.tran_id)
			bbmeters.tran_id = tran_id + 1;

		sqlite_load_t& item = iter->second;
		int sqlite_id = item.sqlite_id;

		ue_mgr.set_ctx(item.user_idx);

		if (orig_table_id != item.table_id) {
			orig_table_id = item.table_id;
			te = ue_mgr.get_table(item.table_id);
			te_mgr.set_ctx(te);
			index_id = te_mgr.primary_key();
		}

		switch (item.type) {
		case UPDATE_TYPE_INSERT:
			DBCT->_DBCT_insert_size = item.row_size;
			if (!api_mgr.insert(item.table_id, item.row_data)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: insert {1} failed, sqlite database may corrupt, {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				return retval;
			}
			break;
		case UPDATE_TYPE_UPDATE:
			DBCT->_DBCT_insert_size = item.row_size;
			if (api_mgr.update(item.table_id, index_id, item.old_row_data, item.row_data) != 1) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: update {1} failed, sqlite database may corrupt, {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				return retval;
			}
			break;
		case UPDATE_TYPE_DELETE:
			if (api_mgr.erase(item.table_id, index_id, item.row_data) != 1) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: delete {1} failed, sqlite database may corrupt, {2}") % te->te_meta.table_name % DBCT->strerror()).str(SGLOCALE));
				return retval;
			}
			break;
		default:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error.")).str(SGLOCALE));
			return retval;
		}

		load_map.erase(iter);

		if (!get_row(sqlite_id, load_map)) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't get row from sqlite")).str(SGLOCALE));
			return retval;
		}
	}

	retval = true;
	return retval;
}

bool dbc_sqlite::persist(int64_t tran_id)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, (_("tran_id={1}") % tran_id).str(SGLOCALE), &retval);
#endif

	retval = true;
	return retval;
}

bool dbc_sqlite::move2sync(sqlite3 *target_db, int64_t target_tran_id)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, (_("target_tran_id={1}") % target_tran_id).str(SGLOCALE), &retval);
#endif

	sqlite3_stmt *target_stmt = NULL;
	BOOST_SCOPE_EXIT((&target_stmt)) {
		sqlite3_finalize(target_stmt);
	} BOOST_SCOPE_EXIT_END

	// 准备目的数据库语句
	if (!prepare(target_db, target_stmt, DBC_SQLITE_INSERT)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't prepare statement for insert")).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_exec(target_db, "begin", NULL, NULL, NULL) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't begin transaction, {1}") % sqlite3_errmsg(target_db)).str(SGLOCALE));
		return retval;
	}

	string del_str = (boost::format("delete from dbc where tran_id <= %1%") % target_tran_id).str();

	for (int sqlite_id = 0; sqlite_id < MAX_SQLITES; sqlite_id++) {
		// 对源表加锁
		dbc_sqlite *_this = this;
		lock_sqlite(sqlite_id);
		BOOST_SCOPE_EXIT((&_this)(&sqlite_id)) {
			_this->put_sqlite(sqlite_id);
		} BOOST_SCOPE_EXIT_END

		// 打开源表
		dbc_sqlite_t& sqlite = DBCP->_DBCP_sqlites[sqlite_id];
		sqlite3 *& db = sqlite.db;
		sqlite3_stmt *& stmt = sqlite.select_stmt;

		if (db == NULL) {
			string db_name = (boost::format("%1%sqlite_dbc.%2%") % bbparms.sync_dir % sqlite_id).str();

			if (!open(db_name, db)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open database {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
				return retval;
			}
		}

		if (stmt == NULL) {
			if (!prepare(db, stmt, DBC_SQLITE_MOVE)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't prepare statement for select")).str(SGLOCALE));
				return retval;
			}
		}

		int64_t tran_id;
		int64_t sequence;
		int type;
		int user_idx;
		int table_id;
		int row_size;
		int old_row_size;
		const char *row_data;
		const char *old_row_data;

		// 重置源表
		if (sqlite3_reset(stmt) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't reset statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}

		if (sqlite3_bind_int64(stmt, 1, target_tran_id) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}

		// 把源表数据导入到目标表
		while (1) {
			switch (sqlite3_step(stmt)) {
			case SQLITE_ROW:
				tran_id = sqlite3_column_int64(stmt, 0);
				sequence = sqlite3_column_int64(stmt, 1);
				type = sqlite3_column_int(stmt, 2);
				user_idx = sqlite3_column_int(stmt, 3);
				table_id = sqlite3_column_int(stmt, 4);
				row_size = sqlite3_column_bytes(stmt, 5);
				old_row_size = sqlite3_column_bytes(stmt, 6);
				row_data = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
				old_row_data = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));

				if (!insert(target_db, target_stmt, tran_id, sequence, type, user_idx, table_id,
					row_size, old_row_size, row_data, old_row_data)) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't insert data to sqlite")).str(SGLOCALE));
					return retval;
				}
				break;
			case SQLITE_BUSY:
				boost::this_thread::yield();
				break;
			case SQLITE_DONE:
				retval = true;
				return retval;
			default:
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: unknown opcode, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
				return retval;
			}
		}

		// 提交目标表插入操作
		if (sqlite3_exec(target_db, "commit", NULL, NULL, NULL) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't commit transaction, {1}") % sqlite3_errmsg(target_db)).str(SGLOCALE));
			return retval;
		}

		// 删除源表记录
		if (sqlite3_exec(db, del_str.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't delete data from sqlite, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}

		// 提交源表的删除操作
		if (sqlite3_exec(db, "commit", NULL, NULL, NULL) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't commit transaction, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}
	}

	retval = true;
	return retval;
}

bool dbc_sqlite::insert(sqlite3 *db, sqlite3_stmt *stmt, int64_t tran_id, int64_t sequence, int type, int user_idx,
	int table_id, int row_size, int old_row_size, const char *row_data, const char *old_row_data)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, (_("tran_id={1}, sequence={2}, type={3}, user_idx={4}, table_id={5}, row_size={6}, old_row_size={7}") %tran_id % sequence % type % user_idx % table_id % row_size % old_row_size).str(SGLOCALE), &retval);
#endif

	if (sqlite3_reset(stmt) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't reset statement for insert, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_bind_int64(stmt, 1, tran_id) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_bind_int64(stmt, 2, sequence) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_bind_int(stmt, 3, type) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_bind_int(stmt, 4, user_idx) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_bind_int(stmt, 5, table_id) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_bind_text(stmt, 6, row_data, row_size, SQLITE_STATIC) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_bind_text(stmt, 7, old_row_data, old_row_size, SQLITE_STATIC) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	while (1) {
		switch (sqlite3_step(stmt)) {
		case SQLITE_DONE:
			retval = true;
			return retval;
		case SQLITE_BUSY:
			boost::this_thread::yield();
			continue;
		default:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: unknown opcode, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}
	}
}

bool dbc_sqlite::erase(sqlite3 *db, sqlite3_stmt *stmt, int user_idx, int64_t tran_id)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, (_("user_idx={1}, tran_id={2}") % user_idx % tran_id).str(SGLOCALE), &retval);
#endif

	if (sqlite3_reset(stmt) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't reset statement for delete, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_bind_int(stmt, 1, user_idx) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_bind_int64(stmt, 2, tran_id) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't bind statement, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	while (1) {
		switch (sqlite3_step(stmt)) {
		case SQLITE_DONE:
			retval = true;
			return retval;
		case SQLITE_BUSY:
			boost::this_thread::yield();
			continue;
		default:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: unknown opcode, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}
	}
}

bool dbc_sqlite::truncate(int table_id)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, (_("table_id={1}") % table_id).str(SGLOCALE), &retval);
#endif

	string sql_str = (boost::format("delete from dbc where table_id = %1%") % table_id).str();

	for (int sqlite_id = 0; sqlite_id < MAX_SQLITES; sqlite_id++) {
		// 对源表加锁
		dbc_sqlite *_this = this;
		lock_sqlite(sqlite_id);
		BOOST_SCOPE_EXIT((&_this)(&sqlite_id)) {
			_this->put_sqlite(sqlite_id);
		} BOOST_SCOPE_EXIT_END

		dbc_sqlite_t& sqlite = DBCP->_DBCP_sqlites[sqlite_id];
		sqlite3 *& db = sqlite.db;

		if (db == NULL) {
			string db_name = (boost::format("%1%sqlite_dbc.%2%") % bbparms.sync_dir % sqlite_id).str();

			if (!open(db_name, db)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open database {1}, {2}") % db_name % sqlite3_errmsg(db)).str(SGLOCALE));
				return retval;
			}
		}

		if (sqlite3_exec(db, sql_str.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't delete data from sqlite, {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}
	}

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	dbc_data_t& te_meta = te->te_meta;
	sqlite3 *base_db;

	sql_str = (_("truncate table {1}") % te_meta.table_name).str(SGLOCALE);

	for (int partition_id = 0; partition_id < te_meta.partitions; partition_id++) {
		string db_name = (boost::format("%1%%2%-%3%.b") % bbparms.data_dir % te_meta.table_name % partition_id).str();

		if (DBCP->_DBCP_proc_type != DBC_STANDALONE) {
			sgc_ctx *SGC = SGT->SGC();

			db_name += ".";
			db_name += boost::lexical_cast<string>(SGC->_SGC_proc.mid);
		}

		// 检查数据库是否存在，不存在则跳过
		if (access(db_name.c_str(), F_OK) == -1)
			continue;

		if (sqlite3_open_v2(db_name.c_str(), &base_db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open database {1}, {2}") % db_name % sqlite3_errmsg(base_db)).str(SGLOCALE));
			return retval;
		}

		BOOST_SCOPE_EXIT((&base_db)) {
			sqlite3_close(base_db);
		} BOOST_SCOPE_EXIT_END

		if (sqlite3_exec(base_db, sql_str.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't truncate table {1}, {2}") % te_meta.table_name % sqlite3_errmsg(base_db)).str(SGLOCALE));
			return retval;
		}
	}

	retval = true;
	return retval;
}

int dbc_sqlite::get_sqlite()
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	bi::scoped_lock<bi::interprocess_mutex> lock(bbmeters.sqlite_mutex);

	while (1) {
		for (int sqlite_id = 0; sqlite_id < MAX_SQLITES; sqlite_id++) {
			if (!(bbmeters.sqlite_states[sqlite_id] & SQLITE_IN_USE)) {
				bbmeters.sqlite_states[sqlite_id] |= SQLITE_IN_USE;
				return sqlite_id;
			}
		}

		bbmeters.sqlite_cond.wait(lock);
	}
}

void dbc_sqlite::lock_sqlite(int sqlite_id)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	bi::scoped_lock<bi::interprocess_mutex> lock(bbmeters.sqlite_mutex);

	while (1) {
		if (!(bbmeters.sqlite_states[sqlite_id] & SQLITE_IN_USE)) {
			bbmeters.sqlite_states[sqlite_id] |= SQLITE_IN_USE;
			return;
		}

		bbmeters.sqlite_cond.wait(lock);
	}
}

void dbc_sqlite::put_sqlite(int sqlite_id)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	bi::scoped_lock<bi::interprocess_mutex> lock(bbmeters.sqlite_mutex);

	bbmeters.sqlite_states[sqlite_id] &= ~SQLITE_IN_USE;
	bbmeters.sqlite_cond.notify_one();
}

bool dbc_sqlite::open(const string& db_name, sqlite3 *& db)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, (_("db_name={1}") % db_name).str(SGLOCALE), &retval);
#endif

	// 连接数据库
	if (sqlite3_open_v2(db_name.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open database: {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	// 关闭同步方式
	if (sqlite3_exec(db, "PRAGMA synchronous=0", NULL, NULL, NULL) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: PRAGMA synchronous=0 failed: {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	// 设置PERSIST方式
	if (sqlite3_exec(db, "PRAGMA journal_mode=PERSIST", NULL, NULL, NULL) != SQLITE_OK) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: PRAGMA journal_mode=PERSIST failed: {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

void dbc_sqlite::close(sqlite3 *& db)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, "", NULL);
#endif

	sqlite3_close(db);
	db = NULL;
}

bool dbc_sqlite::prepare(sqlite3 *db, sqlite3_stmt *& stmt, int type)
{
	string sql_str;
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, "", &retval);
#endif

	switch (type) {
	case DBC_SQLITE_LOAD:
		sql_str = "select tran_id, type, user_idx, table_id, row_data, old_row_data from dbc order by tran_id, sequence";
		break;
	case DBC_SQLITE_INSERT:
		sql_str = "insert into dbc(tran_id, sequence, type, user_idx, table_id, row_data, old_row_data) values(:1, :2, :3, :4, :5, :6, :7)";
		break;
	case DBC_SQLITE_MOVE:
		sql_str = "select tran_id, sequence, type, user_idx, table_id, row_data, old_row_data from dbc where tran_id <= :1";
		break;
	case DBC_SQLITE_SYNC:
		sql_str = "select type, user_idx, table_id, row_data, old_row_data from dbc order by user_idx, tran_id, sequence";
		break;
	default:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error, invalid type given")).str(SGLOCALE));
		return retval;
	}

	if (sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
		string csql_str = "create table dbc(tran_id integer, sequence integer, type integer, user_idx integer, "
			"table_id integer, row_data text, old_row_data text)";
		if (sqlite3_exec(db, csql_str.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create table dbc: {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}

		csql_str = "create unique index idx_dbc on dbc(";
		if (type != DBC_SQLITE_SYNC)
			csql_str += "tran_id, sequence)";
		else
			csql_str += "user_idx, tran_id, sequence)";
		if (sqlite3_exec(db, csql_str.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create index idx_dbc: {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}

		if (sqlite3_reset(stmt) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't reset database: {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}

		if (sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't prepare statement: {1}") % sqlite3_errmsg(db)).str(SGLOCALE));
			return retval;
		}
	}

	retval = true;
	return retval;
}

dbc_sqlite::dbc_sqlite()
{
}

dbc_sqlite::~dbc_sqlite()
{
}

bool dbc_sqlite::get_row(int sqlite_id, sqlite_load_map_t& load_map)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(800, __PRETTY_FUNCTION__, "", &retval);
#endif
	dbc_sqlite_t& sqlite = DBCP->_DBCP_sqlites[sqlite_id];
	sqlite3_stmt *stmt = sqlite.select_stmt;

	int64_t tran_id;
	sqlite_load_t item;
	int status;

RETRY:
	status = sqlite3_step(stmt);
	switch (status) {
	case SQLITE_ROW:
		tran_id = sqlite3_column_int64(stmt, 0);
		item.sqlite_id = sqlite_id;
		item.type = sqlite3_column_int(stmt, 1);
		item.user_idx = sqlite3_column_int(stmt, 2);
		item.table_id = sqlite3_column_int(stmt, 3);
		item.row_size = sqlite3_column_bytes(stmt, 4);
		item.row_data = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
		item.old_row_data = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
		load_map[tran_id] = item;
		break;
	case SQLITE_BUSY:
		boost::this_thread::yield();
		goto RETRY;
	case SQLITE_DONE:
		break;
	default:
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: unknown sqlite error code {1}, {2}") % status % sqlite3_errmsg(sqlite.db)).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

void dbc_sqlite::cleanup()
{
	for (int i = 0; i < MAX_SQLITES; i++) {
		dbc_sqlite_t& sqlite = DBCP->_DBCP_sqlites[i];

		if (sqlite.select_stmt) {
			sqlite3_finalize(sqlite.select_stmt);
			sqlite.select_stmt = NULL;
		}

		if (sqlite.insert_stmt) {
			sqlite3_finalize(sqlite.insert_stmt);
			sqlite.insert_stmt = NULL;
		}

		if (sqlite.delete_stmt) {
			sqlite3_finalize(sqlite.delete_stmt);
			sqlite.delete_stmt = NULL;
		}

		sqlite3_close(sqlite.db);
		sqlite.db = NULL;
	}
}

}
}

