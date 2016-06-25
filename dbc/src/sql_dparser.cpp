#include "sql_dparser.h"
#include "dbc_admin.h"

namespace bi = boost::interprocess;
namespace bio = boost::io;
namespace po = boost::program_options;
using namespace std;

extern void yyrestart(FILE *input_file);

namespace ai
{
namespace scci
{

extern int inquote;
extern int incomments;
extern string tminline;
extern int line_no;
extern int column;

}
}

namespace ai
{
namespace sg
{

using namespace ai::scci;

desc_dparser::desc_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("table,T", po::value<string>(&table_name)->required(), (_("specify table name to describe, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;
	pdesc.add("table", 1);
}

desc_dparser::~desc_dparser()
{
}

dparser_result_t desc_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.find_te(table_name);
	if (te == NULL) {
		err_msg = (_("ERROR: object {1} not exist.") % table_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_data_t& te_meta = te->te_meta;

	int name_len = 4;
	int type_len = 4;
	int i;
	for (i = 0; i < te_meta.field_size; i++) {
		dbc_data_field_t& field = te_meta.fields[i];

		int len = strlen(field.field_name);
		name_len = std::max(name_len, len);

		switch (field.field_type) {
		case SQLTYPE_CHAR:
			type_len = std::max(type_len, 4);
			break;
		case SQLTYPE_UCHAR:
			type_len = std::max(type_len, 5);
			break;
		case SQLTYPE_SHORT:
			type_len = std::max(type_len, 5);
			break;
		case SQLTYPE_USHORT:
			type_len = std::max(type_len, 6);
			break;
		case SQLTYPE_INT:
			type_len = std::max(type_len, 3);
			break;
		case SQLTYPE_UINT:
			type_len = std::max(type_len, 4);
			break;
		case SQLTYPE_LONG:
			type_len = std::max(type_len, 4);
			break;
		case SQLTYPE_ULONG:
			type_len = std::max(type_len, 5);
			break;
		case SQLTYPE_FLOAT:
			type_len = std::max(type_len, 5);
			break;
		case SQLTYPE_DOUBLE:
			type_len = std::max(type_len, 6);
			break;
		case SQLTYPE_STRING:
			type_len = std::max(type_len, static_cast<int>(log2(field.field_size - 1) / log2(10) + 7));
			break;
		case SQLTYPE_VSTRING:
			type_len = std::max(type_len, static_cast<int>(log2(field.field_size - 1) / log2(10) + 8));
			break;
		case SQLTYPE_DATE:
			type_len = std::max(type_len, 4);
			break;
		case SQLTYPE_TIME:
			type_len = std::max(type_len, 4);
			break;
		case SQLTYPE_DATETIME:
			type_len = std::max(type_len, 8);
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Unknown field_type {1}") % field.field_type).str(SGLOCALE));
		}
	}

	cout << std::left << std::setw(name_len + 1) << (_("Name")).str(SGLOCALE) << std::setw(type_len + 1) << (_("Type")).str(SGLOCALE) << (_("Primary")).str(SGLOCALE) << "\n";
	cout << std::right << std::setfill('-') << std::setw(name_len + 1) << " " << std::setw(type_len + 1) << " " << std::setw(6 + 1) << " " << "\n";
	cout << std::left << std::setfill(' ');

	for (i = 0; i < te_meta.field_size; i++) {
		dbc_data_field_t& field = te_meta.fields[i];

		cout << std::setw(name_len + 1) << field.field_name << std::setw(type_len + 1);
		switch (field.field_type) {
		case SQLTYPE_CHAR:
			cout << "CHAR";
			break;
		case SQLTYPE_UCHAR:
			cout << "UCHAR";
			break;
		case SQLTYPE_SHORT:
			cout << "SHORT";
			break;
		case SQLTYPE_USHORT:
			cout << "USHORT";
			break;
		case SQLTYPE_INT:
			cout << "INT";
			break;
		case SQLTYPE_UINT:
			cout << "UINT";
			break;
		case SQLTYPE_LONG:
			cout << "LONG";
			break;
		case SQLTYPE_ULONG:
			cout << "ULONG";
			break;
		case SQLTYPE_FLOAT:
			cout << "FLOAT";
			break;
		case SQLTYPE_DOUBLE:
			cout << "DOUBLE";
			break;
		case SQLTYPE_STRING:
			cout << (boost::format("CHAR(%1%)") % (field.field_size - 1)).str();
			break;
		case SQLTYPE_VSTRING:
			cout << (boost::format("VCHAR(%1%)") % (field.field_size - 1)).str();
			break;
		case SQLTYPE_DATE:
			cout << "DATE";
			break;
		case SQLTYPE_TIME:
			cout << "TIME";
			 break;
		case SQLTYPE_DATETIME:
			cout << "DATETIME";
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Unknown field_type {1}") % field.field_type).str(SGLOCALE));
		}
		cout << std::setw(6 + 1) << (field.is_primary ? "Y" : "N") << "\n";
	}

	cout << std::right << "\n";

	// Describe indexes.
	te_mgr.set_ctx(te);
	for (int index_id = 0; index_id <= te->max_index; index_id++) {
		dbc_ie_t *ie = te_mgr.get_index(index_id);
		if (ie == NULL)
			continue;

		dbc_index_t& ie_meta = ie->ie_meta;

		if (index_id > 0)
			cout << "\n";

		cout << "create ";
		switch (ie_meta.index_type) {
		case INDEX_TYPE_PRIMARY:
			cout << "primary key ";
			break;
		case INDEX_TYPE_UNIQUE:
			cout << "unique index ";
			break;
		case INDEX_TYPE_NORMAL:
			cout << "index ";
			break;
		case INDEX_TYPE_RANGE:
			cout << "range index ";
			break;
		}
		cout << ie_meta.index_name << " on ";
		cout << te_meta.table_name << "(";
		bool first = true;
		for (i = 0; i < ie_meta.field_size; i++) {
			if (ie_meta.fields[i].search_type) {
				if (!first)
					cout << ", ";
				first = false;
				cout << ie_meta.fields[i].field_name;
			}
		}
		cout << ");\n";
	}

	return DPARSE_SUCCESS;
}

sql_dparser::sql_dparser(int flags_, const string& command_, dbc_admin *admin_mgr_)
	: cmd_dparser(flags_),
	  command(command_),
	  admin_mgr(admin_mgr_)
{
}

sql_dparser::~sql_dparser()
{
}

dparser_result_t sql_dparser::parse(int argc, char **argv)
{
	char *last_arg = argv[argc - 1];
	int len = strlen(last_arg);
	if (len > 0 && last_arg[len - 1] == ';')
		last_arg[len - 1] = '\0';

	if (command == "create") {
		if (argc >= 3 && strcasecmp(argv[1], "sequence") == 0)
			return do_screate(argc, argv);
		else
			return do_create(admin_mgr->get_cmd());
	} else if (command == "drop") {
		return do_drop(argc, argv);
	} else if (command == "alter") {
		return do_alter(argc, argv);
	} else if (command == "commit") {
		return do_commit();
	} else if (command == "rollback") {
		return do_rollback();
	} else if (command == "select") {
		string sql;
		if (argc > 3 && strcmp(argv[1], "*") == 0 && strcasecmp(argv[2], "from") == 0) {
			dbc_te& te_mgr = dbc_te::instance(DBCT);
			dbc_te_t *te = te_mgr.find_te(argv[3]);
			if (te == NULL) {
				err_msg = (_("ERROR: object {1} not exist.") % argv[3]).str(SGLOCALE);
				return DPARSE_CERR;
			}

			sql = "select";

			dbc_data_t& te_meta = te->te_meta;
			for (int i = 0; i < te_meta.field_size; i++) {
				if (i == 0)
					sql += " ";
				else
					sql += ", ";
				sql += te_meta.fields[i].field_name;
			}

			// Skip over first *
			string& cmd = admin_mgr->get_cmd();
			string::size_type pos = cmd.find("*");
			sql += cmd.substr(pos + 1);
		} else {
			sql = admin_mgr->get_cmd();
		}

		if (global_flags & SDC)
			return do_sdc_select(sql);
		else
			return do_select(sql);
	} else if (command == "insert") {
		return do_insert(admin_mgr->get_cmd());
	} else if (command == "update") {
		return do_update(admin_mgr->get_cmd());
	} else if (command == "delete") {
		return do_delete(admin_mgr->get_cmd());
	} else if (command == "truncate") {
		return do_truncate(argc, argv);
	} else {
		err_msg = (_("ERROR: unknown command {1} given.") % command).str(SGLOCALE);
		return DPARSE_CERR;
	}
}

dparser_result_t sql_dparser::do_screate(int argc, char **argv)
{
	dbc_se_t se_node;
	se_node.init();

	if (strlen(argv[2]) > SEQUENCE_NAME_SIZE) {
		err_msg = (_("ERROR: too long sequence name.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	strcpy(se_node.seq_name, argv[2]);

	for (int i = 3; i < argc; i++) {
		if (strcasecmp(argv[i], "minvalue") == 0) {
			if (i + 1 >= argc) {
				err_msg = (_("ERROR: missing value for [minvalue].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.minval = atol(argv[i + 1]);
			i++;
		} else if (strcasecmp(argv[i], "maxvalue") == 0) {
			if (i + 1 >= argc) {
				err_msg = (_("ERROR: missing value for [maxvalue].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.maxval = atol(argv[i + 1]);
			i++;
		} else if (strcasecmp(argv[i], "start") == 0) {
			if (i + 2 >= argc) {
				err_msg = (_("ERROR: missing key word [with].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			if (strcasecmp(argv[i + 1], "with") != 0) {
				err_msg = (_("ERROR: key word [with] should be followed by [start].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.currval = atol(argv[i + 2]);
			i += 2;
		} else if (strcasecmp(argv[i], "increment") == 0) {
			if (i + 2 >= argc) {
				err_msg = (_("ERROR: missing key word [by].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			if (strcasecmp(argv[i + 1], "by") != 0) {
				err_msg = (_("ERROR: key word [by] should be followed by [increment].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.increment = atol(argv[i + 2]);
			i += 2;
		} else if (strcasecmp(argv[i], "cache") == 0) {
			if (i + 1 >= argc) {
				err_msg = (_("ERROR: missing value for [cache].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.cache = atol(argv[i + 1]);
			i++;
		} else if (strcasecmp(argv[i], "cycle") == 0) {
			se_node.cycle = true;
		} else if (strcasecmp(argv[i], "order") == 0) {
			se_node.order = true;
		} else if (strcasecmp(argv[i], "nominvalue") != 0 && strcasecmp(argv[i], "nomaxvalue") != 0
			&& strcasecmp(argv[i], "nocache") != 0 && strcasecmp(argv[i], "nocycle") != 0
			&& strcasecmp(argv[i], "noorder") != 0) {
			err_msg = (_("ERROR: gotten unknown key word {1}") % argv[i]).str(SGLOCALE);
			return DPARSE_CERR;
		}
	}

	if (se_node.minval < 0) {
		err_msg =  (_("ERROR: minvalue should be a zero or positive number.")).str(SGLOCALE);
		return DPARSE_CERR;
	} else if (se_node.maxval <= se_node.minval) {
		err_msg =  (_("ERROR: maxvalue should be bigger than minvalue.")).str(SGLOCALE);
		return DPARSE_CERR;
	} else if (se_node.currval < se_node.minval || se_node.currval > se_node.maxval) {
		err_msg = (_("ERROR: currval should between minvalue and maxvalue.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_api& api_mgr = dbc_api::instance();
	if (!api_mgr.create_sequence(se_node)) {
		err_msg = (_("ERROR: failed to create object {1}, see log for detailed information.") % argv[2]).str(SGLOCALE);
		return DPARSE_CERR;
	}

	cout << "\n" << (_("sequence {1} created successfully.\n") % argv[2]).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

dparser_result_t sql_dparser::do_create(const string& sql)
{
	int ret;
	char temp_name[4096];
	const char *tmpdir = ::getenv("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = P_tmpdir;

	sprintf(temp_name, "%s/scciXXXXXX", tmpdir);

	int fd = ::mkstemp(temp_name);
	if (fd == -1) {
		err_msg = (_("ERROR: mkstemp failure, {1}") % ::strerror(errno)).str(SGLOCALE);
		return DPARSE_CERR;
	}

	yyin = ::fdopen(fd, "w+");
	if (yyin == NULL) {
		::close(fd);
		::unlink(temp_name);
		err_msg = (_("ERROR: open temp file [{1}] failure, {2}") % temp_name % ::strerror(errno)).str(SGLOCALE);
		return DPARSE_CERR;
	}

	if (::fwrite(sql.c_str(), sql.length(), 1, yyin) != 1) {
		::fclose(yyin);
		::unlink(temp_name);
		err_msg = (_("ERROR: write sql to file [{1}] failure, {2}") % temp_name % ::strerror(errno)).str(SGLOCALE);
		return DPARSE_CERR;
	}

	::fflush(yyin);
	::fseek(yyin, 0, SEEK_SET);

	yyout = stdout;		// default
	errcnt = 0;

	inquote = 0;
	incomments = 0;
	tminline = "";
	line_no = 0;
	column = 0;

	sql_statement::clear();

	// call yyparse() to parse temp file
	yyrestart(yyin);
	if ((ret = yyparse()) < 0) {
		::fclose(yyin);
		::unlink(temp_name);
		err_msg = (_("ERROR: Parse failed.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	if (ret == 1) {
		::fclose(yyin);
		::unlink(temp_name);
		err_msg = (_("ERROR: Severe error found. Stop syntax checking.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	if (errcnt > 0) {
		::fclose(yyin);
		::unlink(temp_name);
		err_msg = (_("ERROR: Above errors found during syntax checking.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	::fclose(yyin);
	::unlink(temp_name);

	map<string, sql_field_t> field_map;
	sql_create *stmt = dynamic_cast<sql_create *>(sql_statement::first());
	string table_name = stmt->gen_table();

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.find_te(table_name);
	if (te != NULL) {
		err_msg = (_("ERROR: table {1} exists") % table_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_te_t dbc_te;

	dbc_te.max_index = -1;
	dbc_te.cur_count = 0;
	dbc_data_t& te_meta = dbc_te.te_meta;

	arg_list_t *field_items = stmt->get_field_items();

	std::transform(table_name.begin(), table_name.end(), table_name.begin(), tolower_predicate());
	strcpy(te_meta.table_name, table_name.c_str());
	te_meta.conditions[0] = '\0';
	te_meta.escape = '\0';
	te_meta.delimiter = '\0';
	te_meta.segment_rows = DFLT_SEGMENT_ROWS;
	te_meta.refresh_type = REFRESH_TYPE_IN_MEM;
	te_meta.field_size = 0;

	bool is_variable = false;
	int extra_size = 0;
	int struct_align = CHAR_ALIGN;
	int internal_align = CHAR_ALIGN;
	int struct_offset = 0;
	int internal_offset = 0;
	for (arg_list_t::const_iterator iter = field_items->begin(); iter != field_items->end(); ++iter) {
		simple_item_t *simple_item = (*iter)->simple_item;
		field_desc_t *field_desc = (*iter)->field_desc;

		dbc_data_field_t& item = te_meta.fields[te_meta.field_size++];
		string field_name = simple_item->value;
		std::transform(field_name.begin(), field_name.end(), field_name.begin(), tolower_predicate());
		strcpy(item.field_name, field_name.c_str());
		strcpy(item.fetch_name, field_name.c_str());
		item.update_name[0] = ':';
		strcpy(item.update_name + 1, field_name.c_str());
		strcpy(item.column_name, field_name.c_str());
		item.field_type = field_desc->type;

		switch (field_desc->type) {
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR:
			item.field_size = sizeof(char);
			break;
		case SQLTYPE_SHORT:
		case SQLTYPE_USHORT:
			item.field_size = sizeof(short);
			struct_align = std::max(struct_align, SHORT_ALIGN);
			internal_align = std::max(internal_align, SHORT_ALIGN);
			struct_offset = common::round_up(struct_offset, SHORT_ALIGN);
			internal_offset = common::round_up(internal_offset, SHORT_ALIGN);
			break;
		case SQLTYPE_INT:
		case SQLTYPE_UINT:
			item.field_size = sizeof(int);
			struct_align = std::max(struct_align, INT_ALIGN);
			internal_align = std::max(internal_align, INT_ALIGN);
			struct_offset = common::round_up(struct_offset, INT_ALIGN);
			internal_offset = common::round_up(internal_offset, INT_ALIGN);
			break;
		case SQLTYPE_LONG:
		case SQLTYPE_ULONG:
			item.field_size = sizeof(long);
			struct_align = std::max(struct_align, LONG_ALIGN);
			internal_align = std::max(internal_align, LONG_ALIGN);
			struct_offset = common::round_up(struct_offset, LONG_ALIGN);
			internal_offset = common::round_up(internal_offset, LONG_ALIGN);
			break;
		case SQLTYPE_FLOAT:
			item.field_size = sizeof(float);
			struct_align = std::max(struct_align, FLOAT_ALIGN);
			internal_align = std::max(internal_align, FLOAT_ALIGN);
			struct_offset = common::round_up(struct_offset, FLOAT_ALIGN);
			internal_offset = common::round_up(internal_offset, FLOAT_ALIGN);
			break;
		case SQLTYPE_DOUBLE:
			item.field_size = sizeof(double);
			struct_align = std::max(struct_align, DOUBLE_ALIGN);
			internal_align = std::max(internal_align, DOUBLE_ALIGN);
			struct_offset = common::round_up(struct_offset, DOUBLE_ALIGN);
			internal_offset = common::round_up(internal_offset, DOUBLE_ALIGN);
			break;
		case SQLTYPE_STRING:
			item.field_size = field_desc->len + 1;
			break;
		case SQLTYPE_VSTRING:
			item.field_size = field_desc->len + 1;
			if (item.field_size < sizeof(long)) {
				err_msg = (_("ERROR: field_size should be at least {1} long when field_type is vstring") % (sizeof(long) - 1)).str(SGLOCALE);
				return DPARSE_CERR;
			}
			internal_align = std::max(internal_align, INT_ALIGN);
			internal_offset = common::round_up(internal_offset, INT_ALIGN);
			is_variable = true;
			extra_size += item.field_size;
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			item.field_size = sizeof(time_t);
			struct_align = std::max(struct_align, TIME_T_ALIGN);
			internal_align = std::max(internal_align, TIME_T_ALIGN);
			struct_offset = common::round_up(struct_offset, TIME_T_ALIGN);
			internal_offset = common::round_up(internal_offset, TIME_T_ALIGN);
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			err_msg = (_("ERROR: Unknown field_type {1}") % item.field_type).str(SGLOCALE);
			return DPARSE_CERR;
		}

		item.field_offset = struct_offset;
		item.internal_offset = internal_offset;
		struct_offset += item.field_size;
		if (field_desc->type == SQLTYPE_VSTRING)
			internal_offset += sizeof(int);
		else
			internal_offset += item.field_size;
	}

	if (extra_size > MAX_VSLOTS * sizeof(long)) {
		err_msg = (_("ERROR: field_size should be at most {1} long when field_type is vstring") % (sizeof(long) - 1)).str(SGLOCALE);
		return DPARSE_CERR;
	}

	te_meta.struct_size = common::round_up(struct_offset, struct_align);
	struct_align = std::max(struct_align, STRUCT_ALIGN);
	// 变长格式的内部结构不需要对齐，因为最后一个字段是字符串型
	if (is_variable)
		te_meta.internal_size = internal_offset;
	else
		te_meta.internal_size = common::round_up(struct_offset, struct_align);
	te_meta.extra_size = extra_size;

	dbc_api& api_mgr = dbc_api::instance();
	if (!api_mgr.create(dbc_te)) {
		err_msg = (_("ERROR: create execution for table ")).str(SGLOCALE);
		err_msg += table_name;
		err_msg += " failed.";
		return DPARSE_CERR;
	}

	cout << "\n" << (_("table {1} created successfully.\n")% table_name).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

dparser_result_t sql_dparser::do_drop(int argc, char **argv)
{
	if (argc != 3)
		return DPARSE_SYNERR;

	int len = strlen(argv[2]);
	if (argv[2][len - 1] == ';')
		argv[2][len - 1] = '\0';

	dbc_api& api_mgr = dbc_api::instance();
	if (strcasecmp(argv[1], "table") == 0) {
		if (!api_mgr.drop(argv[2])) {
			err_msg = (_("ERROR: object {1} not exist.") % argv[2]).str(SGLOCALE);
			return DPARSE_CERR;
		}

		cout << "\n" << (_("table {1} dropped successfully.\n")% argv[2]).str(SGLOCALE);
		return DPARSE_SUCCESS;
	} else if (strcasecmp(argv[1], "sequence") == 0) {
		if (!api_mgr.drop_sequence(argv[2])) {
			err_msg = (_("ERROR: object {1} not exist.") % argv[2]).str(SGLOCALE);
			return DPARSE_CERR;
		}

		cout << "\n" << (_( "sequence {1} dropped successfully.\n")% argv[2]).str(SGLOCALE);
		return DPARSE_SUCCESS;
	}

	return DPARSE_SYNERR;
}

dparser_result_t sql_dparser::do_alter(int argc, char **argv)
{
	if (argc < 3 || strcasecmp(argv[1], "sequence") != 0)
		return DPARSE_SYNERR;

	if (strlen(argv[2]) > SEQUENCE_NAME_SIZE) {
		err_msg = (_("ERROR: too long sequence name.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_se_t *se = ue_mgr.get_seq(argv[2]);
	if (se == NULL) {
		err_msg = (_("ERROR: sequence {1} not exist.") % argv[2]).str(SGLOCALE);
		return DPARSE_CERR;
	}

	ipc_sem::tas(&se->accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&se)) {
		ipc_sem::semclear(&se->accword);
	} BOOST_SCOPE_EXIT_END

	dbc_se_t se_node = *se;
	for (int i = 3; i < argc; i++) {
		if (strcasecmp(argv[i], "minvalue") == 0) {
			if (i + 1 >= argc) {
				err_msg = (_("ERROR: missing value for [minvalue].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.minval = atol(argv[i + 1]);
			i++;
		} else if (strcasecmp(argv[i], "maxvalue") == 0) {
			if (i + 1 >= argc) {
				err_msg = (_("ERROR: missing value for [maxvalue].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.maxval = atol(argv[i + 1]);
			i++;
		} else if (strcasecmp(argv[i], "start") == 0) {
			if (i + 2 >= argc) {
				err_msg = (_("ERROR: missing key word [with].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			if (strcasecmp(argv[i + 1], "with") != 0) {
				err_msg = (_("ERROR: key word [with] should be followed by [start].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.currval = atol(argv[i + 2]);
			i += 2;
		} else if (strcasecmp(argv[i], "increment") == 0) {
			if (i + 2 >= argc) {
				err_msg = (_("ERROR: missing key word [by].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			if (strcasecmp(argv[i + 1], "by") != 0) {
				err_msg = (_("ERROR: key word [by] should be followed by [increment].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.increment = atol(argv[i + 2]);
			i += 2;
		} else if (strcasecmp(argv[i], "cache") == 0) {
			if (i + 1 >= argc) {
				err_msg = (_("ERROR: missing value for [cache].")).str(SGLOCALE);
				return DPARSE_CERR;
			}
			se_node.cache = atol(argv[i + 1]);
			i++;
		} else if (strcasecmp(argv[i], "cycle") == 0) {
			se_node.cycle = true;
		} else if (strcasecmp(argv[i], "order") == 0) {
			se_node.order = true;
		} else if (strcasecmp(argv[i], "nominvalue") != 0 && strcasecmp(argv[i], "nomaxvalue") != 0
			&& strcasecmp(argv[i], "nocache") != 0 && strcasecmp(argv[i], "nocycle") != 0
			&& strcasecmp(argv[i], "noorder") != 0) {
			err_msg = (_("ERROR: gotten unknown key word {1}") % argv[i]).str(SGLOCALE);
			return DPARSE_CERR;
		}
	}

	if (se_node.minval < 0) {
		err_msg =  (_("ERROR: minvalue should be a zero or positive number.")).str(SGLOCALE);
		return DPARSE_CERR;
	} else if (se_node.maxval <= se_node.minval) {
		err_msg =  (_("ERROR: maxvalue should be bigger than minvalue.")).str(SGLOCALE);
		return DPARSE_CERR;
	} else if (se_node.currval < se_node.minval || se_node.currval > se_node.maxval) {
		err_msg = (_("ERROR: currval should between minvalue and maxvalue.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_api& api_mgr = dbc_api::instance();
	*se = se_node;
	if (!api_mgr.alter_sequence(se)) {
		err_msg = (_("ERROR: failed to alter object {1}, see log for detailed information.") % argv[2]).str(SGLOCALE);
		return DPARSE_CERR;
	}

	cout << "\n" << (_("sequence {1} altered successfully.\n")% argv[2]).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

dparser_result_t sql_dparser::do_commit()
{
	dbc_api& api_mgr = dbc_api::instance(DBCT);
	if (!api_mgr.commit()) {
		err_msg = (_("ERROR: Commit execution failed, {1}") % DBCT->strerror()).str(SGLOCALE);
		return DPARSE_CERR;
	}

	cout << "\n" << (_("commit executed successfully.\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

dparser_result_t sql_dparser::do_rollback()
{
	dbc_api& api_mgr = dbc_api::instance(DBCT);
	if (!api_mgr.rollback()) {
		err_msg = (_("ERROR: Rollback execution failed.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	cout << "\n" << (_("rollback executed successfully.\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

dparser_result_t sql_dparser::do_select(const string& sql)
{
	int len;
	vector<int> len_vector;
	int i;
	Dbc_Database *dbc_db = admin_mgr->get_db();
	auto_ptr<Generic_Statement> auto_stmt(dbc_db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(dbc_db->create_data());
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		err_msg = (_("ERROR: Memory allocation failure")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	data->setSQL(sql);
	stmt->bind(data);

	auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
	Generic_ResultSet *rset = auto_rset.get();
	long affected_rows = 0;

	if (!verbose) {
		// Write out header
		for (i = 1; i <= rset->getColumnCount(); i++) {
			if (i > 1)
				cout << " ";
			sqltype_enum column_type = rset->getColumnType(i);
			switch (column_type) {
			case SQLTYPE_DATE:
				len = 8;
				break;
			case SQLTYPE_TIME:
				len = 6;
				break;
			case SQLTYPE_DATETIME:
				len = 14;
				break;
			default:
				len = 1;
				break;
			}

			if (len < rset->getColumnName(i).length())
				len = rset->getColumnName(i).length();
			len_vector.push_back(len);

			bio::ios_all_saver ias(cout);
			cout << std::left << std::setw(len) << rset->getColumnName(i);
		}
		cout << "\n";

		for (i = 0; i < rset->getColumnCount(); i++) {
			bio::ios_all_saver ias(cout);
			cout << std::setfill('-')
				<< std::setw(len_vector[i] + 1)
				<< ' ';
		}
		cout << "\n";

		while (rset->next()) {
			for (i = 1; i <= rset->getColumnCount(); i++) {
				bio::ios_all_saver ias(cout);

				if (i > 1)
					cout << " ";

				string str = rset->getString(i);
				if (len_vector[i - 1] < str.length())
					len_vector[i - 1] = str.length();
				cout << std::left << std::setw(len_vector[i - 1]) << str;
			}
			cout << "\n";
			affected_rows++;
		}
	} else {
		while (rset->next()) {
			if (affected_rows > 0)
				cout << "\n";

			for (i = 1; i <= rset->getColumnCount(); i++) {
				cout << rset->getColumnName(i)
					<< ": " << rset->getString(i)
					<< "\n";
			}
			affected_rows++;
		}
	}

	if (affected_rows > 1)
		cout << "\n" << affected_rows << (_(" records selected successfully.\n")).str(SGLOCALE);
	else
		cout << "\n" << affected_rows << (_(" record selected successfully.\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

dparser_result_t sql_dparser::do_insert(const string& sql)
{
	Dbc_Database *dbc_db = admin_mgr->get_db();
	auto_ptr<Generic_Statement> auto_stmt(dbc_db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(dbc_db->create_data());
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		err_msg = (_("ERROR: Memory allocation failure")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	data->setSQL(sql);
	stmt->bind(data);

	try {
		stmt->executeUpdate();
		cout << "\n" << (_("1 record inserted successfully.\n")).str(SGLOCALE);
		return DPARSE_SUCCESS;
	} catch (exception& ex) {
		err_msg = ex.what();
		return DPARSE_CERR;
	}
}

dparser_result_t sql_dparser::do_sdc_select(const string& sql)
{
	sg_api& sg_mgr = sg_api::instance(SGT);
	sgc_ctx *SGC = SGT->SGC();

	message_pointer send_msg = sg_message::create(SGT);
	message_pointer recv_msg = sg_message::create(SGT);

	size_t len = offsetof(sdc_sql_rqst_t, placeholder) + sql.length() + 1;
	send_msg->set_length(len);
	sdc_sql_rqst_t *rqst = reinterpret_cast<sdc_sql_rqst_t *>(send_msg->data());
	rqst->user_id = DBCT->_DBCT_user_id;
	rqst->max_rows = 10000;
	memcpy(rqst->placeholder, sql.c_str(), sql.length() + 1);

	set<int>& mids = admin_mgr->get_mids();
	for (set<int>::const_iterator iter = mids.begin(); iter != mids.end(); ++iter) {
		int mid = *iter;

		std::cout << (_("Result on node ")).str(SGLOCALE) << SGC->mid2pmid(mid) << ":\n";

		string svc_name = (boost::format("SDC_SQLSVC_%1%") % mid).str();
		if (!sg_mgr.call(svc_name.c_str(), send_msg, recv_msg, SGSIGRSTRT)) {
			std::cout << (_("ERROR: Call operation on node {1} failed\n") % mid).str(SGLOCALE);
		} else {
			string result(recv_msg->data(), recv_msg->data() + recv_msg->length());
			std::cout << result;
		}
		std::cout << "\n";
	}

	return DPARSE_SUCCESS;
}

dparser_result_t sql_dparser::do_update(const string& sql)
{
	Dbc_Database *dbc_db = admin_mgr->get_db();
	auto_ptr<Generic_Statement> auto_stmt(dbc_db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(dbc_db->create_data());
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		err_msg = (_("ERROR: Memory allocation failure")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	data->setSQL(sql);
	stmt->bind(data);

	try {
		stmt->executeUpdate();
		int update_count = stmt->getUpdateCount();
		if (update_count > 1)
			cout << "\n" << update_count << (_(" records updated successfully.\n")).str(SGLOCALE);
		else
			cout << "\n" << update_count << (_(" record updated successfully.\n")).str(SGLOCALE);
		return DPARSE_SUCCESS;
	} catch (exception& ex) {
		err_msg = ex.what();
		return DPARSE_CERR;
	}

	return DPARSE_SUCCESS;
}

dparser_result_t sql_dparser::do_delete(const string& sql)
{
	Dbc_Database *dbc_db = admin_mgr->get_db();
	auto_ptr<Generic_Statement> auto_stmt(dbc_db->create_statement());
	Generic_Statement *stmt = auto_stmt.get();

	auto_ptr<struct_dynamic> auto_data(dbc_db->create_data());
	struct_dynamic *data = auto_data.get();
	if (data == NULL) {
		err_msg = (_("ERROR: Memory allocation failure")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	data->setSQL(sql);
	stmt->bind(data);

	try {
		stmt->executeUpdate();
		int update_count = stmt->getUpdateCount();
		if (update_count > 1)
			cout << "\n" << update_count << (_(" records deleted successfully.\n")).str(SGLOCALE);
		else
			cout << "\n" << update_count << (_(" record deleted successfully.\n")).str(SGLOCALE);
		return DPARSE_SUCCESS;
	} catch (exception& ex) {
		err_msg = ex.what();
		return DPARSE_CERR;
	}

	return DPARSE_SUCCESS;
}

dparser_result_t sql_dparser::do_truncate(int argc, char **argv)
{
	if (argc != 3 || strcasecmp(argv[1], "table") != 0) {
		err_msg = (_("ERROR: invalid truncate SQL statement.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_api& api_mgr = dbc_api::instance();
	if (!api_mgr.truncate(argv[2])) {
		err_msg = (_("ERROR: truncate execution for table {1} failed.") % argv[2]).str(SGLOCALE);
		return DPARSE_CERR;
	}

	cout << "\n" << (_("table {1} truncated successfully.\n")% argv[2]).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

}
}

