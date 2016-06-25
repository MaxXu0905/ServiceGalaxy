#include "sql_control.h"
#include "gpp_ctx.h"

namespace ai
{
namespace scci
{

using namespace ai::sg;

bad_sql::bad_sql(const string& file, int line, int sys_code, int error_code, const string& message) throw()
{
	error_code_ = error_code;
	error_msg_ = message;
	what_ = (_("ERROR: In file {1}:{2}: error code: {3}, error message: {4}") % file % line % error_code_ % error_msg_).str(SGLOCALE);
}

bad_sql::~bad_sql() throw()
{
}

int bad_sql::get_error_code() const throw()
{
	return error_code_;
}

const char * bad_sql::error_msg() const throw()
{
    return error_msg_.c_str();
}

const char * bad_sql::what () const throw()
{
    return what_.c_str();
}

Generic_Database::Generic_Database()
{
}

Generic_Database::~Generic_Database()
{
}

struct_dynamic * Generic_Database::create_data(bool ind_required, int size)
{
	return new struct_dynamic(get_dbtype(), ind_required, size);
}

void Generic_Database::terminate_data(struct_dynamic *& data)
{
	delete data;
	data = NULL;
}

string Generic_Database::get_seq_item(const string& seq_name, int seq_action)
{
	string seq_item;

	switch (seq_action) {
	case SEQ_ACTION_CURRVAL:
		seq_item = seq_name;
		seq_item += ".currval";
		break;
	case SEQ_ACTION_NEXTVAL:
		seq_item = seq_name;
		seq_item += ".nextval";
		break;
	default:
		return "";
	}

	return seq_item;
}

Generic_Statement::Generic_Statement()
{
	iteration = 0;
	maxIterations = 0;
	data = NULL;
	select_fields = NULL;
	bind_fields = NULL;
	sort_buf = NULL;
}

Generic_Statement::~Generic_Statement()
{
	if (sort_buf) {
		for (int i = 0; i < bind_count; i++)
			delete []sort_buf[i];
		delete []sort_buf;
	}
}

struct_dynamic * Generic_Statement::create_data(bool ind_required, int size)
{
	return new struct_dynamic(get_dbtype(), ind_required, size);
}

void Generic_Statement::terminate_data(struct_dynamic *& data)
{
	delete data;
	data = NULL;
}

void Generic_Statement::setTable(const string& newTable_, const string& oldTable_)
{
	newTable = newTable_;
	oldTable = oldTable_;
}

void Generic_Statement::setMaxIterations(int maxIterations_)
{
	maxIterations = maxIterations_;
}

int Generic_Statement::getMaxIterations() const
{
	return maxIterations;
}

void Generic_Statement::resetIteration()
{
	iteration = 0;
}

void Generic_Statement::addIteration()
{
	++iteration;
}

int Generic_Statement::getCurrentIteration() const
{
	return iteration;
}

void Generic_Statement::copy_bind(int dst_idx, const Generic_Statement& src_stmt, int src_idx)
{
	for (int i = 0; i < bind_count; i++) {
 		const bind_field_t& field = bind_fields[i];
		const bind_field_t& src_field = src_stmt.bind_fields[i];
		char *dst_data = reinterpret_cast<char *>(data) + field.field_offset + field.field_length * dst_idx;
		char *src_data = reinterpret_cast<char *>(src_stmt.data) + src_field.field_offset + src_field.field_length * src_idx;

		if (dst_data != src_data)
			memcpy(dst_data, src_data, field.field_length);

		if (field.ind_offset != -1) {
			char *dst_ind = reinterpret_cast<char *>(data) + field.ind_offset + field.ind_length * dst_idx;
			char *src_ind = reinterpret_cast<char *>(src_stmt.data) + src_field.ind_offset + src_field.ind_length * src_idx;

			if (dst_ind != src_ind)
				memcpy(dst_ind, src_ind, field.ind_length);
		}
	}
}

void Generic_Statement::sortArray(int arrayLength, const vector<int>& keys, int *sort_array)
{
	if (sort_buf == NULL) {
		sort_buf = new char *[bind_count];
		for (int i = 0; i < bind_count; i++) {
			const bind_field_t& field = bind_fields[i];
			sort_buf[i] = new char[field.field_length];
		}
	}

	for (int i = 0; i < arrayLength; i++)
		sort_array[i] = i;

	qsort(0, arrayLength - 1, keys, sort_array);

	for (int i = 0; i < arrayLength; i++) {
		if (i != sort_array[i])
			sort_array[i] = -sort_array[i] - 1;
	}

	for (int i = 0; i < arrayLength; i++) {
		if (sort_array[i] >= 0)
			continue;

		sort_array[i] = -sort_array[i] - 1;
		int pidx = sort_array[i];

		for (int j = 0; j < bind_count; j++) {
			const bind_field_t& field = bind_fields[j];
			char *addr = reinterpret_cast<char *>(data) + field.field_offset;

			memcpy(sort_buf[j], addr + i * field.field_length, field.field_length);
			memcpy(addr + i * field.field_length, addr + pidx * field.field_length, field.field_length);
		}

		while (1) {
			sort_array[pidx] = -sort_array[pidx] - 1;
			int nidx = sort_array[pidx];

			if (nidx == i) {
				for (int j = 0; j < bind_count; j++) {
					const bind_field_t& field = bind_fields[j];
					char *addr = reinterpret_cast<char *>(data) + field.field_offset;

					memcpy(addr + pidx * field.field_length, sort_buf[j], field.field_length);
				}
				break;
			} else {
				for (int j = 0; j < bind_count; j++) {
					const bind_field_t& field = bind_fields[j];
					char *addr = reinterpret_cast<char *>(data) + field.field_offset;

					memcpy(addr + pidx * field.field_length, addr + nidx * field.field_length, field.field_length);
				}
			}

			pidx = nidx;
		}
	}
}

void Generic_Statement::sort(const vector<int>& keys, int *sort_array)
{
	sortArray(iteration + 1, keys, sort_array);
}

char Generic_Statement::getChar(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<char>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<char>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<char>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<char>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<char>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<char>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<char>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<char>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<char>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<char>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<char>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

unsigned char Generic_Statement::getUChar(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<unsigned char>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<unsigned char>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<unsigned char>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<unsigned char>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<unsigned char>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<unsigned char>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<unsigned char>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<unsigned char>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<unsigned char>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<unsigned char>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<unsigned char>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

short Generic_Statement::getShort(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<short>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<short>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<short>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<short>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<short>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<short>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<short>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<short>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<short>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<short>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<short>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

unsigned short Generic_Statement::getUShort(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<unsigned short>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<unsigned short>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<unsigned short>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<unsigned short>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<unsigned short>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<unsigned short>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<unsigned short>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<unsigned short>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<unsigned short>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<unsigned short>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<unsigned short>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

int Generic_Statement::getInt(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<int>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<int>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<int>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<int>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<int>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<int>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<int>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<int>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<int>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<int>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<int>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<int>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<int>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<int>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

unsigned int Generic_Statement::getUInt(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<unsigned int>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<unsigned int>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<unsigned int>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<unsigned int>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<unsigned int>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<unsigned int>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<unsigned int>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<unsigned int>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<unsigned int>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<unsigned int>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<unsigned int>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<unsigned int>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<unsigned int>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<unsigned int>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

long Generic_Statement::getLong(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<long>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<long>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<long>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<long>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<long>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<long>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<long>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<long>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<long>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<long>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<long>(::atol(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<long>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<long>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<long>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

unsigned long Generic_Statement::getULong(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<unsigned long>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<unsigned long>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<unsigned long>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<unsigned long>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<unsigned long>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<unsigned long>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<unsigned long>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<unsigned long>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<unsigned long>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<unsigned long>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<unsigned long>(::atol(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<unsigned long>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<unsigned long>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<unsigned long>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

float Generic_Statement::getFloat(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<float>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<float>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<float>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<float>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<float>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<float>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<float>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<float>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<float>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<float>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<float>(::atof(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<float>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<float>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<float>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

double Generic_Statement::getDouble(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<double>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<double>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<double>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<double>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<double>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<double>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<double>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<double>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<double>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<double>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<double>(::atof(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<double>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<double>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<double>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

string Generic_Statement::getString(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	fmt.str("");
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		fmt << *reinterpret_cast<char *>(addr);
		break;
	case SQLTYPE_UCHAR:
		fmt << *reinterpret_cast<unsigned char *>(addr);
		break;
	case SQLTYPE_SHORT:
		fmt << *reinterpret_cast<short *>(addr);
		break;
	case SQLTYPE_USHORT:
		fmt << *reinterpret_cast<unsigned short *>(addr);
		break;
	case SQLTYPE_INT:
		fmt << *reinterpret_cast<int *>(addr);
		break;
	case SQLTYPE_UINT:
		fmt << *reinterpret_cast<unsigned int *>(addr);
		break;
	case SQLTYPE_LONG:
		fmt << *reinterpret_cast<long *>(addr);
		break;
	case SQLTYPE_ULONG:
		fmt << *reinterpret_cast<unsigned long *>(addr);
		break;
	case SQLTYPE_FLOAT:
		fmt << *reinterpret_cast<float *>(addr);
		break;
	case SQLTYPE_DOUBLE:
		fmt << *reinterpret_cast<double *>(addr);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return string(reinterpret_cast<char *>(addr));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			string str;
			dt.iso_string(str);
			return str;
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			string str;
			tm.iso_string(str);
			return str;
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			string str;
			dt.iso_string(str);
			return str;
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}

	return fmt.str();
}

date Generic_Statement::getDate(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
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
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
	case SQLTYPE_TIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_DATE:
		return date2native(addr);
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return date(dt.date());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

ptime Generic_Statement::getTime(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
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
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
	case SQLTYPE_DATE:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_TIME:
		return time2native(addr);
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return ptime(dt.time());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

datetime Generic_Statement::getDatetime(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
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
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
	case SQLTYPE_TIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return datetime(dt.duration());
		}
	case SQLTYPE_DATETIME:
		return datetime2native(addr);
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

sql_datetime Generic_Statement::getSQLDatetime(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
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
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_DATETIME:
		return sqldatetime2native(addr);
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setChar(int paramIndex, char x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setUChar(int paramIndex, unsigned char x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setShort(int paramIndex, short x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setUShort(int paramIndex, unsigned short x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setInt(int paramIndex, int x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
		native2date(addr, date(static_cast<time_t>(x)));
		break;
	case SQLTYPE_TIME:
		native2time(addr, ptime(static_cast<time_t>(x)));
		break;
	case SQLTYPE_DATETIME:
		native2datetime(addr, datetime(static_cast<time_t>(x)));
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setUInt(int paramIndex, unsigned int x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
		native2date(addr, date(static_cast<time_t>(x)));
		break;
	case SQLTYPE_TIME:
		native2time(addr, ptime(static_cast<time_t>(x)));
		break;
	case SQLTYPE_DATETIME:
		native2datetime(addr, datetime(static_cast<time_t>(x)));
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setLong(int paramIndex, long x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
		native2date(addr, date(static_cast<time_t>(x)));
		break;
	case SQLTYPE_TIME:
		native2time(addr, ptime(static_cast<time_t>(x)));
		break;
	case SQLTYPE_DATETIME:
		native2datetime(addr, datetime(static_cast<time_t>(x)));
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setULong(int paramIndex, unsigned long x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
		native2date(addr, date(static_cast<time_t>(x)));
		break;
	case SQLTYPE_TIME:
		native2time(addr, ptime(static_cast<time_t>(x)));
		break;
	case SQLTYPE_DATETIME:
		native2datetime(addr, datetime(static_cast<time_t>(x)));
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setFloat(int paramIndex, float x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
		native2date(addr, date(static_cast<time_t>(x)));
		break;
	case SQLTYPE_TIME:
		native2time(addr, ptime(static_cast<time_t>(x)));
		break;
	case SQLTYPE_DATETIME:
		native2datetime(addr, datetime(static_cast<time_t>(x)));
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setDouble(int paramIndex, double x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(x);
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(x);
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(x);
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(x);
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x);
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x);
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x);
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x);
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(x);
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(x);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		fmt.str("");
		fmt << x;
		::strcpy(addr, fmt.str().c_str());
		break;
	case SQLTYPE_DATE:
		native2date(addr, date(static_cast<time_t>(x)));
		break;
	case SQLTYPE_TIME:
		native2time(addr, ptime(static_cast<time_t>(x)));
		break;
	case SQLTYPE_DATETIME:
		native2datetime(addr, datetime(static_cast<time_t>(x)));
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setString(int paramIndex, const string& x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		*reinterpret_cast<char *>(addr) = static_cast<char>(::atoi(x.c_str()));
		break;
	case SQLTYPE_UCHAR:
		*reinterpret_cast<unsigned char *>(addr) = static_cast<unsigned char>(::atoi(x.c_str()));
		break;
	case SQLTYPE_SHORT:
		*reinterpret_cast<short *>(addr) = static_cast<short>(::atoi(x.c_str()));
		break;
	case SQLTYPE_USHORT:
		*reinterpret_cast<unsigned short *>(addr) = static_cast<unsigned short>(::atoi(x.c_str()));
		break;
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(::atoi(x.c_str()));
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(::atoi(x.c_str()));
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(::atol(x.c_str()));
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(::atol(x.c_str()));
		break;
	case SQLTYPE_FLOAT:
		*reinterpret_cast<float *>(addr) = static_cast<float>(::atof(x.c_str()));
		break;
	case SQLTYPE_DOUBLE:
		*reinterpret_cast<double *>(addr) = static_cast<double>(::atof(x.c_str()));
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		::strcpy(addr, x.c_str());
		break;
	case SQLTYPE_DATE:
		native2date(addr, date(x));
		break;
	case SQLTYPE_TIME:
		native2time(addr, ptime(x));
		break;
	case SQLTYPE_DATETIME:
		native2datetime(addr, datetime(x));
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setDate(int paramIndex, const date& x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
	case SQLTYPE_UCHAR:
	case SQLTYPE_SHORT:
	case SQLTYPE_USHORT:
	case SQLTYPE_FLOAT:
	case SQLTYPE_DOUBLE:
	case SQLTYPE_TIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x.duration());
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x.duration());
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x.duration());
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x.duration());
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		{
			string str;
			x.iso_string(str);
			::strcpy(addr, str.c_str());
			break;
		}
	case SQLTYPE_DATE:
		native2date(addr, x);
		break;
	case SQLTYPE_DATETIME:
		{
			datetime dt(x.duration());
			native2datetime(addr, dt);
			break;
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setTime(int paramIndex, const ptime& x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
	case SQLTYPE_UCHAR:
	case SQLTYPE_SHORT:
	case SQLTYPE_USHORT:
	case SQLTYPE_FLOAT:
	case SQLTYPE_DOUBLE:
	case SQLTYPE_DATE:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x.duration());
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x.duration());
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x.duration());
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x.duration());
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		{
			string str;
			x.iso_string(str);
			::strcpy(addr, str.c_str());
			break;
		}
	case SQLTYPE_TIME:
		native2time(addr, x);
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setDatetime(int paramIndex, const datetime& x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
	case SQLTYPE_UCHAR:
	case SQLTYPE_SHORT:
	case SQLTYPE_USHORT:
	case SQLTYPE_FLOAT:
	case SQLTYPE_DOUBLE:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_INT:
		*reinterpret_cast<int *>(addr) = static_cast<int>(x.duration());
		break;
	case SQLTYPE_UINT:
		*reinterpret_cast<unsigned int *>(addr) = static_cast<unsigned int>(x.duration());
		break;
	case SQLTYPE_LONG:
		*reinterpret_cast<long *>(addr) = static_cast<long>(x.duration());
		break;
	case SQLTYPE_ULONG:
		*reinterpret_cast<unsigned long *>(addr) = static_cast<unsigned long>(x.duration());
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		{
			string str;
			x.iso_string(str);
			::strcpy(addr, str.c_str());
			break;
		}
	case SQLTYPE_DATE:
		{
			date dt(x.year(), x.month(), x.day());
			native2date(addr, dt);
			break;
		}
	case SQLTYPE_TIME:
		{
			ptime tm(x.hour(), x.minute(), x.second());
			native2time(addr, tm);
			break;
		}
	case SQLTYPE_DATETIME:
		native2datetime(addr, x);
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::setSQLDatetime(int paramIndex, const sql_datetime& x)
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	const bind_field_t& field = bind_fields[paramIndex - 1];
	if (field.ind_offset != -1) // No indicator
		clearNull(reinterpret_cast<char *>(data) + field.ind_offset);

	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
	case SQLTYPE_UCHAR:
	case SQLTYPE_SHORT:
	case SQLTYPE_USHORT:
	case SQLTYPE_FLOAT:
	case SQLTYPE_DOUBLE:
	case SQLTYPE_INT:
	case SQLTYPE_UINT:
	case SQLTYPE_LONG:
	case SQLTYPE_ULONG:
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		{
			string str;
			x.iso_string(str);
			::strcpy(addr, str.c_str());
			break;
		}
	case SQLTYPE_DATETIME:
		native2sqldatetime(addr, x);
		break;
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

void Generic_Statement::bind(struct_base *data_, int index_)
{
	if (data_->get_dbtype() != get_dbtype())
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: dbtype doesn't match, expected {1}, but struct_base is {2}") % get_dbtype() % data_->get_dbtype()).str(SGLOCALE));

	const bind_field_t *ptr;

	data = data_;
	index = index_;

	select_fields = data->getSelectFields(index);
	select_count = 0;
	for (ptr = select_fields; ptr->field_type != SQLTYPE_UNKNOWN; ptr++)
		select_count++;

	bind_fields = data->getBindFields(index);
	bind_count = 0;
	for (ptr = bind_fields; ptr->field_type != SQLTYPE_UNKNOWN; ptr++)
		bind_count++;

	iteration = 0;
	maxIterations = data->size();

	prepared = false;
	prepare();
}

const string& Generic_Statement::getColumnName(int colIndex) const
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	return select_fields[colIndex - 1].field_name;
}

int Generic_Statement::getColumnCount() const
{
	return select_count;
}

sqltype_enum Generic_Statement::getColumnType(int colIndex) const
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	return select_fields[colIndex - 1].field_type;
}

const string& Generic_Statement::getBindName(int paramIndex) const
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	return bind_fields[paramIndex - 1].field_name;
}

int Generic_Statement::getBindCount() const
{
	return bind_count;
}

sqltype_enum Generic_Statement::getBindType(int paramIndex) const
{
	if (paramIndex <= 0 || paramIndex > bind_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: paramIndex {1} out of range") % paramIndex).str(SGLOCALE));

	return bind_fields[paramIndex - 1].field_type;
}

void Generic_Statement::qsort(int lower, int upper, const vector<int>& keys, int *sort_array)
{
	int i;
	int last;

	if (lower >= upper)
		return;

	std::swap(sort_array[lower], sort_array[(lower + upper) / 2]);
	last = lower;
	for (i = lower + 1; i <= upper; i++) {
		if (compare(sort_array[lower], sort_array[i], keys) < 0) {
			++last;
			std::swap(sort_array[last], sort_array[i]);
		}
	}

	std::swap(sort_array[lower], sort_array[last]);
	qsort(lower, last - 1, keys, sort_array);
	qsort(last + 1, upper, keys, sort_array);
}

long Generic_Statement::compare(int first, int second, const vector<int>& keys)
{
	BOOST_FOREACH(const int& key, keys) {
		const bind_field_t& field = bind_fields[key];
		char *field_addr = reinterpret_cast<char *>(data) + field.field_offset;
		char *first_addr = field_addr + first * field.field_length;
		char *second_addr = field_addr + second * field.field_length;
		long result;

		switch (field.field_type) {
		case SQLTYPE_CHAR:
			result = static_cast<long>(*reinterpret_cast<char *>(first_addr) - *reinterpret_cast<char *>(second_addr));
			break;
		case SQLTYPE_UCHAR:
			result = static_cast<long>(*reinterpret_cast<unsigned char *>(first_addr) - *reinterpret_cast<unsigned char *>(second_addr));
			break;
		case SQLTYPE_SHORT:
			result = static_cast<long>(*reinterpret_cast<short *>(first_addr) - *reinterpret_cast<short *>(second_addr));
			break;
		case SQLTYPE_USHORT:
			result = static_cast<long>(*reinterpret_cast<unsigned short *>(first_addr) - *reinterpret_cast<unsigned short *>(second_addr));
			break;
		case SQLTYPE_INT:
			result = static_cast<long>(*reinterpret_cast<int *>(first_addr) - *reinterpret_cast<int *>(second_addr));
			break;
		case SQLTYPE_UINT:
			result = static_cast<long>(*reinterpret_cast<unsigned int *>(first_addr) - *reinterpret_cast<unsigned int *>(second_addr));
			break;
		case SQLTYPE_LONG:
			result = static_cast<long>(*reinterpret_cast<long *>(first_addr) - *reinterpret_cast<long *>(second_addr));
			break;
		case SQLTYPE_ULONG:
			result = static_cast<long>(*reinterpret_cast<unsigned long *>(first_addr) - *reinterpret_cast<unsigned long *>(second_addr));
			break;
		case SQLTYPE_FLOAT:
			result = static_cast<long>(*reinterpret_cast<float *>(first_addr) - *reinterpret_cast<float *>(second_addr));
			break;
		case SQLTYPE_DOUBLE:
			result = static_cast<long>(*reinterpret_cast<double *>(first_addr) - *reinterpret_cast<double *>(second_addr));
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			result = static_cast<long>(strcmp(first_addr, second_addr));
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			result = static_cast<long>(*reinterpret_cast<time_t *>(first_addr) - *reinterpret_cast<time_t *>(second_addr));
			break;
		default:
			result = 0L;
			break;
		}

		if (result != 0L)
			return result;
	}

	return 0L;
}

Generic_ResultSet::Generic_ResultSet()
{
	data = NULL;
	select_fields = NULL;
}

Generic_ResultSet::~Generic_ResultSet()
{
}

void Generic_ResultSet::bind(struct_base *data_, int index_)
{
	data = data_;
	index = index_;

	select_fields = data->getSelectFields(index);
	select_count = 0;
	for (const bind_field_t *ptr = select_fields; ptr->field_type != SQLTYPE_UNKNOWN; ptr++)
		select_count++;
}

Generic_ResultSet::Status Generic_ResultSet::status() const
{
	return _status;
}

const string& Generic_ResultSet::getColumnName(int colIndex) const
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	return select_fields[colIndex - 1].field_name;
}

int Generic_ResultSet::getColumnCount() const
{
	return select_count;
}

sqltype_enum Generic_ResultSet::getColumnType(int colIndex) const
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	return select_fields[colIndex - 1].field_type;
}

char Generic_ResultSet::getChar(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<char>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<char>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<char>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<char>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<char>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<char>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<char>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<char>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<char>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<char>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<char>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

unsigned char Generic_ResultSet::getUChar(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<unsigned char>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<unsigned char>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<unsigned char>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<unsigned char>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<unsigned char>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<unsigned char>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<unsigned char>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<unsigned char>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<unsigned char>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<unsigned char>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<unsigned char>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

short Generic_ResultSet::getShort(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<short>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<short>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<short>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<short>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<short>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<short>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<short>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<short>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<short>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<short>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<short>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

unsigned short Generic_ResultSet::getUShort(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<unsigned short>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<unsigned short>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<unsigned short>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<unsigned short>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<unsigned short>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<unsigned short>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<unsigned short>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<unsigned short>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<unsigned short>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<unsigned short>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<unsigned short>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
	case SQLTYPE_DATETIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

int Generic_ResultSet::getInt(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<int>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<int>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<int>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<int>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<int>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<int>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<int>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<int>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<int>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<int>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<int>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<int>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<int>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<int>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

unsigned int Generic_ResultSet::getUInt(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<unsigned int>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<unsigned int>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<unsigned int>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<unsigned int>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<unsigned int>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<unsigned int>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<unsigned int>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<unsigned int>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<unsigned int>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<unsigned int>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<unsigned int>(::atoi(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<unsigned int>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<unsigned int>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<unsigned int>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

long Generic_ResultSet::getLong(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<long>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<long>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<long>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<long>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<long>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<long>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<long>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<long>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<long>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<long>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<long>(::atol(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<long>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<long>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<long>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

unsigned long Generic_ResultSet::getULong(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<unsigned long>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<unsigned long>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<unsigned long>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<unsigned long>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<unsigned long>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<unsigned long>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<unsigned long>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<unsigned long>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<unsigned long>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<unsigned long>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<unsigned long>(::atol(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<unsigned long>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<unsigned long>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<unsigned long>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

float Generic_ResultSet::getFloat(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<float>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<float>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<float>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<float>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<float>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<float>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<float>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<float>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<float>(*reinterpret_cast<float *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<float>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<float>(::atof(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<float>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<float>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<float>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

double Generic_ResultSet::getDouble(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		return static_cast<double>(*reinterpret_cast<char *>(addr));
	case SQLTYPE_UCHAR:
		return static_cast<double>(*reinterpret_cast<unsigned char *>(addr));
	case SQLTYPE_SHORT:
		return static_cast<double>(*reinterpret_cast<short *>(addr));
	case SQLTYPE_USHORT:
		return static_cast<double>(*reinterpret_cast<unsigned short *>(addr));
	case SQLTYPE_INT:
		return static_cast<double>(*reinterpret_cast<int *>(addr));
	case SQLTYPE_UINT:
		return static_cast<double>(*reinterpret_cast<unsigned int *>(addr));
	case SQLTYPE_LONG:
		return static_cast<double>(*reinterpret_cast<long *>(addr));
	case SQLTYPE_ULONG:
		return static_cast<double>(*reinterpret_cast<unsigned long *>(addr));
	case SQLTYPE_FLOAT:
		return static_cast<double>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_DOUBLE:
		return static_cast<double>(*reinterpret_cast<double *>(addr));
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return static_cast<double>(::atof(reinterpret_cast<char *>(addr)));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return static_cast<double>(dt.duration());
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			return static_cast<double>(tm.duration());
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return static_cast<double>(dt.duration());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

string Generic_ResultSet::getString(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
	fmt.str("");
	switch (field.field_type) {
	case SQLTYPE_CHAR:
		fmt << *reinterpret_cast<char *>(addr);
		break;
	case SQLTYPE_UCHAR:
		fmt << *reinterpret_cast<unsigned char *>(addr);
		break;
	case SQLTYPE_SHORT:
		fmt << *reinterpret_cast<short *>(addr);
		break;
	case SQLTYPE_USHORT:
		fmt << *reinterpret_cast<unsigned short *>(addr);
		break;
	case SQLTYPE_INT:
		fmt << *reinterpret_cast<int *>(addr);
		break;
	case SQLTYPE_UINT:
		fmt << *reinterpret_cast<unsigned int *>(addr);
		break;
	case SQLTYPE_LONG:
		fmt << *reinterpret_cast<long *>(addr);
		break;
	case SQLTYPE_ULONG:
		fmt << *reinterpret_cast<unsigned long *>(addr);
		break;
	case SQLTYPE_FLOAT:
		fmt << *reinterpret_cast<float *>(addr);
		break;
	case SQLTYPE_DOUBLE:
		fmt << *reinterpret_cast<double *>(addr);
		break;
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
		return string(reinterpret_cast<char *>(addr));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			string str;
			dt.iso_string(str);
			return str;
		}
	case SQLTYPE_TIME:
		{
			ptime tm = time2native(addr);
			string str;
			tm.iso_string(str);
			return str;
		}
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			string str;
			dt.iso_string(str);
			return str;
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}

	return fmt.str();
}

date Generic_ResultSet::getDate(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
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
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
	case SQLTYPE_TIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_DATE:
		return date2native(addr);
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return date(dt.date());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

ptime Generic_ResultSet::getTime(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
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
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
	case SQLTYPE_DATE:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_TIME:
		return time2native(addr);
	case SQLTYPE_DATETIME:
		{
			datetime dt = datetime2native(addr);
			return ptime(dt.time());
		}
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

datetime Generic_ResultSet::getDatetime(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
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
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
	case SQLTYPE_TIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_DATE:
		{
			date dt = date2native(addr);
			return datetime(dt.duration());
		}
	case SQLTYPE_DATETIME:
		return datetime2native(addr);
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

sql_datetime Generic_ResultSet::getSQLDatetime(int colIndex)
{
	if (colIndex <= 0 || colIndex > select_count)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: colIndex {1} out of range") % colIndex).str(SGLOCALE));

	const bind_field_t& field = select_fields[colIndex - 1];
	char *addr = reinterpret_cast<char *>(data) + field.field_offset + iteration * field.field_length;
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
	case SQLTYPE_STRING:
	case SQLTYPE_VSTRING:
	case SQLTYPE_DATE:
	case SQLTYPE_TIME:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unsupported field type conversion")).str(SGLOCALE));
	case SQLTYPE_DATETIME:
		return sqldatetime2native(addr);
	default:
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: unknown field type provided")).str(SGLOCALE));
	}
}

database_factory database_factory::_instance;

database_factory& database_factory::instance()
{
	return _instance;
}

database_factory::~database_factory()
{
}

Generic_Database * database_factory::create(const string& db_name)
{
	bool found = false;
	BOOST_FOREACH(const database_creator_t& v, _instance.database_creators) {
		if (strcasecmp(v.db_name.c_str(), db_name.c_str()) == 0) {
			try {
				return (*v.create)();
			} catch (exception& ex) {
				throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Failed to create database class for {1}, {2}") % db_name % ex.what()).str(SGLOCALE));
			}

			found = true;
			break;
		}
	}

	throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find database class for {1}") % db_name).str(SGLOCALE));
}

void database_factory::parse(const std::string& openinfo, std::map<string, string>& conn_info)
{
	boost::escaped_list_separator<char> sep('\\', ';', '\"');
	boost::tokenizer<boost::escaped_list_separator<char> > tok(openinfo, sep);

	for (boost::tokenizer<boost::escaped_list_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter) {
		string str = *iter;

		string::size_type pos = str.find('=');
		if (pos == string::npos)
			continue;

		string key(str.begin(), str.begin() + pos);
		string value(str.begin() + pos + 1, str.end());

		if (key.empty())
			continue;

		if (key[0] == '@') {
			char text[4096];
			common::decrypt(value.c_str(), text);
			value = text;
			key.assign(key.begin() + 1, key.end());
		}

		conn_info[key] = value;
	}
}

void database_factory::push_back(const database_creator_t& item)
{
	BOOST_FOREACH(const database_creator_t& v, database_creators) {
		if (v.db_name == item.db_name) {
			gpp_ctx *GPP = gpp_ctx::instance();
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Duplicate database creator for {1}") % item.db_name).str(SGLOCALE));
			exit(1);
		}
	}

	database_creators.push_back(item);
}

database_factory::database_factory()
{
}

}
}

