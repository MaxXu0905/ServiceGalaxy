#if !defined(__SQL_STATEMENT_H__)
#define __SQL_STATEMENT_H__

#include "gpp_ctx.h"
#include "gpt_ctx.h"
#include "sql_base.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;
using namespace std;

class struct_dynamic;

struct combine_field_t
{
	sqltype_enum field_type;
	int field_length;
};

class sql_statement
{
public:
	sql_statement();
	virtual ~sql_statement();

	static void add_statement(sql_statement *stmt);
	static void set_h_path(const string& h_path_);
	static void set_cpp_path(const string& cpp_path_);
	static void set_file_prefix(const string& file_prefix_);
	static void set_class_name(const string& class_name_);
	static void set_array_size(int array_size_);
	static void set_reqire_ind(bool require_ind_);
	static void set_dbtype(dbtype_enum dbtype_);
	static dbtype_enum get_dbtype();
	static int get_bind_pos();
	void set_stmt_type(stmttype_enum stmt_type_);
	stmttype_enum get_stmt_type() const;
	bool is_root() const;
	void set_root();
	void set_comments(char *comments_);

	static bool gen_data_static();
	static bool gen_data_dynamic(struct_dynamic *data_);
	virtual string gen_sql() = 0;
	virtual string gen_table() = 0;

	static void print_data();
	static void clear();
	static sql_statement * first();

protected:
	bool (sql_statement::*select_item_action)(const composite_item_t& composite_item);
	bool (sql_statement::*bind_item_action)(const composite_item_t& composite_item);
	void (sql_statement::*finish_action)();

	gpp_ctx *GPP;
	gpt_ctx *GPT;
	ostringstream fmt;

	map<string, int> table_pos_map;
	stmttype_enum stmt_type;
	bool root_flag;
	string comments;
	static bool dynamic_flag;

private:
	virtual bool gen_data() = 0;
	virtual void print() const = 0;

	bool gen_select_static(const composite_item_t& composite_item);
	bool gen_bind_static(const composite_item_t& composite_item);
	void gen_static();
	bool gen_select_dynamic(const composite_item_t& composite_item);
	bool gen_bind_dynamic(const composite_item_t& composite_item);
	void gen_dynamic();

	static string h_path;
	static string cpp_path;
	static string file_prefix;
	static string filename_h;
	static string filename_cpp;
	static string class_name;
	static int array_size;
	static bool require_ind;
	static dbtype_enum dbtype;
	static string ind_type_str;
	static string date_type_str;
	static string time_type_str;
	static string datetime_type_str;
	static int ind_type_size;
	static int date_type_size;
	static int time_type_size;
	static int datetime_type_size;
	static int bind_pos;

	// Used for generate data struct statically
	ostringstream data_select_fmt;
	ostringstream data_bind_fmt;
	static ostringstream data_field_fmt;
	static map<string, combine_field_t> data_map;
	static vector<sql_statement *> stmt_vector;

	// Used for generate data struct dynamically
	struct_dynamic *data;
};

}
}

#endif

