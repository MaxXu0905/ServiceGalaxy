#if !defined(__DBC_SERVER_H__)
#define __DBC_SERVER_H__

#include "dbc_internal.h"
#include <sqlite3.h>

namespace ai
{
namespace sg
{

int const Q_TYPE_INSERT = 0x1;
int const Q_TYPE_UPDATE = 0x2;
int const Q_TYPE_DELETE = 0x4;

struct dbc_partition_t : public dbc_manager
{
	std::string db_name;
	sqlite3 *db;
	sqlite3_stmt *stmt;

	dbc_partition_t();
	~dbc_partition_t();

	bool open(const std::string& data_dir, const std::string& table_name, int partition_id, bool refresh);
	void close();
	bool insert(const char *data, int size);
	bool insert(int type, const char *data, int size);
	bool insert_field(const std::string& fields_string);
	void remove();

private:
	gpp_ctx *GPP;
};

class dbc_server : public dbc_manager
{
public:
	dbc_server();
	~dbc_server();

	void run();
	bool safe_load(dbc_te_t *te);
	bool unsafe_load(dbc_te_t *te);
	bool save_to_file();
	void clean();
	bool extend_shm(int shm_count);
	void wait_ready();

private:
	bool login();
	void logout();
	void watch();
	bool load();
	bool fill_shm(const vector<data_index_t>& meta, const vector<dbc_user_t>& users);
	bool load_table(dbc_te_t *te);
	void refresh_table(dbc_te_t *te);
	bool create_users(const vector<dbc_user_t>& users);
	void load_sequences();
	bool load_from_db();
	bool load_from_file(bool log_error);
	bool load_from_bfile(bool log_error);
	bool load_from_bfile(bool log_error, int partition_id, int mid, const string& pmid);
	bool refresh_from_db();
	bool refresh_from_bfile();
	bool refresh_from_bfile(int partition_id, int mid);
	bool get_sql(const dbc_data_t& te_meta, string& sql, bool refresh);
	void get_struct(const dbc_data_t& te_meta, char *row_data, Generic_ResultSet *rset);
	int get_record(const dbc_data_t& te_meta, char *row_data, Generic_ResultSet *rset);
	void agg_struct(char *dst_row, const char *src_row, const dbc_data_t& te_meta);
	Generic_Database * connect(int user_idx, const dbc_data_t& te_meta);
	void te_clean();
	void rte_clean();
	void chk_tran_timeout();
	void check_index_lock();
	bool db_to_bfile(dbc_switch_t *dbc_switch, dbc_te_t *te);
	bool file_to_bfile(dbc_switch_t *dbc_switch, dbc_te_t *te);
	int get_vsize(const dbc_data_t& te_meta, Generic_ResultSet *rset);
	int get_vsize(const char *src_buf, const dbc_data_t& te_meta);
	int get_vsize(const string& src_buf, const dbc_data_t& te_meta);
	string get_fields_str(const dbc_data_t& te_meta);

	static void s_to_bfile(dbc_switch_t *dbc_switch, dbc_te_t *te, int& retval);
	static void s_load_table(dbc_te_t *te, int& retval);
	static void stdsigterm(int signo);

	template <typename T>
	void agg_number(T& dst_field, const T& src_field, aggtype_enum agg_type, const string& field_name)
	{
		switch (agg_type) {
		case AGGTYPE_MIN:
			dst_field = std::min(dst_field, src_field);
			break;
		case AGGTYPE_MAX:
			dst_field = std::max(dst_field, src_field);
			break;
		case AGGTYPE_SUM:
			dst_field += src_field;
			break;
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Invalid agg_type {1} for field {2}") % agg_type % field_name).str(SGLOCALE));
		}
	}

	template <typename T>
	void agg_string(T *dst_field, const T *src_field, aggtype_enum agg_type, const string& field_name)
	{
		switch (agg_type) {
		case AGGTYPE_MIN:
			if (strcmp(dst_field, src_field) > 0)
				strcpy(dst_field, src_field);
			break;
		case AGGTYPE_MAX:
			if (strcmp(dst_field, src_field) < 0)
				strcpy(dst_field, src_field);
			break;
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Invalid agg_type {1} for field {2}") % agg_type % field_name).str(SGLOCALE));
		}
	}

	char *orow_buf;
	char *row_buf;
	int orow_buf_size;
	int row_buf_size;
	time_t login_time;

	friend class dbc_msvr;
};

}
}

#endif

