#if !defined(__DBC_CONTROL_H__)
#define __DBC_CONTROL_H__

#include "sql_macro.h"
#include "sql_control.h"
#include "sql_extern.h"
#include "sql_select.h"
#include "sql_insert.h"
#include "sql_update.h"
#include "sql_delete.h"
#include "sql_create.h"
#include "sql_drop.h"
#include "sql_truncate.h"
#include "compiler.h"
#include "gpp_ctx.h"
#include "gpt_ctx.h"
#include "dbc_struct.h"
#include "dbcp_ctx.h"
#include "dbct_ctx.h"
#include "common.h"

namespace ai
{
namespace sg
{

class dbc_api;
class dbc_cursor;

}
}

namespace ai
{
namespace scci
{

using namespace ai::sg;

class Dbc_ResultSet;
class Dbc_Statement;

sqltype_enum const BIND_DEFAULT_SQLTYPE = SQLTYPE_STRING;
int const BIND_DEFAULT_LENGTH = 1024;

class group_data_t
{
public:
	group_data_t();
	~group_data_t();

	bool operator<(const group_data_t& rhs) const;

	char *data;
	long rows;
};

class group_order_compare
{
public:
	group_order_compare(compiler::func_type order_func_);

	bool operator()(char *left, char *right) const;

private:
	compiler::func_type order_func;
};

struct select_data_t
{
	std::string code;
	std::string agg_func_name;
	int field_id;
	sqltype_enum field_type;
	int field_length;
	int field_offset;
};

struct order_data_t
{
	std::string field_name;
	ordertype_enum field_type;
};

class Dbc_Database : public Generic_Database
{
public:
	Dbc_Database();
	~Dbc_Database();

	dbtype_enum get_dbtype();
	void connect(const std::map<std::string, std::string>& conn_info);
	void disconnect();
	void commit();
	void rollback();
	Generic_Statement * create_statement();
	void terminate_statement(Generic_Statement *& stmt);

private:
	dbct_ctx *DBCT;
	dbc_api& api_mgr;
	std::ostringstream fmt;

	friend class Dbc_Statement;
};

class Dbc_Statement : public Generic_Statement
{
public:
	dbtype_enum get_dbtype();
	int getUpdateCount() const;
	Generic_ResultSet * executeQuery();
	void executeUpdate();
	void executeArrayUpdate(int arrayLength);
	void closeResultSet(Generic_ResultSet *& rset);

	bool isNull(int colIndex);
	bool isTruncated(int colIndex);
	void setNull(int paramIndex);

	void setExecuteMode(execute_mode_enum execute_mode_);
	execute_mode_enum getExecuteMode() const;

	date date2native(void *x) const;
	ptime time2native(void *x) const;
	datetime datetime2native(void *x) const;
	sql_datetime sqldatetime2native(void *x) const;
	void native2date(void *addr, const date& x) const;
	void native2time(void *addr, const ptime& x) const;
	void native2datetime(void *addr, const datetime& x) const;
	void native2sqldatetime(void *addr, const sql_datetime& x) const;

	void setRowOffset(ub4 rowOffset_);
	int getRowsErrorCount() const;
	ub4 * getRowsError();
	sb4 * getRowsErrorCode();

private:
	Dbc_Statement(Dbc_Database *db_);
	virtual ~Dbc_Statement();

	void prepare();
	void execute(int arrayLength);
	void clearNull(char *addr);

	int prepare_select(sql_select *stmt);
	int prepare_insert(sql_insert *stmt);
	int prepare_update(sql_update *stmt);
	int prepare_delete(sql_delete *stmt);
	void parse_where(sql_statement *stmt, relation_item_t *rela_item);
	void gen_where();
	void gen_wkey_by_rowid(sql_statement *stmt);
	void gen_wkey_by_index(sql_statement *stmt);
	void match_index(relation_item_t *rela_item);
	bool cond_is_index_style(relation_item_t *rela_item);
	void parse_order(sql_select *stmt);
	bool is_grouped(sql_select *stmt);
	void gen_direct_select(sql_select *stmt);
	void gen_group_select(sql_select *stmt);
	void get_order_fields(sql_select *stmt);
	void parse_having(sql_select *stmt);
	void get_field_map();
	void get_select_map(sql_select *stmt);
	void describe();
	void gen_format(sqltype_enum field_type, string& str1, string& str2, string& str3);

	Dbc_Database *db;

	ub4 *pRowsError;
	int iRowsErrorCount;
	sb4 *pRowsErrorCode;

	ub4 rowOffset;
	int update_count;

	execute_mode_enum execute_mode;

	dbcp_ctx *DBCP;
	dbct_ctx *DBCT;
	compiler *cmpl;
	dbc_te_t *prepared_te;
	std::string wkey_code;
	std::string where_code;
	std::string set_code;
	std::string order_code;
	std::string group_code;
	std::string agg_code;
	std::string gcomp_code;
	std::string having_code;

	std::map<std::string, select_data_t> select_map;
	// In where statement, parse 'key = statement' pairs to map
	std::map<std::string, std::string> cond_map;
	std::vector<std::string> cond_codes;
	std::set<std::string> group_codes;
	std::vector<order_data_t> order_fields;

	compiler::func_type where_func;
	compiler::func_type wkey_func;
	compiler::func_type set_func;
	compiler::func_type order_func;
	compiler::func_type group_func;
	compiler::func_type agg_func;
	compiler::func_type gcomp_func;
	compiler::func_type having_func;
	int row_size;

	dbc_api& api_mgr;
	int index_id;
	bool use_index_hash;
	stmttype_enum stmt_type;
	std::auto_ptr<dbc_cursor> cursor;

	std::map<string, sql_field_t> field_map;

	friend class Dbc_Database;
	friend class Dbc_ResultSet;
};

class Dbc_ResultSet : public Generic_ResultSet
{
public:
	dbtype_enum get_dbtype();
	Status next();
	Status batchNext();
	int getFetchRows() const;

	bool isNull(int colIndex);
	bool isTruncated(int paramIndex);

	date date2native(void *x) const;
	ptime time2native(void *x) const;
	datetime datetime2native(void *x) const;
	sql_datetime sqldatetime2native(void *x) const;

private:
	Dbc_ResultSet(Dbc_Statement *stmt_);
	virtual ~Dbc_ResultSet();

	void group();

	Dbc_Statement *stmt;
	std::auto_ptr<dbc_cursor>& cursor;

	int fetched_rows;		// Ô¤ÌáÈ¡Êý
	Status batch_status;

	dbct_ctx *DBCT;
	std::set<group_data_t> group_set;
	std::vector<char *> data_vector;
	std::vector<char *>::iterator curr_data;
	bool use_group;

	friend class Dbc_Statement;
};

}
}

#endif

