#if !defined(__DBC_SQLITE_H__)
#define __DBC_SQLITE_H__

#include <set>
#include <sqlite3.h>
#include "dbc_lock.h"
#include "dbc_redo.h"
#include "dbc_struct.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

struct sqlite_load_t
{
	int sqlite_id;
	int type;
	int user_idx;
	int table_id;
	int row_size;
	const char *row_data;
	const char *old_row_data;
};

typedef map<int64_t, sqlite_load_t> sqlite_load_map_t;

class dbc_sqlite : public dbc_manager
{
public:
	static dbc_sqlite& instance(dbct_ctx *DBCT);

	bool save(const std::set<redo_prow_t>& row_set, int flags);
	bool load();
	bool persist(int64_t tran_id);
	bool move2sync(sqlite3 *target_db, int64_t target_tran_id);
	bool insert(sqlite3 *db, sqlite3_stmt *stmt, int64_t tran_id, int64_t sequence, int type, int user_idx,
		int table_id, int row_size, int old_row_size, const char *row_data, const char *old_row_data);
	bool erase(sqlite3 *db, sqlite3_stmt *stmt, int user_idx, int64_t tran_id);
	bool truncate(int table_id);
	int get_sqlite();
	void lock_sqlite(int sqlite_id);
	void put_sqlite(int sqlite_id);
	bool open(const string& db_name, sqlite3 *& db);
	void close(sqlite3 *& db);
	bool prepare(sqlite3 *db, sqlite3_stmt *& stmt, int type);

private:
	dbc_sqlite();
	virtual ~dbc_sqlite();

	bool get_row(int sqlite_id, sqlite_load_map_t& load_map);
	void cleanup();

	friend class dbct_ctx;
};

}
}

#endif

