#if !defined(__SQL_BASE_H__)
#define __SQL_BASE_H__

#include <vector>
#include <string>
#include "machine.h"

namespace ai
{
namespace scci
{

using namespace std;

extern string print_prefix;

enum dbtype_enum
{
	DBTYPE_ORACLE = 0,
	DBTYPE_ALTIBASE = 1,
	DBTYPE_DBC = 2,
	DBTYPE_GP=3
};

enum stmttype_enum
{
	STMTTYPE_SELECT = 0,
	STMTTYPE_INSERT = 1,
	STMTTYPE_UPDATE = 2,
	STMTTYPE_DELETE = 3,
	STMTTYPE_CREATE = 4,
	STMTTYPE_DROP = 5,
	STMTTYPE_TRUNCATE = 6
};

enum wordtype_enum
{
	WORDTYPE_CONSTANT = 0,
	WORDTYPE_DCONSTANT = 1,
	WORDTYPE_STRING = 2,
	WORDTYPE_IDENT = 3,
	WORDTYPE_NULL = 4
};

enum sqltype_enum
{
	SQLTYPE_UNKNOWN = -1,
	SQLTYPE_CHAR = 0,
	SQLTYPE_UCHAR = 1,
	SQLTYPE_SHORT = 2,
	SQLTYPE_USHORT = 3,
	SQLTYPE_INT = 4,
	SQLTYPE_UINT = 5,
	SQLTYPE_LONG = 6,
	SQLTYPE_ULONG = 7,
	SQLTYPE_FLOAT = 8,
	SQLTYPE_DOUBLE = 9,
	SQLTYPE_STRING = 10,
	SQLTYPE_DATE = 11,
	SQLTYPE_TIME = 12,
	SQLTYPE_DATETIME = 13,
	SQLTYPE_VSTRING = 14,
	SQLTYPE_ANY = 15
};

enum optype_enum
{
	OPTYPE_OR = 0,
	OPTYPE_AND = 1,
	OPTYPE_NOT = 2,
	OPTYPE_GT = 3,
	OPTYPE_LT = 4,
	OPTYPE_GE = 5,
	OPTYPE_LE = 6,
	OPTYPE_EQ = 7,
	OPTYPE_NEQ = 8,
	OPTYPE_ADD = 9,
	OPTYPE_SUB = 10,
	OPTYPE_STRCAT = 11,
	OPTYPE_MUL = 12,
	OPTYPE_DIV = 13,
	OPTYPE_MOD = 14,
	OPTYPE_UMINUS = 15,
	OPTYPE_IN = 16,
	OPTYPE_NOT_IN = 17,
	OPTYPE_BETWEEN = 18,
	OPTYPE_EXISTS = 19,
	OPTYPE_IS_NULL = 20,
	OPTYPE_IS_NOT_NULL = 21
};

enum itemtype_enum
{
	ITEMTYPE_SIMPLE = 0,
	ITEMTYPE_FUNC = 1,
	ITEMTYPE_RELA = 2,
	ITEMTYPE_COMPOSIT = 3
};

enum ordertype_enum
{
	ORDERTYPE_NONE = 0,
	ORDERTYPE_ASC = 1,
	ORDERTYPE_DESC = 2
};

struct sql_item_t
{
	itemtype_enum type;
	void *data;
};

struct sql_field_t
{
	sqltype_enum field_type;
	int field_offset;
	int internal_offset;
};

struct bind_field_t
{
	sqltype_enum field_type;
	string field_name;
	int field_length;
	long field_offset;
	int ind_length;
	long ind_offset;
};

struct table_field_t
{
	string table_name;
	int pos;
};

struct column_desc_t
{
	sqltype_enum field_type;
	int field_length;
};

int const GEN_CODE_DOT = 0x1;			// 使用.分隔符
int const GEN_CODE_USE_INDEX = 0x2;		// 可以使用索引
int const GEN_CODE_HAS_AGG = 0x4;		// 有聚合函数
int const GEN_CODE_INTERNAL = 0x8;		// 使用内部标识

struct gen_code_t
{
	gen_code_t(const map<string, sql_field_t> *field_map_, const bind_field_t *bind_fields_);
	~gen_code_t();

	bool dot_flag() const { return (flags & GEN_CODE_DOT); }
	bool can_use_index() const { return (flags & GEN_CODE_USE_INDEX); }
	bool has_agg_func() const { return (flags & GEN_CODE_HAS_AGG); }
	bool use_internal() const { return (flags & GEN_CODE_INTERNAL); }

	void set_dot_flag() { flags |= GEN_CODE_DOT; }
	void set_can_use_index() { flags |= GEN_CODE_USE_INDEX; }
	void set_has_agg_func() { flags |= GEN_CODE_HAS_AGG; }
	void set_use_internal() { flags |= GEN_CODE_INTERNAL; }

	void clear_dot_flag() { flags &= ~GEN_CODE_DOT; }
	void clear_can_use_index() { flags &= ~GEN_CODE_USE_INDEX; }
	void clear_has_agg_func() { flags &= ~GEN_CODE_HAS_AGG; }
	void clear_use_internal() { flags &= ~GEN_CODE_INTERNAL; }

	int flags;
	string code;
	const map<string, sql_field_t> *field_map;
	const bind_field_t *bind_fields;
	int row_index;
};

struct simple_item_t
{
	simple_item_t();
	~simple_item_t();

	void gen_sql(string& sql, bool dot_flag) const;
	void gen_code(gen_code_t& para) const;
	void print() const;
	void push(vector<sql_item_t>& vector_root);

	wordtype_enum word_type;
	string schema;
	string value;
};

struct field_desc_t
{
	field_desc_t();
	~field_desc_t();

	void gen_sql(string& sql, bool dot_flag) const;
	void gen_code(gen_code_t& para) const;
	void print() const;

	sqltype_enum type;
	int len;
	string alias;
};

struct composite_item_t;
struct relation_item_t
{
	relation_item_t();
	relation_item_t(optype_enum op_, composite_item_t *left_, composite_item_t *right_);
	~relation_item_t();

	void gen_sql(string& sql, bool dot_flag) const;
	void gen_code(gen_code_t& para) const;
	void print() const;
	void push(vector<sql_item_t>& vector_root);

	optype_enum op;
	composite_item_t *left;
	composite_item_t *right;
	bool has_curve;
};

struct sql_func_t;
class sql_select;
typedef vector<composite_item_t *> arg_list_t;
struct composite_item_t
{
	composite_item_t();
	~composite_item_t();

	void gen_sql(string& sql, bool dot_flag) const;
	void gen_code(gen_code_t& para) const;
	void print() const;
	void push(vector<sql_item_t>& vector_root);

	simple_item_t *simple_item;
	sql_func_t *func_item;
	relation_item_t *rela_item;
	field_desc_t *field_desc;
	arg_list_t *arg_item;
	sql_select *select_item;
	ordertype_enum order_type;
	bool has_curve;
};

struct sql_func_t
{
	sql_func_t();
	~sql_func_t();

	void gen_sql(string& sql, bool dot_flag) const;
	void gen_code(gen_code_t& para) const;
	void print() const;
	void push(vector<sql_item_t>& vector_root);

	string func_name;
	arg_list_t *args;

	static const char *agg_functions[];
};

}
}

#endif

