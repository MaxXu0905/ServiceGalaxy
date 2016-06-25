#include "build_meta.h"

namespace ai
{
namespace sg
{

using namespace std;

build_meta::build_meta()
	: data_map(dbc_config::instance().get_data_map()),
	  index_map(dbc_config::instance().get_index_map()),
	  func_map(dbc_config::instance().get_func_map())
{
	char *ptr = ::getenv("SGROOT");
	if (ptr == NULL)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: SGROOT environment not set")).str(SGLOCALE));

	sgdir = ptr;
}

build_meta::~build_meta()
{
}

void build_meta::gen_switch()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	string filename = sgdir + "/src/meta_switch.cpp";
	ofstream ofs(filename.c_str());
	if (!ofs)
		throw bad_msg(__FILE__, __LINE__, 40, (_("ERROR: Can't open file {1}") % filename).str(SGLOCALE));

	ofs << "#include <iostream>\n"
		<< "#include <iomanip>\n"
		<< "#include \"dbc_struct.h\"\n"
		<< "#include \"datastruct_structs.h\"\n\n"
		<< "using namespace ai::sg;\n"
		<< "using namespace std;\n\n"
		<< "static dbc_switch_t **dbc_switches = NULL;\n"
		<< "static pthread_once_t once_control = PTHREAD_ONCE_INIT;\n"
		<< "static bool inited = false;\n\n";

	ofs << "static void init()\n"
		<< "{\n";

	int switch_count = 0;
	map<int, dbc_data_t>::const_reverse_iterator riter = data_map.rbegin();
	if (riter != data_map.rend())
		switch_count = riter->first;

	ofs << "\tdbc_switch_t *dbc_switch;\n"
		<< "\tint const SWITCH_COUNT = " << switch_count << ";\n\n"
		<< "\tdbc_switches = new dbc_switch_t *[SWITCH_COUNT + 2];\n"
		<< "\tmemset(dbc_switches, '\\0', sizeof(dbc_switch_t *) * (SWITCH_COUNT + 2));\n\n";

	for (map<int, dbc_data_t>::const_iterator iter = data_map.begin(); iter != data_map.end(); ++iter) {
		dbc_index_key_t index_key;
		map<dbc_index_key_t, dbc_index_t>::const_iterator index_iter;

		index_key.table_id = iter->first;
		index_key.index_id = 0;
		map<dbc_index_key_t, dbc_index_t>::const_iterator lower_iter = index_map.lower_bound(index_key);

		string struct_name;
		const dbc_data_t& dbc_data = find_dbc_data(iter->first);
		string table_name = dbc_data.table_name;

		to_struct(table_name, struct_name);
		struct_name = string("__") + struct_name;

		int max_index = -1;
		for (index_iter = lower_iter; index_iter != index_map.end() && index_iter->first.table_id == iter->first; ++index_iter)
			max_index = std::max(max_index, index_iter->first.index_id);

		ofs << "\tdbc_switch = new dbc_switch_t();\n"
			<< "\tdbc_switches[" << iter->first << "] = dbc_switch;\n"
			<< "\tdbc_switch->max_index = " << max_index << ";\n"
			<< "\tdbc_switch->struct_size = " << dbc_data.struct_size << ";\n"
			<< "\tdbc_switch->internal_size = " << dbc_data.internal_size << ";\n"
			<< "\tdbc_switch->extra_size = " << dbc_data.extra_size << ";\n"
			<< "\tdbc_switch->equal = " << struct_name << "::equal;\n"
			<< "\tdbc_switch->get_partition = " << struct_name << "::get_partition;\n"
			<< "\tdbc_switch->print = " << struct_name << "::print;\n"
			<< "\tdbc_switch->index_switches = ";
		if (max_index < 0)
			ofs << "NULL;\n";
		else
			ofs << "new dbc_index_switch_t[" << max_index << " + 1];\n";

		int i = 0;
		for (index_iter = lower_iter; index_iter != index_map.end() && index_iter->first.table_id == iter->first; ++index_iter, i++) {
			ofs << "\tdbc_switch->index_switches[" << i << "].get_hash = " << struct_name << "::get_hash" << i << ";\n"
				<< "\tdbc_switch->index_switches[" << i << "].compare = "<< struct_name << "::compare" << i << ";\n"
				<< "\tdbc_switch->index_switches[" << i << "].compare_exact = "<< struct_name << "::compare_exact" << i << ";\n";
		}
		ofs << "\n";
	}

	ofs << "\tinited = true;\n"
		<< "}\n\n";

	ofs << "extern \"C\"\n"
		<< "dbc_switch_t ** get_switch()\n"
		<< "{\n"
		<< "\tif (!inited)\n"
		<< "\t\tpthread_once(&once_control, init);\n\n"
		<< "\treturn dbc_switches;\n"
		<< "}\n\n";

	// 创建MD5SUM
	dbc_config& config_mgr = dbc_config::instance();
	string result;
	if (!config_mgr.md5sum(result, true))
		throw bad_msg(__FILE__, __LINE__, 0, "ERROR: Can't generate client md5sum");

	ofs << "extern \"C\"\n"
		<< "const char * get_client_md5sum()\n"
		<< "{\n"
		<< "\treturn \"" << result << "\";\n"
		<< "}\n\n";
}

void build_meta::gen_structs()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif
	map<int, dbc_data_t>::const_iterator iter;

	string filename = sgdir + "/include/datastruct_structs.h";
	ofstream ofs(filename.c_str());
	if (!ofs)
		throw bad_msg(__FILE__, __LINE__, 40, (_("ERROR: Can't open file {1}") % filename).str(SGLOCALE));

	ofs << "#if !defined(__DATASTRUCT_STRUCTS_H__)\n"
		<< "#define __DATASTRUCT_STRUCTS_H__\n\n";

	ofs << "namespace ai\n"
		<< "{\n"
		<< "namespace sg\n"
		<< "{\n\n";

	// 创建enum enTableName
	ofs << "enum enTableName {\n";
	for (iter = data_map.begin(); iter != data_map.end(); ++iter) {
		string macro_name;
		to_macro(iter->second.table_name, macro_name);
		ofs << "\t" << macro_name << " = " << iter->first << ",\n";
	}
	if (!data_map.empty())
		ofs << "\tT_NUM\n";
	else
		ofs << "\tT_NUM = 1\n";
	ofs << "};\n\n";

	ofs << "}\n"
		<< "}\n\n";

	scan_file<> h_scan(sgdir + "/include", "^t_.*\\.h$");
	vector<string> h_files;
	h_scan.get_files(h_files);

	scan_file<> cpp_scan(sgdir + "/src", "^t_.*\\.cpp$");
	vector<string> cpp_files;
	cpp_scan.get_files(cpp_files);

	scan_file<> o_scan(sgdir + "/src", "^t_.*\\.o$");
	vector<string> o_files;
	o_scan.get_files(o_files);

	for (iter = data_map.begin(); iter != data_map.end(); ++iter) {
		const dbc_data_t& dbc_data = iter->second;
		string struct_name;

		to_struct(dbc_data.table_name, struct_name);

		string h_name = struct_name + ".h";
		for (vector<string>::iterator str_iter = h_files.begin(); str_iter != h_files.end(); ++str_iter) {
			if (*str_iter == h_name) {
				h_files.erase(str_iter);
				break;
			}
		}

		string cpp_name = struct_name + ".cpp";
		for (vector<string>::iterator str_iter = cpp_files.begin(); str_iter != cpp_files.end(); ++str_iter) {
			if (*str_iter == cpp_name) {
				cpp_files.erase(str_iter);
				break;
			}
		}

		string o_name = struct_name + ".o";
		for (vector<string>::iterator str_iter = o_files.begin(); str_iter != o_files.end(); ++str_iter) {
			if (*str_iter == o_name) {
				o_files.erase(str_iter);
				break;
			}
		}

		gen_struct(iter->first, dbc_data, struct_name, ofs);
	}

	ofs << "\n#endif\n\n";

	// 删除已经不用的文件
	BOOST_FOREACH(const string& file, h_files) {
		string full_name = string("include/") + file;
		(void)::remove(full_name.c_str());
	}

	BOOST_FOREACH(const string& file, cpp_files) {
		string full_name = string("src/") + file;
		(void)::remove(full_name.c_str());
	}

	BOOST_FOREACH(const string& file, o_files) {
		string full_name = string("src/") + file;
		(void)::remove(full_name.c_str());
	}
}

void build_meta::gen_struct(int table_id, const dbc_data_t& dbc_data, const string& struct_name, ostream& os)
{
	string tabs_prefix;
	string oper_str;
	string upper_name;
	string str;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, struct_name={2}") % table_id % struct_name).str(SGLOCALE), NULL);
#endif

	upper_name = struct_name;
	std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), toupper_predicate());

	os << "#include \"" << struct_name << ".h\"\n";

	string h_file = sgdir + "/include/" + struct_name + ".h";
	string cpp_file = sgdir + "/src/" + struct_name + ".cpp";
	string h_text;
	string cpp_text;

	ifstream h_ifs(h_file.c_str());
	if (h_ifs.good()) {
		h_ifs.seekg(0, std::ios::end);
		size_t size = h_ifs.tellg();

		boost::shared_array<char> auto_data(new char[size]);
		char *data = auto_data.get();

		h_ifs.seekg(0, std::ios_base::beg);
		if (h_ifs.read(data, size).good())
			h_text.assign(data, data + size);
	}

	ifstream cpp_ifs(cpp_file.c_str());
	if (cpp_ifs.good()) {
		cpp_ifs.seekg(0, std::ios::end);
		size_t size = cpp_ifs.tellg();

		boost::shared_array<char> auto_data(new char[size]);
		char *data = auto_data.get();

		cpp_ifs.seekg(0, std::ios_base::beg);
		if (cpp_ifs.read(data, size).good())
			cpp_text.assign(data, data + size);
	}

	ostringstream h_fmt;
	ostringstream cpp_fmt;

	h_fmt << "#if !defined(__" << upper_name << "_H__)\n"
		<< "#define __" << upper_name << "_H__\n\n"
		<< "#include <string.h>\n"
		<< "#include <stdlib.h>\n"
		<< "#include <stdio.h>\n"
		<< "#include <time.h>\n"
		<< "#include <iostream>\n"
		<< "#include <sstream>\n"
		<< "#include <iomanip>\n\n"
		<< "namespace ai\n"
		<< "{\n"
		<< "namespace sg\n"
		<< "{\n\n";

	cpp_fmt << "#include \"" << struct_name << ".h\"\n\n"
		<< "namespace ai\n"
		<< "{\n"
		<< "namespace sg\n"
		<< "{\n\n"
		<< "using namespace std;\n\n";

	for (int loop = 0; loop < 2; loop++) {
		string real_struct_name;
		bool internal;
		if (loop == 0) {
			internal = false;
			real_struct_name = struct_name;
		} else {
			internal = true;
			real_struct_name = string("__") + struct_name;
		}

		// 结构开始
		h_fmt << "struct " << real_struct_name << "\n"
			<< "{\n";

		bool is_variable = false;
		if (internal) {
			for (int i = 0; i < dbc_data.field_size; i++) {
				const dbc_data_field_t& field = dbc_data.fields[i];

				if (field.field_type == SQLTYPE_VSTRING) {
					is_variable = true;
					break;
				}
			}
		}

		// 输出字段
		for (int i = 0; i < dbc_data.field_size; i++) {
			const dbc_data_field_t& field = dbc_data.fields[i];
			gen_format(dbc_data.table_name, field, str, internal);
			h_fmt << "\t" << str << " " << field.field_name;

			if (!internal) {
				if (field.field_type == SQLTYPE_STRING || field.field_type == SQLTYPE_VSTRING)
					h_fmt << "[" << (field.field_size - 1) << " + 1];\n";
				else
					h_fmt << ";\n";
			} else {
				if (field.field_type == SQLTYPE_STRING)
					h_fmt << "[" << (field.field_size - 1) << " + 1];\n";
				else
					h_fmt << ";\n";
			}
		}

		if (is_variable)
			h_fmt << "\tchar placeholder[1];\n";

		h_fmt << "\n";

		// 主键是否相等比较函数
		h_fmt << "\tstatic long equal(const void *key, const void *data);\n";
		gen_equal_func(real_struct_name,  dbc_data, cpp_fmt, internal);
		// 生成分区关键字
		h_fmt << "\tstatic void get_partition(char *key, const void *data);\n";
		gen_partition_func(real_struct_name, dbc_data, cpp_fmt, internal);
		// 生成打印函数
		h_fmt << "\tstatic void print(const void *data);\n";
		gen_print_func(real_struct_name, dbc_data, cpp_fmt, internal);

		dbc_index_key_t index_key;
		index_key.table_id = table_id;
		index_key.index_id = 0;

		map<dbc_index_key_t, dbc_index_t>::const_iterator index_iter = index_map.lower_bound(index_key);;
		for (; index_iter != index_map.end() && index_iter->first.table_id == table_id; ++index_iter) {
			// 生成哈希关键字
			h_fmt << "\tstatic void get_hash" << index_iter->first.index_id << "(char *key, const void *data);\n";
			gen_hash_func(real_struct_name, dbc_data, *index_iter, cpp_fmt, internal);
			// 生成比较函数
			h_fmt << "\tstatic long compare" << index_iter->first.index_id << "(const void *key, const void *data);\n";
			gen_compare_func(real_struct_name, dbc_data, *index_iter, cpp_fmt, false, internal);
			// 线性查找时的比较函数
			h_fmt << "\tstatic long compare_exact" << index_iter->first.index_id << "(const void *key, const void *data);\n";
			gen_compare_func(real_struct_name, dbc_data, *index_iter, cpp_fmt, true, internal);

		}

		// 结构结束
		h_fmt << "};\n\n";
	}

	// 结构结束
	h_fmt << "}\n"
		<< "}\n\n"
		<< "#endif\n\n";

	cpp_fmt << "}\n"
		<< "}\n\n";

	string h_str = h_fmt.str();
	string cpp_str = cpp_fmt.str();

	// 内容没有变化，跳过重新生成
	if (h_str == h_text && cpp_str == cpp_text)
		return;

	ofstream h_ofs(h_file.c_str());
	if (!h_ofs)
		throw bad_msg(__FILE__, __LINE__, 40, (_("ERROR: Can't open file {1}") % h_file).str(SGLOCALE));

	ofstream cpp_ofs(cpp_file.c_str());
	if (!cpp_ofs)
		throw bad_msg(__FILE__, __LINE__, 40, (_("ERROR: Can't open file {1}") % cpp_file).str(SGLOCALE));

	h_ofs << h_str;
	cpp_ofs << cpp_str;
}

void build_meta::gen_searchs()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	for (int i = 0; i < 2; i++) {
		string filename = sgdir;

		if (i == 0)
			filename += "/src/search_dbc.cpp";
		else
			filename += "/src/search_sdc.cpp";

		ofstream ofs(filename.c_str());
		if (!ofs)
			throw bad_msg(__FILE__, __LINE__, 40, (_("ERROR: Can't open file {1}") % filename).str(SGLOCALE));

		ofs << "#include <iostream>\n"
			<< "#include \"common.h\"\n"
			<< "#include \"datastruct_structs.h\"\n";

		if (i == 0)
			ofs << "#include \"dbc_api.h\"\n";
		else
			ofs << "#include \"sdc_api.h\"\n";

		ofs << "#include \"perf_stat.h\"\n"
			<< "#include \"dstream.h\"\n\n"
			<< "extern \"C\"\n"
			<< "{\n\n"
			<< "using namespace ai::sg;\n"
			<< "using namespace std;\n\n"
			<< "// module_id = '13', so add 13 * 10,000,000\n"
			<< "int const ERROR_OFFSET = 130000000;\n\n";

		// 状态检查，保证libdbcsearch.so和libsdcsearch.so不能同时链接
		ofs << "namespace\n"
			<< "{\n"
			<< "\n"
			<< "class search_registry\n"
			<< "{\n"
			<< "public:\n"
			<< "\tsearch_registry() {\n"
			<< "\t\tdbcp_ctx *DBCP = dbcp_ctx::instance();\n\n";
		if (i == 0) {
			ofs << "\t\tif (DBCP->_DBCP_search_type == SEARCH_TYPE_SDC)\n"
				<< "\t\t\tabort();\n\n"
				<< "\t\tDBCP->_DBCP_search_type = SEARCH_TYPE_DBC;\n";
		} else {
			ofs << "\t\tif (DBCP->_DBCP_search_type == SEARCH_TYPE_DBC)\n"
				<< "\t\t\tabort();\n\n"
				<< "\t\tDBCP->_DBCP_search_type = SEARCH_TYPE_SDC;\n";
		}
		ofs << "\t}\n"
			<< "} search_instance;\n\n"
			<< "}\n\n";

		// 创建MD5SUM
		dbc_config& config_mgr = dbc_config::instance();
		string result;
		if (!config_mgr.md5sum(result, false))
			throw bad_msg(__FILE__, __LINE__, 0, "ERROR: Can't generate search md5sum");

		ofs << "const char * get_search_md5sum()\n"
			<< "{\n"
			<< "\treturn \"" << result << "\";\n"
			<< "}\n\n";

		for (map<dbc_index_key_t, dbc_func_t>::const_iterator iter = func_map.begin(); iter != func_map.end(); ++iter) {
			gen_search(*iter, ofs, (i == 0), false);
			gen_search(*iter, ofs, (i == 0), true);
		}

		ofs << "}\n\n";
	}
}

void build_meta::gen_search(const func_value_type& fitem, ostream& os, bool is_dbc, bool is_array)
{
	int i;
	string func_name;
	string struct_name;
	string macro_name;
	const dbc_data_t& dbc_data = find_dbc_data(fitem.first.table_id);
	string table_name = dbc_data.table_name;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("is_dbc={1}, is_array={2}") % is_dbc % is_array).str(SGLOCALE), NULL);
#endif

	to_struct(table_name, struct_name);
	to_function(table_name, fitem.first.index_id, func_name, is_array);
	to_macro(table_name, macro_name);

	gen_search_header(fitem, os, is_array);

	// 函数体定义
	os << "\n{\n";

	if (is_dbc)
		os << "\tdbc_api *api_mgr = reinterpret_cast<dbc_api *>(instance_mgr);\n\n";
	else
		os << "\tsdc_api *api_mgr = reinterpret_cast<sdc_api *>(instance_mgr);\n\n";

	os << "#if defined(DEBUG)\n"
		<< "\tcout << \"" << func_name << ":\" << endl;\n";

	// 输入字段
	for (i = 0; i < fitem.second.field_size; i++) {
		const dbc_func_field_t& field = fitem.second.fields[i];

		int param_mode = get_param_mode(fitem.first, field);
		if (param_mode == PARAM_MODE_IN)
			os << "\tcout << \"" << field.field_name << " = [\" << " << field.field_name << " << \"]\" << endl;\n";
	}
	os << "\tcout << std::flush;\n";

	// 变量定义
	os << "#endif\n\n"
		<< "\tperf_stat stat(__FUNCTION__);\n"
		<< "\t" << struct_name << " key;\n";
	if (!is_array)
		os << "\t" << struct_name << " result;\n\n";

	// 赋值操作
	for (i = 0; i < fitem.second.field_size; i++) {
		const dbc_func_field_t& field = fitem.second.fields[i];

		int param_mode = get_param_mode(fitem.first, field);
		if (param_mode == PARAM_MODE_IN) {
			bool ref_flag = has_ref(fitem.second, field.field_name);
			const dbc_data_field_t *data_field = find_data_field(dbc_data, field.field_name);

			switch (data_field->field_type) {
			case SQLTYPE_CHAR:
			case SQLTYPE_UCHAR:
				os << "\tkey." << data_field->field_name << " = " << data_field->field_name;
				if (ref_flag)
					os << "[0];\n";
				else
					os << ";\n";
				break;
			case SQLTYPE_SHORT:
			case SQLTYPE_USHORT:
			case SQLTYPE_INT:
			case SQLTYPE_UINT:
			case SQLTYPE_LONG:
			case SQLTYPE_ULONG:
			case SQLTYPE_FLOAT:
			case SQLTYPE_DOUBLE:
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				os << "\tkey." << data_field->field_name << " = " << data_field->field_name;
				if (ref_flag)
					os << "[0];\n";
				else
					os << ";\n";
				break;
			case SQLTYPE_STRING:
			case SQLTYPE_VSTRING:
				os << "\tif (" << data_field->field_name << " == NULL) {\n"
					<< "\t\tkey." << data_field->field_name << "[0] = '\\0';\n"
					<< "\t} else {\n"
					<< "\t\tmemcpy(key." << data_field->field_name << ", "
					<< data_field->field_name << ", sizeof(key." << data_field->field_name << ") - 1);\n"
					<< "\t\tkey." << data_field->field_name << "[sizeof(key." << data_field->field_name << ") - 1] = '\\0';\n"
					<< "\t}\n\n";
				break;
			case SQLTYPE_UNKNOWN:
			case SQLTYPE_ANY:
			default:
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: table {1}: Unknown field_type {2}") % dbc_data.table_name % data_field->field_type).str(SGLOCALE));
			}
		}
	}

	// 多行result 处理
	if (is_array){
		os << "\t" << struct_name <<" *results = NULL;\n"
			<< "\tboost::scoped_array<" << struct_name << "> result_scoped;\n"
			<< "\t" <<struct_name << " result_array[256];\n\n"
			<< "\tif (max_rows <= 256) {\n"
			<< "\t\tresults = result_array;\n"
			<< "\t} else{ \n"
			<< "\t\tresults = new " << struct_name << "[max_rows];\n"
			<< "\t\tresult_scoped.reset(results);\n"
			<< "\t}\n\n"
			<< "\tlong retval = api_mgr->find(" << macro_name << ", " << fitem.first.index_id << ", &key, results, max_rows);\n";
	} else {
		os << "\tlong retval = api_mgr->find(" << macro_name << ", " << fitem.first.index_id << ", &key, &result);\n";
	}

	os << "\tif (retval < 1) {\n"
		<< "\t\tif (retval < 0)\n"
		<< "\t\t\tapi_mgr->set_ur_code(" << macro_name << ");\n\n";

	if (is_array)
		os << "\t\treturn retval;\n";
	else
		os<< "\t\treturn " << macro_name << " * 1000 + ERROR_OFFSET;\n";

	os << "\t}\n\n";

	bool has_output = false;
	for (i = 0; i < fitem.second.field_size; i++) {
		const dbc_func_field_t& field = fitem.second.fields[i];

		int param_mode = get_param_mode(fitem.first, field);
		if (param_mode != PARAM_MODE_IN) {
			has_output = true;
			break;
		}
	}

	if (has_output) {
		string intab2 = "\t";
		string intab3 = "\t\t";
		if (is_array) {
			intab2 += "\t";
			intab3 += "\t";

			os << "\tfor (int i = 0; i < retval; i++) {\n"
				<< "\t\t" << struct_name << "& result = results[i];\n\n";
		}

		// 赋值操作
		for (i = 0; i < fitem.second.field_size; i++) {
			const dbc_func_field_t& field = fitem.second.fields[i];

			int param_mode = get_param_mode(fitem.first, field);
			if (param_mode == PARAM_MODE_IN)
				continue;

			string item_name;
			if (field.field_ref.empty())
				item_name = field.field_name;
			else
				item_name = field.field_ref;

			const dbc_data_field_t *data_field = find_data_field(dbc_data, field.field_name);
			switch (data_field->field_type) {
			case SQLTYPE_CHAR:
			case SQLTYPE_UCHAR:
				os << intab2 << "if (" << item_name << " != NULL) {\n"
					<< intab3 << item_name << "[0] = result." << data_field->field_name << ";\n"
					<< intab3 << item_name << "[1] = '\\0';\n";
				if (is_array)
					os << intab3 << item_name << " += __" << item_name << "_w;\n";
				os << intab2 << "}\n";
				break;
			case SQLTYPE_SHORT:
			case SQLTYPE_USHORT:
			case SQLTYPE_INT:
			case SQLTYPE_UINT:
			case SQLTYPE_LONG:
			case SQLTYPE_ULONG:
			case SQLTYPE_FLOAT:
			case SQLTYPE_DOUBLE:
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				os << intab2 << "if (" << item_name << " != NULL)";
				if (is_array)
					os << " {\n";
				else
					os << "\n";
				os << intab3 << "*" << item_name << " = result." << data_field->field_name << ";\n";
				if (is_array) {
					os<<intab3<<item_name<<"++;\n";
					os << intab2 << "}\n";
				}
				break;
			case SQLTYPE_STRING:
			case SQLTYPE_VSTRING:
				os << intab2 << "if (" << item_name << " != NULL)";
				if (is_array)
					os << " {\n";
				else
					os << "\n";
				os << intab3 << "strcpy(" << item_name << ", result." << data_field->field_name << ");\n";
				if (is_array) {
					os << intab3 << item_name << " += __" << item_name << "_w;\n";
					os << intab2 << "}\n";
				}
				break;
			case SQLTYPE_UNKNOWN:
			case SQLTYPE_ANY:
			default:
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: table {1}: Unknown field_type {2}") % dbc_data.table_name % data_field->field_type).str(SGLOCALE));
			}
		}

		//调试信息
		os << "\n#if defined(DEBUG)\n";
		for (i = 0; i < fitem.second.field_size; i++) {
			const dbc_func_field_t& field = fitem.second.fields[i];

			int param_mode = get_param_mode(fitem.first, field);
			if (param_mode != PARAM_MODE_IN) {
				string item_name;
				if (field.field_ref.empty())
					item_name = field.field_name;
				else
					item_name = field.field_ref;

				const dbc_data_field_t *data_field = find_data_field(dbc_data, field.field_name);

				os << intab2 << "if (" << item_name << " != NULL)\n";
				if (data_field->field_type != SQLTYPE_CHAR
					&& data_field->field_type != SQLTYPE_UCHAR
					&& data_field->field_type != SQLTYPE_STRING
					&& data_field->field_type != SQLTYPE_VSTRING){
					if (is_array)
						os << intab3 << "cout << \"" << item_name << " = [\" << *(" << item_name+" - 1" << ") << \"]\" << endl;\n";
					else
						os << intab3 << "cout << \"" << item_name << " = [\" << *" << item_name << " << \"]\" << endl;\n";
				} else {
					if (is_array)
						os << intab3 << "cout << \"" << item_name << " = [\" << (" << item_name+" - __"+item_name+"_w"<< ") << \"]\" << endl;\n";
					else
						os << intab3 << "cout << \"" << item_name << " = [\" << " << item_name << " << \"]\" << endl;\n";
				}
			}
		}

		os << intab2 << "cout << std::flush;\n"
			<<"#endif\n";

		if (is_array)
			os << "\t}\n";
	}

	if (is_array)
		os << "\n\treturn retval;\n";
	else
		os << "\n\treturn 0;\n";
	os << "}\n\n";
}

void build_meta::gen_search_headers()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	string filename = sgdir + "/include/search_h.inl";
	ofstream ofs(filename.c_str());
	if (!ofs)
		throw bad_msg(__FILE__, __LINE__, 40, (_("ERROR: Can't open file {1}") % filename).str(SGLOCALE));

	ofs << "#if !defined(__SEARCH_H_INL__)\n"
		<< "#define __SEARCH_H_INL__\n\n"
		<< "#if defined(__cplusplus)\n"
		<< "extern \"C\"\n"
		<< "{\n"
		<< "#endif\n\n";

	for (map<dbc_index_key_t, dbc_func_t>::const_iterator iter = func_map.begin(); iter != func_map.end(); ++iter) {
		gen_search_header(*iter, ofs, false);
		ofs << ";\n";
		gen_search_header(*iter, ofs, true);
		ofs << ";\n";
	}

	ofs << "\n#if defined(__cplusplus)\n"
		<< "}\n"
		<< "#endif\n\n"
		<< "#endif\n\n";
}

void build_meta::gen_search_header(const func_value_type& fitem, ostream& os, bool is_array)
{
	int i;
	int count = 0;
	string func_name;
	const dbc_data_t& dbc_data = find_dbc_data(fitem.first.table_id);
	string table_name = dbc_data.table_name;
	string str;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("is_array={1}") % is_array).str(SGLOCALE), NULL);
#endif

	to_function(table_name, fitem.first.index_id, func_name, is_array);

	os << "int " << func_name << "(void *instance_mgr, ";

	// 变量定义
	for (i = 0; i < fitem.second.field_size; i++) {
		const dbc_func_field_t& field = fitem.second.fields[i];
		int param_mode = get_param_mode(fitem.first, field);

		if (param_mode == PARAM_MODE_OUT) {
			const dbc_data_field_t *data_field = find_data_field(dbc_data, field.field_name);

			if (count > 0)
				os << ", ";
			if (count % 3 == 0 && count > 0)
				os << "\n\t";
			gen_format(table_name, *data_field, str, false);
			os << str << " *" << field.field_name;
			++count;

			//为字符串类型增加宽度
			if (is_array
				&& (data_field->field_type == SQLTYPE_CHAR
					|| data_field->field_type == SQLTYPE_UCHAR
					|| data_field->field_type == SQLTYPE_STRING
					|| data_field->field_type == SQLTYPE_VSTRING)) {
				if (count > 0)
					os << ", ";
				if (count % 3 == 0 && count > 0)
					os << "\n\t";
				os << "int " << "__" << field.field_name << "_w";
				++count;
			}
		} else if (param_mode == PARAM_MODE_IN) {
			const dbc_data_field_t *data_field = find_data_field(dbc_data, field.field_name);

			bool ref_flag = has_ref(fitem.second, field.field_name);
			if (count > 0)
				os << ", ";
			if (count % 3 == 0 && count > 0)
				os << "\n\t";
			++count;

			if (!ref_flag && (data_field->field_type == SQLTYPE_STRING
				|| data_field->field_type == SQLTYPE_VSTRING))
				os << "const ";

			gen_format(table_name, *data_field, str, false);
			os << str << " ";

			if (ref_flag || data_field->field_type == SQLTYPE_STRING
				|| data_field->field_type == SQLTYPE_VSTRING)
				os << "*";

			os << field.field_name;

			//为字符串类型增加宽度
			if (is_array && ref_flag
				&& (data_field->field_type == SQLTYPE_CHAR
					|| data_field->field_type == SQLTYPE_UCHAR
					|| data_field->field_type == SQLTYPE_STRING
					|| data_field->field_type == SQLTYPE_VSTRING)) {
				if (count > 0)
					os << ", ";
				if (count % 3 == 0 && count > 0)
					os << "\n\t";
				os << "int " << "__" << field.field_name << "_w";
				++count;
			}
		}
	}

	if (is_array) {
		if (count > 0)
			os << ", ";
		if (count % 3 == 0 && count > 0)
			os << "\n\t";
		os << "long max_rows";
	}

	os << ")";
}

void build_meta::gen_search_defs()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	string filename = sgdir + "/include/search_def.inl";
	ofstream ofs(filename.c_str());
	if (!ofs)
		throw bad_msg(__FILE__, __LINE__, 40, (_("ERROR: Can't open file {1}") % filename).str(SGLOCALE));

	ofs << "#if !defined(__SEARCH_DEF_INL__)\n"
		<< "#define __SEARCH_DEF_INL__\n\n";

	for (map<dbc_index_key_t, dbc_func_t>::const_iterator iter = func_map.begin(); iter != func_map.end(); ++iter) {
		gen_search_def(*iter, ofs, false);
		gen_search_def(*iter, ofs, true);
	}

	ofs << "\n#endif\n";
}

void build_meta::gen_search_def(const func_value_type& fitem, ostream& os,bool is_array)
{
	int count = 0;
	string func_name;
	ostringstream arg_fmt;
	const dbc_data_t& dbc_data = find_dbc_data(fitem.first.table_id);
	string table_name = dbc_data.table_name;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("is_array={1}") % is_array).str(SGLOCALE), NULL);
#endif

	to_function(table_name, fitem.first.index_id, func_name, is_array);

	// 变量列表
	for (int i = 0; i < fitem.second.field_size; i++) {
		const dbc_func_field_t& field = fitem.second.fields[i];

		int param_mode = get_param_mode(fitem.first, field);
		if (param_mode == PARAM_MODE_INOUT)
			continue;

		if (count > 0)
			arg_fmt << ", ";
		if (count % 5 == 0 && count > 0)
			arg_fmt << "\\\n\t";

		arg_fmt << field.field_name;
		++count;

		if (is_array) {
			bool require_width = (param_mode == PARAM_MODE_OUT) || has_ref(fitem.second, field.field_name);
			const dbc_data_field_t *data_field = find_data_field(dbc_data, field.field_name);

			if (require_width &&
				(data_field->field_type == SQLTYPE_CHAR
					|| data_field->field_type == SQLTYPE_UCHAR
					|| data_field->field_type == SQLTYPE_STRING
					|| data_field->field_type == SQLTYPE_VSTRING)) {
				if (count > 0)
					arg_fmt << ", ";
				if (count % 5 == 0 && count > 0)
					arg_fmt << "\\\n\t";
				arg_fmt << "__" << field.field_name << "_w";
				++count;
			}
		}
	}

	if (is_array) {
		if (count > 0)
			arg_fmt << ", ";
		if (count % 5 == 0 && count > 0)
			arg_fmt << "\\\n\t";
		arg_fmt << "max_rows";
	}

	string arg_list = arg_fmt.str();

	os << "#define " << func_name << "("
		<< arg_list
		<< ") \\\n"
		<< "\t" << func_name << "(instance_mgr, "
		<< arg_list
		<< ")\n";
}

void build_meta::gen_exp()
{
	string rstr;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	string filename = sgdir + "/etc/so.exp";
	ofstream ofs(filename.c_str());
	if (!ofs)
		throw bad_msg(__FILE__, __LINE__, 40, (_("ERROR: Can't open file {1}") % filename).str(SGLOCALE));

	filename = sgdir + "/etc/so_static.exp";
	ifstream ifs(filename.c_str());
	if (!ifs)
		throw bad_msg(__FILE__, __LINE__, 40, (_("ERROR: Can't open file {1}") % filename).str(SGLOCALE));

	ofs << "#!\n";
	while (1) {
		getline(ifs, rstr);

		if (ifs.eof())
			break;
		if (rstr.empty())
			continue;

		ofs << rstr << "\n";
	}

	for (map<dbc_index_key_t, dbc_func_t>::const_iterator iter = func_map.begin(); iter != func_map.end(); ++iter) {
		string func_name;
		const dbc_data_t& dbc_data = find_dbc_data(iter->first.table_id);
		to_function(dbc_data.table_name, iter->first.index_id, func_name, false);
		ofs << func_name << "\n";
		to_function(dbc_data.table_name, iter->first.index_id, func_name, true);
		ofs << func_name << "\n";
	}
}

void build_meta::to_struct(const string& table_name, string& struct_name)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_name={1}") % table_name).str(SGLOCALE), NULL);
#endif

	if (table_name.compare(0, 2, "v_") == 0)
		struct_name = "t_" + table_name.substr(2);
	else
		struct_name = "t_" + table_name;
}

void build_meta::to_function(const string& table_name, int index, string& func_name, bool is_array)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_name={1}, index={2}, func_name={3}, is_array={4}") % table_name % index % func_name % is_array).str(SGLOCALE), NULL);
#endif

	string f_head;
	if (is_array)
		f_head = "gets_";
	else
		f_head = "get_";

	if (table_name.compare(0, 2, "v_") == 0)
		func_name = f_head + table_name.substr(2);
	else
		func_name = f_head + table_name;

	if (index > 0) {
		ostringstream msg;
		msg << "_" << index;
		func_name += msg.str();
	}
}

void build_meta::to_macro(const string& table_name, string& macro_name)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_name={1}, macro_name={2}") % table_name % macro_name).str(SGLOCALE), NULL);
#endif

	to_struct(table_name, macro_name);
	std::transform(macro_name.begin(), macro_name.end(), macro_name.begin(), toupper_predicate());
}

void build_meta::gen_equal_func(const string& struct_name, const dbc_data_t& dbc_data, ostream& os, bool internal)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("struct_name={1}") % struct_name).str(SGLOCALE), NULL);
#endif

	os << "long " << struct_name << "::equal(const void *key, const void *data)\n"
		<< "{\n";

	int i;
	bool has_primary = false;
	bool require_nret = false;
	for (i = 0; i < dbc_data.field_size; i++) {
		const dbc_data_field_t& field = dbc_data.fields[i];

		if (field.is_primary) {
			has_primary = true;

			if (field.field_type == SQLTYPE_STRING || field.field_type == SQLTYPE_VSTRING)
				require_nret = true;
		}
	}

	if (has_primary) {
		os << "\tconst " << struct_name << " *keyp = reinterpret_cast<const " << struct_name << " *>(key);\n"
			<< "\tconst " << struct_name << " *datap = reinterpret_cast<const " << struct_name << " *>(data);\n\n";
	}

	if (require_nret)
		os << "\tlong nret;\n";

	for (i = 0; i < dbc_data.field_size; i++) {
		const dbc_data_field_t& field = dbc_data.fields[i];

		if (!field.is_primary)
			continue;

		if (i > 0)
			os << "\n";

		switch (field.field_type) {
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR:
		case SQLTYPE_SHORT:
		case SQLTYPE_USHORT:
		case SQLTYPE_INT:
		case SQLTYPE_UINT:
		case SQLTYPE_LONG:
		case SQLTYPE_ULONG:
		case SQLTYPE_FLOAT:
		case SQLTYPE_DOUBLE:
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			os << "\tif (keyp->" << field.field_name << " < datap->" << field.field_name << ")\n"
				<< "\t\treturn -1;\n"
				<< "\telse if (keyp->" << field.field_name << " > datap->" << field.field_name << ")\n"
				<< "\t\treturn 1;\n";
			break;
		case SQLTYPE_STRING:
			os << "\tnret = strcmp(keyp->" << field.field_name << ", datap->" << field.field_name << ");\n"
				<< "\tif (nret != 0)\n"
				<< "\t\treturn nret;\n";
			break;
		case SQLTYPE_VSTRING:
			if (!internal) {
				os << "\tnret = strcmp(keyp->" << field.field_name << ", datap->" << field.field_name << ");\n"
					<< "\tif (nret != 0)\n"
					<< "\t\treturn nret;\n";
			} else {
				os << "\tnret = strcmp(keyp->placeholder + keyp->" << field.field_name << ", datap->placeholder + datap->" << field.field_name << ");\n"
					<< "\tif (nret != 0)\n"
					<< "\t\treturn nret;\n";
			}
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: table {1}: Unknown field_type {2}") % dbc_data.table_name % field.field_type).str(SGLOCALE));
		}
	}
	os << "\n\treturn 0;\n"
		<< "}\n\n";
}

void build_meta::gen_partition_func(const string& struct_name, const dbc_data_t& dbc_data, ostream& os, bool internal)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("struct_name={1}") % struct_name).str(SGLOCALE), NULL);
#endif

	os << "void " << struct_name << "::get_partition(char *key, const void *data)\n"
 		<< "{\n";

	string format_str = "";
	string param_str = "";

	for (int i = 0; i < dbc_data.field_size; i++) {
		const dbc_data_field_t& field = dbc_data.fields[i];

		if (!field.is_partition)
			continue;

		switch (field.field_type) {
		case SQLTYPE_CHAR:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%c";
			break;
		case SQLTYPE_UCHAR:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%uc";
			break;
		case SQLTYPE_SHORT:
		case SQLTYPE_INT:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%d";
			break;
		case SQLTYPE_USHORT:
		case SQLTYPE_UINT:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%ud";
			break;
		case SQLTYPE_LONG:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%ld";
			break;
		case SQLTYPE_ULONG:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%uld";
			break;
		case SQLTYPE_FLOAT:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%f";
			break;
		case SQLTYPE_DOUBLE:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%lf";
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%s";
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			if (field.partition_format[0] != '\0')
				format_str += field.partition_format;
			else
				format_str += "%ld";
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: table {1}: Unknown field_type {2}") % dbc_data.table_name % field.field_type).str(SGLOCALE));
		}

		if (!param_str.empty())
			param_str += ", ";
		if (internal && field.field_type == SQLTYPE_VSTRING)
			param_str += "datap->placeholder + ";
		param_str += "datap->";
		param_str += field.field_name;
	}

	if (format_str.empty()) {
		os << "\tkey[0] = \'\\0\';\n";
	} else {
 		os <<"\tconst " << struct_name << " *datap = reinterpret_cast<const " << struct_name << " *>(data);\n\n"
			<< "\tsprintf(key, \"" << format_str << "\", " << param_str << ");\n";
	}

	os << "}\n\n";
}

void build_meta::gen_print_func(const string& struct_name, const dbc_data_t& dbc_data, ostream& os, bool internal)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("struct_name={1}") % struct_name).str(SGLOCALE), NULL);
#endif

	os << "void " << struct_name << "::print(const void *data)\n"
		<< "{\n"
		<< "\tconst " << struct_name << " *datap = reinterpret_cast<const " << struct_name << " *>(data);\n\n";
	os << "\tcout << \"" << struct_name << ":\\n\";\n";
	for (int i = 0; i < dbc_data.field_size; i++) {
		const dbc_data_field_t& field = dbc_data.fields[i];
		if (internal && field.field_type == SQLTYPE_VSTRING)
			os << "\tcout << \"" << field.field_name << " = [\" << datap->placeholder + datap->" << field.field_name << " << \"]\\n\";\n";
		else
			os << "\tcout << \"" << field.field_name << " = [\" << datap->" << field.field_name << " << \"]\\n\";\n";
	}

	os << "\tcout << std::flush;\n"
		<< "}\n\n";
}

void build_meta::gen_hash_func(const string& struct_name, const dbc_data_t& dbc_data, const index_value_type& iitem, ostream& os, bool internal)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("struct_name={1}") % struct_name).str(SGLOCALE), NULL);
#endif
	const dbc_index_t& dbc_index = iitem.second;

	os << "void " << struct_name << "::get_hash" << iitem.first.index_id << "(char *key, const void *data)\n"
 		<< "{\n";

	if (!(dbc_index.method_type & METHOD_TYPE_HASH)) {
		os << "\tkey[0] = '\\0';\n"
			<< "}\n\n";
	} else {
		os <<"\tconst " << struct_name << " *datap = reinterpret_cast<const " << struct_name << " *>(data);\n\n";

		string format_str = "";
		string param_str = "";

		for (int i = 0; i < dbc_index.field_size; i++) {
			const dbc_index_field_t& field = dbc_index.fields[i];

			if (!field.is_hash)
				continue;

			const dbc_data_field_t *data_field = find_data_field(dbc_data, field.field_name);

			switch (data_field->field_type) {
			case SQLTYPE_CHAR:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%c";
				break;
			case SQLTYPE_UCHAR:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%uc";
				break;
			case SQLTYPE_SHORT:
			case SQLTYPE_INT:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%d";
				break;
			case SQLTYPE_USHORT:
			case SQLTYPE_UINT:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%ud";
				break;
			case SQLTYPE_LONG:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%ld";
				break;
			case SQLTYPE_ULONG:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%uld";
				break;
			case SQLTYPE_FLOAT:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%f";
				break;
			case SQLTYPE_DOUBLE:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%lf";
				break;
			case SQLTYPE_STRING:
			case SQLTYPE_VSTRING:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%s";
				break;
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				if (field.hash_format[0] != '\0')
					format_str += field.hash_format;
				else
					format_str += "%ld";
				break;
			case SQLTYPE_UNKNOWN:
			case SQLTYPE_ANY:
			default:
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: index {1}: Unknown field_type {2}") % dbc_index.index_name % data_field->field_type).str(SGLOCALE));
			}

			if (!param_str.empty())
				param_str += ", ";
			if (internal && data_field->field_type == SQLTYPE_VSTRING)
				param_str += "datap->placeholder + ";
			param_str += "datap->";
			param_str += field.field_name;
		}

		if (format_str.empty())
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: index {1}: method_type is hash, but not given hash key.") % dbc_index.index_name).str(SGLOCALE));

		os << "\tsprintf(key, \"" << format_str << "\", " << param_str << ");\n"
			<< "}\n\n";
	}
}

void build_meta::gen_compare_func(const string& struct_name, const dbc_data_t& dbc_data, const index_value_type& iitem, ostream& os, bool exact, bool internal)
{
	int i;
	const dbc_index_t& dbc_index = iitem.second;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("struct_name={1}") % struct_name).str(SGLOCALE), NULL);
#endif

	os << "long " << struct_name << "::compare";
	if (exact)
		os << "_exact";
	os << iitem.first.index_id << "(const void *key, const void *data)\n"
		<< "{\n"
		<< "\tconst " << struct_name << " *keyp = reinterpret_cast<const " << struct_name << " *>(key);\n"
		<< "\tconst " << struct_name << " *datap = reinterpret_cast<const " << struct_name << " *>(data);\n";

	bool require_nret = false;
	for (i = 0; i < dbc_index.field_size && !require_nret; i++) {
		const dbc_index_field_t& field = dbc_index.fields[i];

		if ((!exact && field.special_type > 0) || field.search_type == 0)
			continue;

		const dbc_data_field_t *data_field = find_data_field(dbc_data, field.field_name);

		switch (data_field->field_type) {
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR:
			if (!(field.search_type & SEARCH_TYPE_WILDCARD))
				require_nret = true;
			break;
		case SQLTYPE_SHORT:
		case SQLTYPE_USHORT:
		case SQLTYPE_INT:
		case SQLTYPE_UINT:
		case SQLTYPE_LONG:
		case SQLTYPE_ULONG:
		case SQLTYPE_FLOAT:
		case SQLTYPE_DOUBLE:
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			require_nret = true;
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			if (field.search_type & (SEARCH_TYPE_EQUAL | SEARCH_TYPE_LESS | SEARCH_TYPE_MORE))
				require_nret = true;
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Unknown field_type {1}") % data_field->field_type).str(SGLOCALE));
		}
	}

	if (require_nret)
		os << "\tlong nret;\n";

	os << "\n#if defined(DEBUG_COMPARE)\n"
		<< "\tprint(data);\n"
		<< "#endif\n";

	for (i = 0; i < dbc_index.field_size; i++) {
		const dbc_index_field_t& field = dbc_index.fields[i];

		if (!exact && field.special_type > 0) {
			gen_special(dbc_index, field.special_type, field.field_name, os);
			continue;
		} else if (field.search_type == 0) {
			continue;
		}

 		const dbc_data_field_t *data_field = find_data_field(dbc_data, field.field_name);

		os << "\n";

		if (field.range_group == 0) { // 非范围字段
			gen_compare_part(data_field->field_type, field.search_type, dbc_index.index_name,
				field.field_name, field.field_name, os, false, internal);
		} else { // 范围字段
			string replaced_field;
			// 只有INDEX_TYPE_RANGE索引，或者结束字段，才需要使用replaced_field
			if ((dbc_index.index_type & INDEX_TYPE_RANGE) || field.range_group > 0) {
				for (int j = 0; j < dbc_index.field_size; j++) {
					const dbc_index_field_t& field2 = dbc_index.fields[j];

					if (field2.range_group == -field.range_group) {
						replaced_field = field2.field_name;
						break;
					}
				}

				if (replaced_field.empty())
					throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find field_name {1}, range_group = {2}, index_name = {3}") % field.field_name % field.range_group % dbc_index.index_name).str(SGLOCALE));
			} else {
				replaced_field = field.field_name;
			}

			if (!(dbc_index.index_type & INDEX_TYPE_RANGE)) {
				gen_compare_part(data_field->field_type, field.search_type, dbc_index.index_name,
					replaced_field, field.field_name, os, false, internal);
			} else {
				string field_name1;

				fmt.str("");
				fmt << field.field_name;
				if (field.range_offset) {
					if (field.range_group < 0)
						fmt << " + ";
					else
						fmt << " - ";
					fmt << field.range_offset;
				}
				field_name1 = fmt.str();
				gen_compare_part(data_field->field_type, field.search_type, dbc_index.index_name,
					field_name1, field.field_name, os, false, internal);

				os << "\n";

				fmt.str("");
				fmt << field.field_name;
				if (field.range_offset) {
					if (field.range_group < 0)
						fmt << " - ";
					else
						fmt << " + ";
					fmt << field.range_offset;
				}
				field_name1 = fmt.str();
				gen_compare_part(data_field->field_type, field.search_type, dbc_index.index_name,
					field_name1, replaced_field, os, true, internal);
			}
 		}
 	}

	os << "\n\treturn 0;\n"
		<< "}\n\n";
}

void build_meta::gen_special(const dbc_index_t& dbc_index, int special_type, const string& field_name, ostream& os)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("special_type={1}, field_name={2}") % special_type % field_name).str(SGLOCALE), NULL);
#endif

	switch (special_type) {
	case SPECIAL_TYPE_1:
		if (field_name != "is_prefix")
			throw bad_param(__FILE__, __LINE__, 213, (_("ERROR: index {1}: field_name must be is_prefix.") % dbc_index.index_name).str(SGLOCALE));

		os << "\tif (datap->is_prefix == 0) {\n"
			<< "\t\tif (strlen(keyp->lower_special_number) != strlen(datap->lower_special_number))\n"
			<< "\t\t\treturn 1;\n"
			<< "\t}\n\n";
		break;
	case SPECIAL_TYPE_2:
		if (field_name == "begin_step" || field_name == "begin_free") {
			os << "\tfor (int i = 0 ; i < strlen(" << field_name << "); i ++) {\n"
				<< "\t\t	if (datap->" << field_name << "[i] != '?') {\n"
				<< "\t\t\tnret = keyp->" << field_name << "[i] - datap->" << field_name << "[i];\n"
				<< "\t\t\tif (nret != 0)\n"
				<< "\t\t\t\treturn nret;\n"
				<< "\t\t}\n"
				<< "\t}\n\n";
		} else if (field_name == "end_step" || field_name == "end_free") {
			os << "\tfor (int i = 0 ; i < strlen(" << field_name << "); i ++) {\n"
				<< "\t\tif (datap->" << field_name << "[i] != '?') {\n"
				<< "\t\t\tnret = keyp->begin" << field_name.substr(3) << "[i] - datap->" << field_name << "[i];\n"
				<< "\t\t\tif (nret != 0)\n"
				<< "\t\t\t\treturn nret;\n"
				<< "\t\t}\n"
				<< "\t}\n\n";
		} else {
			throw bad_param(__FILE__, __LINE__, 214, (_("ERROR: index {1}: field_name must be begin_step, end_step, begin_free or end_free.") % dbc_index.index_name ).str(SGLOCALE));
		}
		break;
	case SPECIAL_TYPE_3:
		if (field_name  == "special_number") {
			os << "\t\tint number_len = strlen(datap->special_number);\n"
				<< "\t\tswitch (match_type) {\n"
				<< "\t\tcase PREFIX_MATCH:\n"
				<< "\t\t\tnret = memcmp(keyp->special_number, datap->special_number, number_len);\n"
				<< "\t\t\tif (nret != 0)\n"
				<< "\t\t\t\treturn nret;\n"
				<< "\t\t\tbreak;\n"
				<< "\t\tcase FULL_MATCH:\n"
				<< "\t\t\tnret = strcmp(keyp->special_number, datap->special_number);\n"
				<< "\t\t\tif (nret != 0)\n"
				<< "\t\t\t\treturn nret;\n"
				<< "\t\t\tbreak;\n"
				<< "\t\tdefault:\n"
				<< "\t\t\treturn -1;\n"
				<< "\t\t}\n\n";
		} else if (field_name == "match_type") {
			// do nothing
		} else {
			throw bad_param(__FILE__, __LINE__, 215, (_("ERROR: index {1}: field_name must be special_number or match_type.") % dbc_index.index_name).str(SGLOCALE));
		}
		break;
	case SPECIAL_TYPE_4:
		if (field_name == "begin_weekday" || field_name == "end_weekday") {
			bool has_eff_date = false;
			bool has_exp_date = false;
			bool has_begin_weekday = false;
			bool has_end_weekday = false;

			for (int i = 0; i < dbc_index.field_size; i++) {
				const dbc_index_field_t& field = dbc_index.fields[i];

				if (strcmp(field.field_name, "eff_date") == 0)
					has_eff_date = true;
				else if (strcmp(field.field_name, "exp_date") == 0)
					has_exp_date = true;
				else if (strcmp(field.field_name, "begin_weekday") == 0)
					has_begin_weekday = true;
				else if (strcmp(field.field_name, "end_weekday") == 0)
					has_end_weekday = true;
			}

			if (!has_begin_weekday || !has_end_weekday)
				throw bad_param(__FILE__, __LINE__, 407, (_("ERROR: index {1}: index must have begin_weekday and end_weekday.") % dbc_index.index_name).str(SGLOCALE));

			if (has_eff_date && has_exp_date)
				os << "\n\t\tif ((keyp->exp_date - keyp->eff_date) < 86400 * 7) {\n";
			else
			    os << "\n\t\t{\n";

			os	<< "\t\tif (keyp->end_weekday >= keyp->begin_weekday) {\n"
				<< "\t\t\tif (keyp->begin_weekday > datap->end_weekday || keyp->end_weekday < datap->begin_weekday)\n"
				<< "\t\t\t\treturn -1;\n"
				<< "\t\t} else {\n"
				<< "\t\t\tif (keyp->end_weekday < datap->begin_weekday && keyp->begin_weekday > datap->end_weekday)\n"
				<< "\t\t\t\treturn -1;\n"
				<< "\t\t}\n"
				<< "\t}\n\n";
		} else {
			throw bad_param(__FILE__, __LINE__, 407, (_("ERROR: index {1}: field_name must be begin_weekday or end_weekday.") % dbc_index.index_name).str(SGLOCALE));
		}
	    break;
	case SPECIAL_TYPE_5:
		if (field_name == "begin_time" || field_name == "end_time") {
			bool has_eff_date = false;
			bool has_exp_date = false;
			bool has_begin_date = false;
			bool has_end_date = false;
			bool has_begin_time = false;
			bool has_end_time = false;

			for (int i = 0; i < dbc_index.field_size; i++) {
				const dbc_index_field_t& field = dbc_index.fields[i];

				if (strcmp(field.field_name, "eff_date") == 0)
					has_eff_date = true;
				else if (strcmp(field.field_name, "exp_date") == 0)
					has_exp_date = true;
				else if (strcmp(field.field_name, "begin_date") == 0)
					has_begin_date = true;
				else if (strcmp(field.field_name, "end_date") == 0)
					has_end_date = true;
				else if (strcmp(field.field_name, "begin_time") == 0)
					has_begin_time = true;
				else if (strcmp(field.field_name, "end_time") == 0)
					has_end_time = true;
			}

			if (has_begin_time == false || has_end_time == false)
				throw bad_param(__FILE__, __LINE__, 408, (_("ERROR: index {1}: field_name must be begin_time or end_time.") % dbc_index.index_name).str(SGLOCALE));

			if (has_eff_date && has_exp_date)
				os << "\n\tif ((keyp->exp_date - keyp->eff_date) < 86400) {\n";
			else if (has_begin_date && has_end_date)
				os << "\n\tif ((keyp->end_date - keyp->begin_date) < 86400) {\n";
			else
				os << "\n\t{\n";

			os << "\t\tif (keyp->end_time >= keyp->begin_time) {\n"
				<< "\t\t\tif (keyp->begin_time > datap->end_time || keyp->end_time < datap->begin_time)\n"
				<< "\t\t\t\treturn -1;\n"
				<< "\t\t} else {\n"
				<< "\t\t\tif (keyp->end_time < datap->begin_time && keyp->begin_time > datap->end_time)\n"
				<< "\t\t\t\treturn -1;\n"
				<< "\t\t}\n"
				<< "\t}\n\n";
		} else {
			throw bad_param(__FILE__, __LINE__, 408, (_("ERROR: index {1}: field_name must be begin_time or end_time.") % dbc_index.index_name).str(SGLOCALE));
		}
		break;
	case SPECIAL_TYPE_999:
		// do nothing
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 409, (_("ERROR: index {1}: special_type not defined.") % dbc_index.index_name).str(SGLOCALE));
	}
}

const dbc_data_t& build_meta::find_dbc_data(int table_id)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}") % table_id).str(SGLOCALE), NULL);
#endif

	map<int, dbc_data_t>::const_iterator iter = data_map.find(table_id);
	if (iter == data_map.end())
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find table name in table_id {1}") % table_id).str(SGLOCALE));

	return iter->second;
}

const dbc_data_field_t * build_meta::find_data_field(const dbc_data_t& dbc_data, const string& field_name)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("field_name={1}") % field_name).str(SGLOCALE), NULL);
#endif

	const dbc_data_field_t *data_field = NULL;
	for (int i = 0; i < dbc_data.field_size; i++) {
		if (field_name == dbc_data.fields[i].field_name) {
			data_field = dbc_data.fields + i;
			break;
		}
	}
	if (data_field == NULL)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find field_name {1} in {2}") % field_name % dbc_data.table_name).str(SGLOCALE));

	return data_field;
}

const dbc_index_field_t * build_meta::find_index_field(const dbc_index_t& dbc_index, const string& field_name)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("field_name={1}") % field_name).str(SGLOCALE), NULL);
#endif

	const dbc_index_field_t *index_field = NULL;
	for (int i = 0; i < dbc_index.field_size; i++) {
		if (field_name == dbc_index.fields[i].field_name) {
			index_field = dbc_index.fields + i;
			break;
		}
	}

	return index_field;
}

int build_meta::get_param_mode(const dbc_index_key_t& index_key, const dbc_func_field_t& field)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, "", NULL);
#endif

	map<dbc_index_key_t, dbc_index_t>::const_iterator index_iter = index_map.find(index_key);
	if (index_iter == index_map.end())
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find index info in table_id {1}, index_id {2}") % index_key.table_id % index_key.index_id).str(SGLOCALE));

	const dbc_index_field_t *index_field = find_index_field(index_iter->second, field.field_name);
	if (index_field) // 输入参数
		return PARAM_MODE_IN;
	else if (field.field_ref.empty())
		return PARAM_MODE_OUT;
	else
		return PARAM_MODE_INOUT;
}

bool build_meta::has_ref(const dbc_func_t& dbc_func, const string& field_name)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("field_name={1}") % field_name).str(SGLOCALE), NULL);
#endif

	bool ref_flag = false;
	for (int i = 0; i < dbc_func.field_size; i++) {
		const dbc_func_field_t& field = dbc_func.fields[i];
		if (field.field_ref == field_name) {
			ref_flag = true;
			break;
		}
	}

	return ref_flag;
}

void build_meta::gen_format(const string& table_name, const dbc_data_field_t& field, string& str, bool internal)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_name={1}") % table_name).str(SGLOCALE), NULL);
#endif

	switch (field.field_type) {
	case SQLTYPE_CHAR:
		str = "char";
		break;
	case SQLTYPE_UCHAR:
		str = "unsigned char";
		break;
	case SQLTYPE_SHORT:
		str = "short";
		break;
	case SQLTYPE_USHORT:
		str = "unsigned short";
		break;
	case SQLTYPE_INT:
		str = "int";
		break;
	case SQLTYPE_UINT:
		str = "unsigned int";
		break;
	case SQLTYPE_LONG:
		str = "long";
		break;
	case SQLTYPE_ULONG:
		str = "unsigned long";
		break;
	case SQLTYPE_FLOAT:
		str = "float";
		break;
	case SQLTYPE_DOUBLE:
		str = "double";
		break;
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		str = "time_t";
		break;
	case SQLTYPE_STRING:
		str = "char";
		break;
	case SQLTYPE_VSTRING:
		if (!internal)
			str = "char";
		else
			str = "int";
		break;
	case SQLTYPE_UNKNOWN:
	case SQLTYPE_ANY:
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: table {1}: Unknown field_type {2}") % table_name % field.field_type).str(SGLOCALE));
	}
}

/*
 * 参数列表说明
 * field_type: 比较的字段类型
 * search_type: 比较类型
 * index_name: 索引名称
 * field_name1: 比较字段名称1
 * field_name2: 比较字段名称2
 * os: 输出流
 * match: 满足条件就返回0
 */
void build_meta::gen_compare_part(sqltype_enum field_type, int search_type, const string& index_name,
	const string& field_name1, const string& field_name2, ostream& os, bool match, bool internal)
{
	string tabs_prefix;
	string oper_str;
	string retval_str;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("field_type={1}, search_type={2}, index_name={3}, field_name1={4}, field_name2={5}, match={6}") % field_type % search_type % index_name % field_name1 % field_name2 % match).str(SGLOCALE), NULL);
#endif

	if (field_type == SQLTYPE_VSTRING && !internal)
		field_type = SQLTYPE_STRING;

	switch (field_type) {
	case SQLTYPE_CHAR:
	case SQLTYPE_UCHAR:
		if (search_type & SEARCH_TYPE_WILDCARD) {
			tabs_prefix = "\t\t";
			os << "\tif (datap->" << field_name2 << " != '*') {\n";
		} else {
			tabs_prefix = "\t";
		}

		os << tabs_prefix << "nret = keyp->" << field_name1 << " - datap->" << field_name2 << ";\n"
			<< tabs_prefix;
		if (search_type & SEARCH_TYPE_EQUAL) {
			if (search_type & SEARCH_TYPE_LESS)
				os << "if (nret > 0)\n";
			else if (search_type & SEARCH_TYPE_MORE)
				os << "if (nret < 0)\n";
			else
				os << "if (nret != 0)\n";

			if (match)
				retval_str = "0";
			else
				retval_str = "nret";
		} else if  (search_type & SEARCH_TYPE_LESS) {
			os << "if (nret >= 0)\n";
			if (match)
				retval_str = "0";
			else
				retval_str = "nret + 1";
		} else if (search_type & SEARCH_TYPE_MORE) {
			os << "if (nret <= 0)\n";
			if (match)
				retval_str = "0";
			else
				retval_str = "nret - 1";
		} else {
			throw bad_param(__FILE__, __LINE__, 210, (_("ERROR: index {1}: search_type {2} not supported.") % index_name % search_type).str(SGLOCALE));
		}
		os << tabs_prefix << "\treturn " << retval_str << ";\n";

		if (search_type & SEARCH_TYPE_WILDCARD)
			os << "\t}\n";

		break;
	case SQLTYPE_SHORT:
	case SQLTYPE_USHORT:
	case SQLTYPE_INT:
	case SQLTYPE_UINT:
	case SQLTYPE_LONG:
	case SQLTYPE_ULONG:
	case SQLTYPE_FLOAT:
	case SQLTYPE_DOUBLE:
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		tabs_prefix = "\t";
		os << tabs_prefix << "nret = keyp->" << field_name1 << " - datap->" << field_name2 << ";\n"
			<< tabs_prefix;

		if (search_type & SEARCH_TYPE_EQUAL) {
			if (search_type & SEARCH_TYPE_LESS)
				os << "if (nret > 0)\n";
			else if (search_type & SEARCH_TYPE_MORE)
				os << "if (nret < 0)\n";
			else
				os << "if (nret != 0)\n";

			if (match)
				retval_str = "0";
			else
				retval_str = "nret";
		} else if  (search_type & SEARCH_TYPE_LESS) {
			os << "if (nret >= 0)\n";
			if (match)
				retval_str = "0";
			else
				retval_str = "nret + 1";
		} else if (search_type & SEARCH_TYPE_MORE) {
			os << "if (nret <= 0)\n";
			if (match)
				retval_str = "0";
			else
				retval_str = "nret - 1";
		} else {
			throw bad_param(__FILE__, __LINE__, 211, (_("ERROR: index {1}: search_type {2} not supported.") % index_name % search_type).str(SGLOCALE));
		}
		os << tabs_prefix << "\treturn " << retval_str << ";\n";

		break;
	case SQLTYPE_STRING:
		if (search_type & SEARCH_TYPE_WILDCARD) {
			tabs_prefix = "\t\t";
			os << "\tif (datap->" << field_name2 << "[0] != '*') {\n";
		} else {
			tabs_prefix = "\t";
		}

		if (search_type & SEARCH_TYPE_STRCHR) {
			os << tabs_prefix << "if (datap->" << field_name2 << "[0] !=  '!') {\n"
				<< tabs_prefix << "\tif (strchr(datap->" << field_name2 << ", keyp->" << field_name1 << "[0]) == NULL)\n"
				<< tabs_prefix << "\t\treturn -1;\n"
				<< tabs_prefix << "} else {\n"
				<< tabs_prefix << "\tif (strchr(datap->" << field_name2 << ", keyp->" << field_name1 << "[0]) != NULL)\n"
				<< tabs_prefix << "\t\treturn -1;\n"
				<< tabs_prefix << "}\n";
		} else if (search_type & SEARCH_TYPE_STRSTR) {
			os << tabs_prefix << "if (datap->" << field_name2 << "[0] !=  '!') {\n"
				<< tabs_prefix << "\tif (strstr(datap->" << field_name2 << ", keyp->" << field_name1 << ") == NULL)\n"
				<< tabs_prefix << "\t\treturn -1;\n"
				<< tabs_prefix << "} else {\n"
				<< tabs_prefix << "\tif (strstr(datap->" << field_name2 << ", keyp->" << field_name1 << ") != NULL)\n"
				<< tabs_prefix << "\t\treturn -1;\n"
				<< tabs_prefix << "}\n";
		} else if (search_type & SEARCH_TYPE_EQUAL) {
			if (search_type & SEARCH_TYPE_IGNORE_CASE)
				os << tabs_prefix << "nret = strcasecmp(keyp->" << field_name1 << ", datap->" << field_name2 << ");\n";
			else if (search_type & SEARCH_TYPE_MATCH_N)
				os << tabs_prefix << "nret = memcmp(keyp->" << field_name1 << ", datap->" << field_name2 << ", " << "strlen(datap->" << field_name2 << "));\n";
			else
				os << tabs_prefix << "nret = strcmp(keyp->" << field_name1 << ", datap->" << field_name2 << ");\n";

			if (search_type & SEARCH_TYPE_LESS)
				os << tabs_prefix << "if (nret > 0)\n";
			else if (search_type & SEARCH_TYPE_MORE)
				os << tabs_prefix << "if (nret < 0)\n";
			else
				os << tabs_prefix << "if (nret != 0)\n";

			if (match)
				retval_str = "0";
			else
				retval_str = "nret";
			os << tabs_prefix << "\treturn " << retval_str << ";\n";
		} else if  (search_type & SEARCH_TYPE_LESS) {
			if (search_type & SEARCH_TYPE_IGNORE_CASE)
				os << tabs_prefix << "nret = strcasecmp(keyp->" << field_name1 << ", datap->" << field_name2 << ");\n";
			else if (search_type & SEARCH_TYPE_MATCH_N)
				os << tabs_prefix << "nret = memcmp(keyp->" << field_name1 << ", datap->" << field_name2 << ", " << "strlen(datap->" << field_name2 << "));\n";
			else
				os << tabs_prefix << "nret = strcmp(keyp->" << field_name1 << ", datap->" << field_name2 << ");\n";
			os << tabs_prefix << "if (nret >= 0)\n";

			if (match)
				retval_str = "0";
			else
				retval_str = "nret + 1";
			os << tabs_prefix << "\treturn " << retval_str << ";\n";
		} else if (search_type & SEARCH_TYPE_MORE) {
			if (search_type & SEARCH_TYPE_IGNORE_CASE)
				os << tabs_prefix << "nret = strcasecmp(keyp->" << field_name1 << ", datap->" << field_name2 << ");\n";
			else if (search_type & SEARCH_TYPE_MATCH_N)
				os << tabs_prefix << "nret = memcmp(keyp->" << field_name1 << ", datap->" << field_name2 << ", " << "strlen(datap->" << field_name2 << "));\n";
			else
				os << tabs_prefix << "nret = strcmp(keyp->" << field_name1 << ", datap->" << field_name2 << ");\n";
			os << tabs_prefix << "if (nret <= 0)\n";

			if (match)
				retval_str = "0";
			else
				retval_str = "nret - 1";
			os << tabs_prefix << "\treturn " << retval_str << ";\n";
		} else {
			throw bad_param(__FILE__, __LINE__, 212, (_("ERROR: index {1}: search_type {2} not supported.") % index_name % search_type).str(SGLOCALE));
		}

		if (search_type & SEARCH_TYPE_WILDCARD)
			os << "\t}\n";
		break;
	case SQLTYPE_VSTRING:
		if (search_type & SEARCH_TYPE_WILDCARD) {
			tabs_prefix = "\t\t";
			os << "\tif (datap->placeholder[datap->" << field_name2 << "] != '*') {\n";
		} else {
			tabs_prefix = "\t";
		}

		if (search_type & SEARCH_TYPE_STRCHR) {
			os << tabs_prefix << "if (datap->placeholder[datap->" << field_name2 << "] !=  '!') {\n"
				<< tabs_prefix << "\tif (strchr(datap->placeholder + datap->" << field_name2 << ", keyp->placeholder[keyp->" << field_name1 << "]) == NULL)\n"
				<< tabs_prefix << "\t\treturn -1;\n"
				<< tabs_prefix << "} else {\n"
				<< tabs_prefix << "\tif (strchr(datap->placeholder + datap->" << field_name2 << ", keyp->placeholder[keyp->" << field_name1 << "]) != NULL)\n"
				<< tabs_prefix << "\t\treturn -1;\n"
				<< tabs_prefix << "}\n";
		} else if (search_type & SEARCH_TYPE_STRSTR) {
			os << tabs_prefix << "if (datap->placeholder[datap->" << field_name2 << "] !=  '!') {\n"
				<< tabs_prefix << "\tif (strstr(datap->placeholder + datap->" << field_name2 << ", keyp->placeholder + keyp->" << field_name1 << ") == NULL)\n"
				<< tabs_prefix << "\t\treturn -1;\n"
				<< tabs_prefix << "} else {\n"
				<< tabs_prefix << "\tif (strstr(datap->placeholder + datap->" << field_name2 << ", keyp->placeholder + keyp->" << field_name1 << ") != NULL)\n"
				<< tabs_prefix << "\t\treturn -1;\n"
				<< tabs_prefix << "}\n";
		} else if (search_type & SEARCH_TYPE_EQUAL) {
			if (search_type & SEARCH_TYPE_IGNORE_CASE)
				os << tabs_prefix << "nret = strcasecmp(keyp->placeholder + keyp->" << field_name1 << ", datap->placeholder + datap->" << field_name2 << ");\n";
			else if (search_type & SEARCH_TYPE_MATCH_N)
				os << tabs_prefix << "nret = memcmp(keyp->placeholder + keyp->" << field_name1 << ", datap->placeholder + datap->" << field_name2 << ", " << "strlen(datap->placeholder + datap->" << field_name2 << "));\n";
			else
				os << tabs_prefix << "nret = strcmp(keyp->placeholder + keyp->" << field_name1 << ", datap->placeholder + datap->" << field_name2 << ");\n";

			if (search_type & SEARCH_TYPE_LESS)
				os << tabs_prefix << "if (nret > 0)\n";
			else if (search_type & SEARCH_TYPE_MORE)
				os << tabs_prefix << "if (nret < 0)\n";
			else
				os << tabs_prefix << "if (nret != 0)\n";

			if (match)
				retval_str = "0";
			else
				retval_str = "nret";
			os << tabs_prefix << "\treturn " << retval_str << ";\n";
		} else if  (search_type & SEARCH_TYPE_LESS) {
			if (search_type & SEARCH_TYPE_IGNORE_CASE)
				os << tabs_prefix << "nret = strcasecmp(keyp->" << field_name1 << ", datap->placeholder + datap->" << field_name2 << ");\n";
			else if (search_type & SEARCH_TYPE_MATCH_N)
				os << tabs_prefix << "nret = memcmp(keyp->placeholder + keyp->" << field_name1 << ", datap->placeholder + datap->" << field_name2 << ", " << "strlen(datap->placeholder + datap->" << field_name2 << "));\n";
			else
				os << tabs_prefix << "nret = strcmp(keyp->placeholder + keyp->" << field_name1 << ", datap->placeholder + datap->" << field_name2 << ");\n";
			os << tabs_prefix << "if (nret >= 0)\n";

			if (match)
				retval_str = "0";
			else
				retval_str = "nret + 1";
			os << tabs_prefix << "\treturn " << retval_str << ";\n";
		} else if (search_type & SEARCH_TYPE_MORE) {
			if (search_type & SEARCH_TYPE_IGNORE_CASE)
				os << tabs_prefix << "nret = strcasecmp(keyp->placeholder + keyp->" << field_name1 << ", datap->placeholder + datap->" << field_name2 << ");\n";
			else if (search_type & SEARCH_TYPE_MATCH_N)
				os << tabs_prefix << "nret = memcmp(keyp->placeholder + keyp->" << field_name1 << ", datap->placeholder + datap->" << field_name2 << ", " << "strlen(datap->placeholder + datap->" << field_name2 << "));\n";
			else
				os << tabs_prefix << "nret = strcmp(keyp->placeholder + keyp->" << field_name1 << ", datap->placeholder + datap->" << field_name2 << ");\n";
			os << tabs_prefix << "if (nret <= 0)\n";

			if (match)
				retval_str = "0";
			else
				retval_str = "nret - 1";
			os << tabs_prefix << "\treturn " << retval_str << ";\n";
		} else {
			throw bad_param(__FILE__, __LINE__, 213, (_("ERROR: index {1}: search_type {2} not supported.") % index_name % search_type).str(SGLOCALE));
		}

		if (search_type & SEARCH_TYPE_WILDCARD)
			os << "\t}\n";
		break;
	case SQLTYPE_UNKNOWN:
	case SQLTYPE_ANY:
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Unknown field_type {1}") % field_type).str(SGLOCALE));
	}
}

}
}

