#include <boost/lexical_cast.hpp>
#include "sql_base.h"
#include "sql_select.h"

namespace ai
{
namespace scci
{

gen_code_t::gen_code_t(const map<string, sql_field_t> *field_map_, const bind_field_t *bind_fields_)
{
	flags = GEN_CODE_DOT | GEN_CODE_USE_INDEX;
	field_map = field_map_;
	bind_fields = bind_fields_;
	row_index = 0;
}

gen_code_t::~gen_code_t()
{
}

simple_item_t::simple_item_t()
{
}

simple_item_t::~simple_item_t()
{
}

void simple_item_t::gen_sql(string& sql, bool dot_flag) const
{
	switch (word_type) {
	case WORDTYPE_CONSTANT:
		sql += value;
		break;
	case WORDTYPE_DCONSTANT:
		sql += value;
		break;
	case WORDTYPE_STRING:
		sql += "'";
		for (string::const_iterator iter = value.begin(); iter != value.end(); ++iter) {
			if (*iter == '\'')
				sql += "''";
			else
				sql += *iter;
		}
		sql += "'";
		break;
	case WORDTYPE_IDENT:
		if (!schema.empty()) {
			sql += schema;
			if (dot_flag)
				sql += ".";
			else
				sql += " ";
		}
		sql += value;
		break;
	case WORDTYPE_NULL:
		sql += "null";
		break;
	default:
		break;
	}
}

void simple_item_t::gen_code(gen_code_t& para) const
{
	string& code = para.code;
	const map<string, sql_field_t>& field_map = *para.field_map;

	switch (word_type) {
	case WORDTYPE_CONSTANT:
		code += value;
		break;
	case WORDTYPE_DCONSTANT:
		code += value;
		break;
	case WORDTYPE_STRING:
		code += '\"';
		for (string::const_iterator iter = value.begin(); iter != value.end(); ++iter) {
			if (*iter == '\\')
				code += "\\\\";
			else if (*iter == '\"')
				code += "\\\"";
			else
				code += *iter;
		}
		code += '\"';
		break;
	case WORDTYPE_IDENT:
		if (!schema.empty()) {
			if (strcasecmp(value.c_str(), "currval") == 0) {
				code += "get_currval(\"";
				code += schema;
				code += "\")";
				break;
			} else if (strcasecmp(value.c_str(), "nextval") == 0) {
				code += "get_nextval(\"";
				code += schema;
				code += "\")";
				break;
			}
		}

		if (strcasecmp(value.c_str(), "rowid") == 0) {
			code += "(global[0] - global[1])";
		} else if (strcasecmp(value.c_str(), "sysdate") == 0) {
			code += "*reinterpret_cast<const long *>(global[2])";
		} else {
			ostringstream fmt;
			map<string, sql_field_t>::const_iterator iter = field_map.find(value);
			if (iter == field_map.end())
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: field name not recognized")).str(SGLOCALE));

			int offset;
			if (para.use_internal())
				offset = iter->second.internal_offset;
			else
				offset = iter->second.field_offset;

			switch (iter->second.field_type) {
			case SQLTYPE_CHAR:
			case SQLTYPE_UCHAR:
				fmt << "global[" << para.row_index << "][" << offset << "]";
				break;
			case SQLTYPE_SHORT:
				fmt << "*reinterpret_cast<const short *>(global[" << para.row_index << "] + " << offset << ")";
				break;
			case SQLTYPE_USHORT:
				fmt << "*reinterpret_cast<const unsigned short *>(global[" << para.row_index << "] + " << offset << ")";
				break;
			case SQLTYPE_INT:
				fmt << "*reinterpret_cast<const int *>(global[" << para.row_index << "] + " << offset << ")";
				break;
			case SQLTYPE_UINT:
				fmt << "*reinterpret_cast<const unsigned int *>(global[" << para.row_index << "] + " << offset << ")";
				break;
			case SQLTYPE_LONG:
				fmt << "*reinterpret_cast<const long *>(global[" << para.row_index << "] + " << offset << ")";
				break;
			case SQLTYPE_ULONG:
				fmt << "*reinterpret_cast<const unsigned long *>(global[" << para.row_index << "] + " << offset << ")";
				break;
			case SQLTYPE_FLOAT:
				fmt << "*reinterpret_cast<const float *>(global[" << para.row_index << "] + " << offset << ")";
				break;
			case SQLTYPE_DOUBLE:
				fmt << "*reinterpret_cast<const double *>(global[" << para.row_index << "] + " << offset << ")";
				break;
			case SQLTYPE_STRING:
				fmt << "global[" << para.row_index << "] + " << offset;
				break;
			case SQLTYPE_VSTRING:
				fmt << "dbc_vdata(global[" << para.row_index << "], " << offset << ")";
				break;
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				fmt << "*reinterpret_cast<const time_t *>(global[" << para.row_index << "] + " << offset << ")";
				break;
			case SQLTYPE_UNKNOWN:
			case SQLTYPE_ANY:
			default:
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unknown type given.")).str(SGLOCALE));
			}
			code += fmt.str();
		}
		para.clear_can_use_index();
		break;
	case WORDTYPE_NULL:
		code += "null";
		break;
	default:
		break;
	}
}

void simple_item_t::print() const
{
	cout << print_prefix << "{simple_item\n";
	print_prefix += "  ";

	switch (word_type) {
	case WORDTYPE_CONSTANT:
		cout << print_prefix << "word type = WORDTYPE_CONSTANT\n";
		break;
	case WORDTYPE_DCONSTANT:
		cout << print_prefix << "word type = WORDTYPE_DCONSTANT\n";
		break;
	case WORDTYPE_STRING:
		cout << print_prefix << "word type = WORDTYPE_STRING\n";
		break;
	case WORDTYPE_IDENT:
		cout << print_prefix << "word type = WORDTYPE_IDENT\n";
		break;
	case WORDTYPE_NULL:
		cout << print_prefix << "word type = WORDTYPE_NULL\n";
		break;
	default:
		cout << print_prefix << "word type = UNKNOWN\n";
		break;
	}
	cout << print_prefix << "schema = " << schema << "\n";
	cout << print_prefix << "value = " << value << "\n";

	print_prefix.resize(print_prefix.size() - 2);
	cout << print_prefix << "}simple_item\n";
}

void simple_item_t::push(vector<sql_item_t>& vector_root)
{
	sql_item_t item;
	item.type = ITEMTYPE_SIMPLE;
	item.data = reinterpret_cast<void *>(this);
	vector_root.push_back(item);
}

field_desc_t::field_desc_t()
{
}

field_desc_t::~field_desc_t()
{
}

void field_desc_t::gen_sql(string& sql, bool dot_flag) const
{
	dbtype_enum dbtype = sql_statement::get_dbtype();

	if (!alias.empty()) {
		if (*sql.rbegin() != ':') {
			sql += " ";
			if (dbtype == DBTYPE_GP)
				sql += "as ";
			sql += alias;
		} else {
			switch (dbtype) {
			case DBTYPE_GP:
				sql.resize(sql.length() - 1);
				sql += "$";
				sql += boost::lexical_cast<string>(sql_statement::get_bind_pos());
				break;
			default:
				sql += alias;
				break;
			}
		}
	}
}

void field_desc_t::gen_code(gen_code_t& para) const
{
	string& code = para.code;
	const bind_field_t *bind_fields = para.bind_fields;

	if (bind_fields == NULL)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: wrong bind_fields given.")).str(SGLOCALE));

	int index = 0;
	const bind_field_t *current_field;
	for (current_field = bind_fields; current_field->field_type != SQLTYPE_UNKNOWN; ++current_field) {
		if (current_field->field_name == alias || current_field->field_name == string(":") + alias)
			break;
		index++;
	}

	if (current_field->field_type == SQLTYPE_UNKNOWN)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find field in bind_fields.")).str(SGLOCALE));

	ostringstream fmt;
	switch (current_field->field_type) {
	case SQLTYPE_CHAR:
		fmt << "*reinterpret_cast<const char *>($" << index << ")";
		break;
	case SQLTYPE_UCHAR:
		fmt << "*reinterpret_cast<const unsigned char *>($" << index << ")";
		break;
	case SQLTYPE_SHORT:
		fmt << "*reinterpret_cast<const short *>($" << index << ")";
		break;
	case SQLTYPE_USHORT:
		fmt << "*reinterpret_cast<const unsigned short *>($" << index << ")";
		break;
	case SQLTYPE_INT:
		fmt << "*reinterpret_cast<const int *>($" << index << ")";
		break;
	case SQLTYPE_UINT:
		fmt << "*reinterpret_cast<const unsigned int *>($" << index << ")";
		break;
	case SQLTYPE_LONG:
		fmt << "*reinterpret_cast<const long *>($" << index << ")";
		break;
	case SQLTYPE_ULONG:
		fmt << "*reinterpret_cast<const unsigned long *>($" << index << ")";
		break;
	case SQLTYPE_FLOAT:
		fmt << "*reinterpret_cast<const float *>($" << index << ")";
		break;
	case SQLTYPE_DOUBLE:
		fmt << "*reinterpret_cast<const double *>($" << index << ")";
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt << "$" << index;
		break;
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		fmt << "*reinterpret_cast<const time_t *>($" << index << ")";
		break;
	case SQLTYPE_UNKNOWN:
	case SQLTYPE_ANY:
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unknown type given.")).str(SGLOCALE));
	}

	code += fmt.str();
}

void field_desc_t::print() const
{
	cout << print_prefix << "{field_desc\n";
	print_prefix += "  ";

	switch (type) {
	case SQLTYPE_CHAR:
		cout << print_prefix << "type = SQLTYPE_CHAR\n";
		break;
	case SQLTYPE_UCHAR:
		cout << print_prefix << "type = SQLTYPE_UCHAR\n";
		break;
	case SQLTYPE_SHORT:
		cout << print_prefix << "type = SQLTYPE_SHORT\n";
		break;
	case SQLTYPE_USHORT:
		cout << print_prefix << "type = SQLTYPE_USHORT\n";
		break;
	case SQLTYPE_INT:
		cout << print_prefix << "type = SQLTYPE_INT\n";
		break;
	case SQLTYPE_UINT:
		cout << print_prefix << "type = SQLTYPE_UINT\n";
		break;
	case SQLTYPE_LONG:
		cout << print_prefix << "type = SQLTYPE_LONG\n";
		break;
	case SQLTYPE_ULONG:
		cout << print_prefix << "type = SQLTYPE_ULONG\n";
		break;
	case SQLTYPE_FLOAT:
		cout << print_prefix << "type = SQLTYPE_FLOAT\n";
		break;
	case SQLTYPE_DOUBLE:
		cout << print_prefix << "type = SQLTYPE_DOUBLE\n";
		break;
	case SQLTYPE_STRING:
		cout << print_prefix << "type = SQLTYPE_STRING\n";
		break;
	case SQLTYPE_VSTRING:
		cout << print_prefix << "type = SQLTYPE_VSTRING\n";
		break;
	case SQLTYPE_DATE:
		cout << print_prefix << "type = SQLTYPE_DATE\n";
		break;
	case SQLTYPE_TIME:
		cout << print_prefix << "type = SQLTYPE_TIME\n";
		break;
	case SQLTYPE_DATETIME:
		cout << print_prefix << "type = SQLTYPE_DATETIME\n";
		break;
	case SQLTYPE_UNKNOWN:
	case SQLTYPE_ANY:
	default:
		cout << print_prefix << "type = UNKOWN\n";
		break;
	}
	cout << print_prefix << "len = " << len << "\n";
	cout << print_prefix << "alias = " << alias << "\n";

	print_prefix.resize(print_prefix.size() - 2);
	cout << print_prefix << "}field_desc\n";
}

relation_item_t::relation_item_t()
{
	left = NULL;
	right = NULL;
	has_curve = false;
}

relation_item_t::relation_item_t(optype_enum op_, composite_item_t *left_, composite_item_t *right_)
{
	op = op_;
	left = left_;
	right = right_;
	has_curve = false;
}

relation_item_t::~relation_item_t()
{
	delete left;
	delete right;
}

void relation_item_t::gen_sql(string& sql, bool dot_flag) const
{
	if (has_curve)
		sql += "(";

	if (left)
		left->gen_sql(sql, dot_flag);

	switch (op) {
	case OPTYPE_OR:
		sql += " OR ";
		break;
	case OPTYPE_AND:
		sql += " AND ";
		break;
	case OPTYPE_NOT:
		if (*sql.rbegin() != ' ')
			sql += " ";
		sql += "NOT ";
		break;
	case OPTYPE_GT:
		sql += " > ";
		break;
	case OPTYPE_LT:
		sql += " < ";
		break;
	case OPTYPE_GE:
		sql += " >= ";
		break;
	case OPTYPE_LE:
		sql += " <= ";
		break;
	case OPTYPE_EQ:
		sql += " = ";
		break;
	case OPTYPE_NEQ:
		sql += " <> ";
		break;
	case OPTYPE_ADD:
		sql += " + ";
		break;
	case OPTYPE_SUB:
		sql += " - ";
		break;
	case OPTYPE_STRCAT:
		sql += " || ";
		break;
	case OPTYPE_MUL:
		sql += " * ";
		break;
	case OPTYPE_DIV:
		sql += " / ";
		break;
	case OPTYPE_MOD:
		sql += " % ";
		break;
	case OPTYPE_UMINUS:
		if (*sql.rbegin() != ' ')
			sql += " ";
		sql += "-";
		break;
	case OPTYPE_IN:
		sql += " IN ";
		break;
	case OPTYPE_NOT_IN:
		sql += " NOT IN ";
		break;
	case OPTYPE_BETWEEN:
		sql += " BETWEEN ";
		break;
	case OPTYPE_EXISTS:
		sql += "EXISTS ";
		break;
	case OPTYPE_IS_NULL:
		sql += " IS NULL";
		break;
	case OPTYPE_IS_NOT_NULL:
		sql += " IS NOT NULL";
		break;
	default:
		break;
	}

	if (right)
		right->gen_sql(sql, dot_flag);

	if (has_curve)
		sql += ")";
}

void relation_item_t::gen_code(gen_code_t& para) const
{
	string& code = para.code;

	if (op == OPTYPE_IN || op == OPTYPE_NOT_IN) {
		if (op == OPTYPE_IN)
			code += "dbc_in(";
		else
			code += "!dbc_in(";

		left->gen_code(para);

		arg_list_t *arg_item = right->arg_item;
		for (arg_list_t::const_iterator iter = arg_item->begin(); iter != arg_item->end(); ++iter) {
			code += ", ";
			(*iter)->gen_code(para);
		}
		code += ")";

		return;
	} else if (op == OPTYPE_BETWEEN) {
		code += "dbc_between(";

		left->gen_code(para);

		relation_item_t *rela_item = right->rela_item;
		rela_item->left->gen_code(para);
		code += ", ";
		rela_item->right->gen_code(para);
		code += ")";

		return;
	}

	if (has_curve)
		code += "(";

	bool use_function = true;
	switch (op) {
	case OPTYPE_GT:
		code += "dbc_gt(";
		break;
	case OPTYPE_LT:
		code += "dbc_lt(";
		break;
	case OPTYPE_GE:
		code += "dbc_ge(";
		break;
	case OPTYPE_LE:
		code += "dbc_le(";
		break;
	case OPTYPE_EQ:
		code += "dbc_eq(";
		break;
	case OPTYPE_NEQ:
		code += "dbc_ne(";
		break;
	case OPTYPE_STRCAT:
		code += "dbc_strcat(";
		break;
	case OPTYPE_OR:
	case OPTYPE_AND:
	case OPTYPE_NOT:
	case OPTYPE_ADD:
	case OPTYPE_SUB:
	case OPTYPE_MUL:
	case OPTYPE_DIV:
	case OPTYPE_MOD:
	case OPTYPE_UMINUS:
	case OPTYPE_IN:
	case OPTYPE_NOT_IN:
	case OPTYPE_BETWEEN:
	case OPTYPE_EXISTS:
	case OPTYPE_IS_NULL:
	case OPTYPE_IS_NOT_NULL:
	default:
		use_function = false;
		break;
	}

	if (left)
		left->gen_code(para);

	if (use_function)
		code += ", ";

	switch (op) {
	case OPTYPE_OR:
		code += " || ";
		break;
	case OPTYPE_AND:
		code += " && ";
		break;
	case OPTYPE_NOT:
		if (*code.rbegin() != ' ')
			code += " ";
		code += "!";
		break;
	case OPTYPE_ADD:
		code += " + ";
		break;
	case OPTYPE_SUB:
		code += " - ";
		break;
	case OPTYPE_MUL:
		code += " * ";
		break;
	case OPTYPE_DIV:
		code += " / ";
		break;
	case OPTYPE_MOD:
		code += " % ";
		break;
	case OPTYPE_UMINUS:
		if (*code.rbegin() != ' ')
			code += " ";
		code += "-";
		break;
	case OPTYPE_GT:
	case OPTYPE_LT:
	case OPTYPE_GE:
	case OPTYPE_LE:
	case OPTYPE_EQ:
	case OPTYPE_NEQ:
	case OPTYPE_STRCAT:
	case OPTYPE_IN:
	case OPTYPE_NOT_IN:
		// Alreay handled previously.
		break;
	case OPTYPE_BETWEEN:
	case OPTYPE_EXISTS:
	case OPTYPE_IS_NULL:
	case OPTYPE_IS_NOT_NULL:
	default:
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Unsupported operator")).str(SGLOCALE));
	}

	if (right)
		right->gen_code(para);

	if (use_function)
		code += ")";

	if (has_curve)
		code += ")";
}


void relation_item_t::print() const
{
	cout << print_prefix << "{relation_item\n";
	print_prefix += "  ";

	if (left)
		left->print();

	switch (op) {
	case OPTYPE_OR:
		cout << print_prefix << "op = OPTYPE_OR\n";
		break;
	case OPTYPE_AND:
		cout << print_prefix << "op = OPTYPE_AND\n";
		break;
	case OPTYPE_NOT:
		cout << print_prefix << "op = OPTYPE_NOT\n";
		break;
	case OPTYPE_GT:
		cout << print_prefix << "op = OPTYPE_GT\n";
		break;
	case OPTYPE_LT:
		cout << print_prefix << "op = OPTYPE_LT\n";
		break;
	case OPTYPE_GE:
		cout << print_prefix << "op = OPTYPE_GE\n";
		break;
	case OPTYPE_LE:
		cout << print_prefix << "op = OPTYPE_LE\n";
		break;
	case OPTYPE_EQ:
		cout << print_prefix << "op = OPTYPE_EQ\n";
		break;
	case OPTYPE_NEQ:
		cout << print_prefix << "op = OPTYPE_NEQ\n";
		break;
	case OPTYPE_ADD:
		cout << print_prefix << "op = OPTYPE_ADD\n";
		break;
	case OPTYPE_SUB:
		cout << print_prefix << "op = OPTYPE_SUB\n";
		break;
	case OPTYPE_STRCAT:
		cout << print_prefix << "op = OPTYPE_STRCAT\n";
		break;
	case OPTYPE_MUL:
		cout << print_prefix << "op = OPTYPE_MUL\n";
		break;
	case OPTYPE_DIV:
		cout << print_prefix << "op = OPTYPE_DIV\n";
		break;
	case OPTYPE_MOD:
		cout << print_prefix << "op = OPTYPE_MOD\n";
		break;
	case OPTYPE_UMINUS:
		cout << print_prefix << "op = OPTYPE_UMINUS\n";
		break;
	case OPTYPE_IN:
		cout << print_prefix << "op = OPTYPE_IN\n";
		break;
	case OPTYPE_NOT_IN:
		cout << print_prefix << "op = OPTYPE_NOT_IN\n";
		break;
	case OPTYPE_BETWEEN:
		cout << print_prefix << "op = OPTYPE_BETWEEN\n";
		break;
	case OPTYPE_EXISTS:
		cout << print_prefix << "op = OPTYPE_EXISTS\n";
		break;
	case OPTYPE_IS_NULL:
		cout << print_prefix << "op = OPTYPE_IS_NULL\n";
		break;
	case OPTYPE_IS_NOT_NULL:
		cout << print_prefix << "op = OPTYPE_IS_NOT_NULL\n";
		break;
	default:
		cout << print_prefix << "op = UNKNOWN\n";
		break;

	}

	if (right)
		right->print();

	cout << print_prefix << "has_curve = " << has_curve << "\n";

	print_prefix.resize(print_prefix.size() - 2);
	cout << print_prefix << "}relation_item\n";
}

void relation_item_t::push(vector<sql_item_t>& vector_root)
{
	if (left)
		left->push(vector_root);

	sql_item_t item;
	item.type = ITEMTYPE_RELA;
	item.data = reinterpret_cast<void *>(this);
	vector_root.push_back(item);

	if (right)
		right->push(vector_root);
}

composite_item_t::composite_item_t()
{
	simple_item = NULL;
	func_item = NULL;
	rela_item = NULL;
	field_desc = NULL;
	arg_item = NULL;
	select_item = NULL;
	order_type = ORDERTYPE_NONE;
	has_curve = false;
}

composite_item_t::~composite_item_t()
{
	delete simple_item;
	delete func_item;
	delete rela_item;
	delete field_desc;

	if (arg_item) {
		for (arg_list_t::iterator iter = arg_item->begin(); iter != arg_item->end(); ++iter)
			delete (*iter);
		delete arg_item;
	}

	delete select_item;
}

void composite_item_t::gen_sql(string& sql, bool dot_flag) const
{
	if (has_curve)
		sql += "(";

	if (simple_item) {
		simple_item->gen_sql(sql, dot_flag);
	} else if (func_item) {
		func_item->gen_sql(sql, dot_flag);
	} else if (rela_item) {
		rela_item->gen_sql(sql, dot_flag);
	} else if (arg_item) {
		bool first = true;
		for (arg_list_t::const_iterator iter = arg_item->begin(); iter != arg_item->end(); ++iter) {
			if (first)
				first = false;
			else
				sql += ", ";
			(*iter)->gen_sql(sql, dot_flag);
		}
	} else if (select_item) {
		sql += select_item->gen_sql();
	} else {
		sql += ":";
	}

	if (field_desc)
		field_desc->gen_sql(sql, dot_flag);

	if (has_curve)
		sql += ")";

	if (order_type == ORDERTYPE_ASC)
		sql += " ASC";
	else if (order_type == ORDERTYPE_DESC)
		sql += " DESC";
}

void composite_item_t::gen_code(gen_code_t& para) const
{
	string& code = para.code;
	const map<string, sql_field_t>& field_map = *para.field_map;

	if (para.row_index == -1) {
		string value;
		gen_sql(value, true);
		map<string, sql_field_t>::const_iterator iter = field_map.find(value);
		if (iter != field_map.end()) {
			ostringstream fmt;
			int offset;
			if (para.use_internal())
				offset = iter->second.internal_offset;
			else
				offset = iter->second.field_offset;

			switch (iter->second.field_type) {
			case SQLTYPE_CHAR:
				fmt << "*reinterpret_cast<char *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_UCHAR:
				fmt << "*reinterpret_cast<unsigned char *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_SHORT:
				fmt << "*reinterpret_cast<short *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_USHORT:
				fmt << "*reinterpret_cast<unsigned short *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_INT:
				fmt << "*reinterpret_cast<int *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_UINT:
				fmt << "*reinterpret_cast<unsigned int *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_LONG:
				fmt << "*reinterpret_cast<long *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_ULONG:
				fmt << "*reinterpret_cast<unsigned long *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_FLOAT:
				fmt << "*reinterpret_cast<float *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_DOUBLE:
				fmt << "*reinterpret_cast<double *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_STRING:
				fmt << "#0 + " << offset;
				break;
			case SQLTYPE_VSTRING:
				fmt << "dbc_vdata(#0, " << offset << ")";
				break;
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				fmt << "*reinterpret_cast<time_t *>(#0 + " << offset << ")";
				break;
			case SQLTYPE_UNKNOWN:
			case SQLTYPE_ANY:
			default:
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unknown type given.")).str(SGLOCALE));
			}
			code += fmt.str();
			return;
		}
	}

	if (has_curve)
		code += "(";

	if (simple_item) {
		simple_item->gen_code(para);
	} else if (func_item) {
		func_item->gen_code(para);
	} else if (rela_item) {
		rela_item->gen_code(para);
	} else if (field_desc) {
		field_desc->gen_code(para);
	} else if (arg_item) {
		bool first = true;
		for (arg_list_t::const_iterator iter = arg_item->begin(); iter != arg_item->end(); ++iter) {
			if (first)
				first = false;
			else
				code += ", ";
			(*iter)->gen_code(para);
		}
	} else if (select_item) {
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: inner select SQL is not supported.")).str(SGLOCALE));
	} else {
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: unknown field name.")).str(SGLOCALE));
	}

	if (has_curve)
		code += ")";
}

void composite_item_t::print() const
{
	cout << print_prefix << "{composite_item\n";
	print_prefix += "  ";

	if (simple_item) {
		simple_item->print();
	} else if (func_item) {
		func_item->print();
	} else if (rela_item) {
		rela_item->print();
	} else if (arg_item) {
		for (arg_list_t::const_iterator iter = arg_item->begin(); iter != arg_item->end(); ++iter)
			(*iter)->print();
	} else if (select_item) {
		select_item->print();
	}

	if (field_desc)
		field_desc->print();

	cout << print_prefix << "order_type = " << order_type << "\n";
	cout << print_prefix << "has_curve = " << has_curve << "\n";

	print_prefix.resize(print_prefix.size() - 2);
	cout << print_prefix << "}composite_item\n";
}

void composite_item_t::push(vector<sql_item_t>& vector_root)
{
	sql_item_t item;
	item.type = ITEMTYPE_COMPOSIT;
	item.data = reinterpret_cast<void *>(this);
	vector_root.push_back(item);

	if (simple_item)
		simple_item->push(vector_root);
	if (func_item)
		func_item->push(vector_root);
	if (rela_item)
		rela_item->push(vector_root);
	if (arg_item) {
		for (arg_list_t::const_iterator iter = arg_item->begin(); iter != arg_item->end(); ++iter)
			(*iter)->push(vector_root);
	}
	if (select_item)
		select_item->push(vector_root);
}

const char * sql_func_t::agg_functions[] = {
	"count", "sum", "min", "max", "avg"
};

sql_func_t::sql_func_t()
{
}

sql_func_t::~sql_func_t()
{
	if (args) {
		for (arg_list_t::iterator iter = args->begin(); iter != args->end(); ++iter)
			delete (*iter);
		delete args;
	}
}

void sql_func_t::gen_sql(string& sql, bool dot_flag) const
{
	sql += func_name;
	sql += "(";
	bool first = true;
	for (arg_list_t::const_iterator iter = args->begin(); iter != args->end(); ++iter) {
		if (first)
			first = false;
		else
			sql += ", ";
		(*iter)->gen_sql(sql, dot_flag);
	}

	sql += ")";
}

void sql_func_t::gen_code(gen_code_t& para) const
{
	string& code = para.code;

	bool is_agg_func = false;
	for (int i = 0; i < sizeof(agg_functions) / sizeof(const char *); i++) {
		if (func_name == agg_functions[i]) {
			is_agg_func = true;
			break;
		}
	}

	if (para.has_agg_func() && is_agg_func)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: nested aggregation functions are not supported.")).str(SGLOCALE));

	if (is_agg_func) {
		para.set_has_agg_func();

		if (func_name == "count") {
			code += "1";
		} else {
			if (args->size() != 1)
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: aggregation function requires only one parameter.")).str(SGLOCALE));

			(*args)[0]->gen_code(para);
		}
	} else {
		code += func_name;
		code += "(";
		bool first = true;
		for (arg_list_t::const_iterator iter = args->begin(); iter != args->end(); ++iter) {
			if (first)
				first = false;
			else
				code += ", ";
			(*iter)->gen_code(para);
		}
		code += ")";
	}
}

void sql_func_t::print() const
{
	cout << print_prefix << "{sql_func\n";
	print_prefix += "  ";

	cout << print_prefix << "func_name = " << func_name << "\n";
	for (arg_list_t::const_iterator iter = args->begin(); iter != args->end(); ++iter)
		(*iter)->print();

	print_prefix.resize(print_prefix.size() - 2);
	cout << print_prefix << "}sql_func\n";
}

void sql_func_t::push(vector<sql_item_t>& vector_root)
{
	sql_item_t item;
	item.type = ITEMTYPE_FUNC;
	item.data = reinterpret_cast<void *>(this);
	vector_root.push_back(item);

	for (arg_list_t::const_iterator iter = args->begin(); iter != args->end(); ++iter)
		(*iter)->push(vector_root);
}

}
}

