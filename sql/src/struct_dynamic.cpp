#include "struct_dynamic.h"
#include "sql_control.h"
#include "sql_extern.h"
#include "sql_select.h"
#include "sql_insert.h"
#include "sql_update.h"
#include "sql_delete.h"
#include <boost/interprocess/sync/scoped_lock.hpp>

extern void yyrestart(FILE *input_file);

namespace ai
{
namespace scci
{

using namespace ai::sg;
namespace bi = boost::interprocess;

extern int inquote;
extern int incomments;
extern string tminline;
extern int line_no;
extern int column;

boost::mutex struct_dynamic::mutex;

struct_dynamic::struct_dynamic(dbtype_enum dbtype_, bool ind_required_, int _size_)
	: dbtype(dbtype_),
	  ind_required(ind_required_),
	  _size(_size_)
{
	select_data = NULL;
	bind_data = NULL;
	select_inds = NULL;
	bind_inds = NULL;
	select_count = 0;
	bind_count = 0;
}

struct_dynamic::~struct_dynamic()
{
	clear();
}

dbtype_enum struct_dynamic::get_dbtype()
{
	return dbtype;
}

void struct_dynamic::setSQL(const string& sql, const vector<column_desc_t>& binds)
{
	int ret;

	if (sql.empty())
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: empty SQL given.")).str(SGLOCALE));

	bi::scoped_lock<boost::mutex> lock(mutex);

	char temp_name[4096];
	const char *tmpdir = ::getenv("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = P_tmpdir;

	sprintf(temp_name, "%s/scciXXXXXX", tmpdir);

	int fd = ::mkstemp(temp_name);
	if (fd == -1)
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: mkstemp() failure, {1}") % ::strerror(errno)).str(SGLOCALE));

	yyin = ::fdopen(fd, "w+");
	if (yyin == NULL) {
		::close(fd);
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: open temp file [{1}] failure, {2}") % temp_name % ::strerror(errno)).str(SGLOCALE));
	}

	if (::fwrite(sql.c_str(), sql.length(), 1, yyin) != 1) {
		::fclose(yyin);
		::unlink(temp_name);
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: write sql to file [{1}] failure, {2}") % temp_name % ::strerror(errno)).str(SGLOCALE));
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
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: Parse failed.")).str(SGLOCALE));
	}

	if (ret == 1) {
		::fclose(yyin);
		::unlink(temp_name);
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: Severe error found. Stop syntax checking.")).str(SGLOCALE));
	}

	if (errcnt > 0) {
		::fclose(yyin);
		::unlink(temp_name);
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: Above errors found during syntax checking.")).str(SGLOCALE));
	}

	::fclose(yyin);
	::unlink(temp_name);

	print_prefix = "";

	sql_statement::set_dbtype(dbtype);
	if (!sql_statement::gen_data_dynamic(this)) {
		sql_statement::clear();
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: Parse failed, see LOG for more information.")).str(SGLOCALE));
	}

	select_count = select_fields.size() - 1;
	bind_count = bind_fields.size() - 1;

	alloc_select();
	alloc_bind(binds);
}

int struct_dynamic::size() const
{
	return _size;
}

void struct_dynamic::alloc_select()
{
	// Will be delayed to alloc in prepare().
	if (select_count <= 0 || select_fields[0].field_type == SQLTYPE_ANY)
		return;

	select_data = new char *[select_count];
	::memset(select_data, '\0', sizeof(char *) * select_count);
	if (ind_required) {
		select_inds = new char *[select_count];
		::memset(select_inds, '\0', sizeof(char *) * select_count);
	}
	for (int i = 0; i < select_count; i++) {
		select_data[i] = new char[select_fields[i].field_length * _size];
		// Adjust offset.
		select_fields[i].field_offset = select_data[i] - reinterpret_cast<char *>(this);

		if (ind_required) {
			select_inds[i] = new char[select_fields[i].ind_length * _size];
			// Adjust offset.
			select_fields[i].ind_offset = select_inds[i] - reinterpret_cast<char *>(this);
		}
	}
}

void struct_dynamic::alloc_bind(const vector<column_desc_t>& binds)
{
	if (bind_count <= 0)
		return;

	if (!binds.empty() && bind_count != binds.size())
		throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: Bind array given doesn't match the size of bind fields.")).str(SGLOCALE));

	bind_data = new char *[bind_count];
	::memset(bind_data, '\0', sizeof(char *) * bind_count);
	if (ind_required) {
		bind_inds = new char *[bind_count];
		::memset(bind_inds, '\0', sizeof(char *) * bind_count);
	}

	for (int i = 0; i < bind_count; i++) {
		if (bind_fields[i].field_type == SQLTYPE_ANY) {
			if (binds.empty())
				throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: Bind array should be provided for ANY type binding")).str(SGLOCALE));
			bind_fields[i].field_type = binds[i].field_type;
			switch (binds[i].field_type) {
			case SQLTYPE_CHAR:
				bind_fields[i].field_length = sizeof(char);
				break;
			case SQLTYPE_UCHAR:
				bind_fields[i].field_length = sizeof(unsigned char);
				break;
			case SQLTYPE_SHORT:
				bind_fields[i].field_length = sizeof(short);
				break;
			case SQLTYPE_USHORT:
				bind_fields[i].field_length = sizeof(unsigned short);
				break;
			case SQLTYPE_INT:
				bind_fields[i].field_length = sizeof(int);
				break;
			case SQLTYPE_UINT:
				bind_fields[i].field_length = sizeof(unsigned int);
				break;
			case SQLTYPE_LONG:
				bind_fields[i].field_length = sizeof(long);
				break;
			case SQLTYPE_ULONG:
				bind_fields[i].field_length = sizeof(unsigned long);
				break;
			case SQLTYPE_FLOAT:
				bind_fields[i].field_length = sizeof(float);
				break;
			case SQLTYPE_DOUBLE:
				bind_fields[i].field_length = sizeof(double);
				break;
			case SQLTYPE_STRING:
			case SQLTYPE_VSTRING:
				bind_fields[i].field_length = binds[i].field_length + 1;
				break;
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				bind_fields[i].field_length = binds[i].field_length;
				break;
			default:
				throw bad_sql(__FILE__, __LINE__, 0, 0, (_("ERROR: Sql type unrecognized.")).str(SGLOCALE));
			}
		}

		bind_data[i] = new char[bind_fields[i].field_length * _size];
		// Adjust offset.
		bind_fields[i].field_offset = bind_data[i] - reinterpret_cast<char *>(this);
		if (ind_required) {
			bind_inds[i] = new char[bind_fields[i].ind_length * _size];
			// Adjust offset.
			bind_fields[i].ind_offset = bind_inds[i] - reinterpret_cast<char *>(this);
		}
	}
}

void struct_dynamic::clear()
{
	int i;

	if (bind_data) {
		for (i = 0; i < bind_count; i++) {
			delete []bind_data[i];
			if (ind_required)
				delete []bind_inds[i];
		}
		delete []bind_data;
		delete []bind_inds;
		bind_data = NULL;
		bind_inds = NULL;
	}

	if (select_data) {
		for (i = 0; i < select_count; i++) {
			delete []select_data[i];
			if (ind_required)
				delete []select_inds[i];
		}
		delete []select_data;
		delete []select_inds;
		select_data = NULL;
		select_inds = NULL;
	}

	select_fields.clear();
	bind_fields.clear();
	table_fields.clear();

	select_count = 0;
	bind_count = 0;
}

bind_field_t * struct_dynamic::getSelectFields(int index)
{
	return &select_fields[0];
}

bind_field_t * struct_dynamic::getBindFields(int index)
{
	return &bind_fields[0];
}

string& struct_dynamic::getSQL(int index)
{
	return _sql;
}

table_field_t * struct_dynamic::getTableFields(int index)
{
	return &table_fields[index];
}

}
}

