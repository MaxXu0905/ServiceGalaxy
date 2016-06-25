#if !defined(__BUILD_META_H__)
#define __BUILD_META_H__

#include "dbc_internal.h"

using namespace std;

namespace ai
{
namespace sg
{

int const PARAM_MODE_IN = 0;		// 输入参数
int const PARAM_MODE_OUT = 1;	// 输出参数
int const PARAM_MODE_INOUT = 2;	// 输入输出参数

class build_meta : public dbc_manager
{
public:
	build_meta();
	~build_meta();

	void gen_switch();
	void gen_structs();
	void gen_searchs();
	void gen_search_headers();
	void gen_search_defs();
	void gen_exp();

private:
	typedef map<dbc_index_key_t, dbc_index_t>::value_type index_value_type;
	typedef map<dbc_index_key_t, dbc_func_t>::value_type func_value_type;

	void gen_struct(int table_id, const dbc_data_t& dbc_data, const string& struct_name, ostream& os);
	void gen_search(const func_value_type& fitem, ostream& os, bool is_dbc, bool is_array);
	void gen_search_header(const func_value_type& fitem, ostream& os, bool is_array);
	void gen_search_def(const func_value_type& fitem, ostream& os, bool is_array);
	void to_struct(const string& table_name, string& struct_name);
	void to_function(const string& table_name, int index, string& func_name, bool is_array);
	void to_macro(const string& table_name, string& macro_name);
	void gen_equal_func(const string& struct_name, const dbc_data_t& dbc_data, ostream& os, bool internal);
	void gen_partition_func(const string& struct_name, const dbc_data_t& dbc_data, ostream& os, bool internal);
	void gen_print_func(const string& struct_name, const dbc_data_t& dbc_data, ostream& os, bool internal);
	void gen_hash_func(const string& struct_name, const dbc_data_t& dbc_data, const index_value_type& iitem, ostream& os, bool internal);
	void gen_compare_func(const string& struct_name, const dbc_data_t& dbc_data, const index_value_type& iitem, ostream& os, bool exact, bool internal);
	void gen_special(const dbc_index_t& dbc_index, int special_type, const string& field_name, ostream& os);
	const dbc_data_t& find_dbc_data(int table_id);
	const dbc_data_field_t * find_data_field(const dbc_data_t& dbc_data, const string& field_name);
	const dbc_index_field_t * find_index_field(const dbc_index_t& dbc_index, const string& field_name);
	int get_param_mode(const dbc_index_key_t& index_key, const dbc_func_field_t& field);
	bool has_ref(const dbc_func_t& dbc_func, const string& field_name);
	void gen_format(const string& table_name, const dbc_data_field_t& field, string& str, bool internal);
	void gen_compare_part(sqltype_enum field_type, int search_type, const string& index_name,
		const string& field_name1, const string& field_name2, ostream& os, bool match, bool internal);

	string sgdir;
	const map<int, dbc_data_t>& data_map;
	const map<dbc_index_key_t, dbc_index_t>& index_map;
	const map<dbc_index_key_t, dbc_func_t>& func_map;
	ostringstream fmt;
};

}
}

#endif

