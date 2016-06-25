#include "dbc_int_function.h"
#include "dbct_ctx.h"

using namespace ai::sg;
using namespace std;

void dbc_assign(char& result, const char *value)
{
	result = value[0];
}

void dbc_assign(unsigned char& result, const char *value)
{
	result = static_cast<unsigned char>(value[0]);
}

void dbc_assign(short& result, const char *value)
{
	result = static_cast<short>(atoi(value));
}

void dbc_assign(unsigned short& result, const char *value)
{
	result = static_cast<unsigned short>(atoi(value));
}

void dbc_assign(int& result, const char *value)
{
	result = static_cast<int>(atoi(value));
}

void dbc_assign(unsigned int& result, const char *value)
{
	result = static_cast<unsigned int>(atoi(value));
}

void dbc_assign(long& result, const char *value)
{
	result = static_cast<long>(atol(value));
}

void dbc_assign(unsigned long& result, const char *value)
{
	result = static_cast<unsigned long>(atol(value));
}

void dbc_assign(float& result, const char *value)
{
	result = static_cast<float>(atof(value));
}

void dbc_assign(double& result, const char *value)
{
	result = static_cast<double>(atof(value));
}

void dbc_assign(char *result, const char *value, int max_len)
{
	int len = std::min(static_cast<int>(strlen(value)), max_len);
	memcpy(result, value, len);
	result[len] = '\0';
}

void dbc_date_assign(time_t& result, time_t value)
{
	result = value;
}

void dbc_date_assign(time_t& result, const char *value)
{
	date dt(value);
	result = dt.duration();
}

void dbc_time_assign(time_t& result, time_t value)
{
	result = value;
}

void dbc_time_assign(time_t& result, const char *value)
{
	ptime dt(value);
	result = dt.duration();
}

void dbc_datetime_assign(time_t& result, time_t value)
{
	result = value;
}

void dbc_datetime_assign(time_t& result, const char *value)
{
	datetime dt(value);
	result = dt.duration();
}

const char * dbc_vdata(const char *base, int offset)
{
	dbct_ctx *DBCT = dbct_ctx::instance();

	if (!DBCT->_DBCT_internal_format)
		return base + offset;
	else
		return base + DBCT->_DBCT_internal_size + *reinterpret_cast<const int *>(base + offset);
}

int dbc_compare(char *value1, char *value2)
{
	return strcmp(value1, value2);
}

int dbc_compare(const char *value1, const char *value2)
{
	return strcmp(value1, value2);
}

#define dbc_impl_strcat1(type1, type2) \
const char * dbc_strcat(type1 value1, type2 value2) \
{ \
	dbct_ctx *DBCT = dbct_ctx::instance(); \
	ostringstream fmt; \
	fmt << value1 << value2; \
	string str = fmt.str(); \
	if (DBCT->_DBCT_strcat_buf_index >= DBCT->_DBCT_strcat_buf.size()) \
		DBCT->_DBCT_strcat_buf.resize(DBCT->_DBCT_strcat_buf_index + 1); \
	string& result = DBCT->_DBCT_strcat_buf[DBCT->_DBCT_strcat_buf_index++]; \
	result = str; \
	return result.c_str(); \
}

#define dbc_impl_strcat2(type) \
dbc_impl_strcat1(type, char) \
dbc_impl_strcat1(type, unsigned char) \
dbc_impl_strcat1(type, char *) \
dbc_impl_strcat1(type, unsigned char *) \
dbc_impl_strcat1(type, const char *) \
dbc_impl_strcat1(type, const unsigned char *)

#define dbc_impl_strcat \
dbc_impl_strcat2(char) \
dbc_impl_strcat2(unsigned char) \
dbc_impl_strcat2(char *) \
dbc_impl_strcat2(unsigned char *) \
dbc_impl_strcat2(const char *) \
dbc_impl_strcat2(const unsigned char *)

dbc_impl_strcat

#define dbc_impl_op1(function, type1, type2, op) \
bool function(type1 value1, type2 value2) \
{ \
	return value1 op static_cast<type1>(value2[0]); \
}

#define dbc_impl_op2(function, type1, type2, op) \
bool function(type1 value1, type2 value2) \
{ \
	return static_cast<type2>(value1[0]) op value2; \
}

#define dbc_impl_op3(function, type1, type2, op) \
bool function(type1 value1, type2 value2) \
{ \
	return strcmp(reinterpret_cast<const char *>(value1), reinterpret_cast<const char *>(value2)) op 0; \
}

#define dbc_impl_op(function, op) \
dbc_impl_op1(function, char, char *, op) \
dbc_impl_op1(function, char, const char *, op) \
dbc_impl_op1(function, char, unsigned char *, op) \
dbc_impl_op1(function, char, const unsigned char *, op) \
dbc_impl_op1(function, unsigned char, char *, op) \
dbc_impl_op1(function, unsigned char, const char *, op) \
dbc_impl_op1(function, unsigned char, unsigned char *, op) \
dbc_impl_op1(function, unsigned char, const unsigned char *, op) \
dbc_impl_op2(function, char *, char, op) \
dbc_impl_op2(function, const char *, char, op) \
dbc_impl_op2(function, unsigned char *, char, op) \
dbc_impl_op2(function, const unsigned char *, char, op) \
dbc_impl_op2(function, char *, unsigned char, op) \
dbc_impl_op2(function, const char *, unsigned char, op) \
dbc_impl_op2(function, unsigned char *, unsigned char, op) \
dbc_impl_op2(function, const unsigned char *, unsigned char, op) \
dbc_impl_op3(function, char *, char *, op) \
dbc_impl_op3(function, char *, const char *, op) \
dbc_impl_op3(function, char *, unsigned char *, op) \
dbc_impl_op3(function, char *, const unsigned char *, op) \
dbc_impl_op3(function, const char *, char *, op) \
dbc_impl_op3(function, const char *, const char *, op) \
dbc_impl_op3(function, const char *, unsigned char *, op) \
dbc_impl_op3(function, const char *, const unsigned char *, op) \
dbc_impl_op3(function, unsigned char *, char *, op) \
dbc_impl_op3(function, unsigned char *, const char *, op) \
dbc_impl_op3(function, unsigned char *, unsigned char *, op) \
dbc_impl_op3(function, unsigned char *, const unsigned char *, op) \
dbc_impl_op3(function, const unsigned char *, char *, op) \
dbc_impl_op3(function, const unsigned char *, const char *, op) \
dbc_impl_op3(function, const unsigned char *, unsigned char *, op) \
dbc_impl_op3(function, const unsigned char *, const unsigned char *, op)

dbc_impl_op(dbc_gt, >)
dbc_impl_op(dbc_lt, <)
dbc_impl_op(dbc_ge, >=)
dbc_impl_op(dbc_le, <=)
dbc_impl_op(dbc_eq, ==)
dbc_impl_op(dbc_ne, !=)

void dbc_min(char *result, const char *value)
{
	if (strcmp(result, value) > 0)
		strcpy(result, value);
}

void dbc_max(char *result, const char *value)
{
	if (strcmp(result, value) < 0)
		strcpy(result, value);
}

#define dbc_impl_avg(type) \
void dbc_avg(type& result, type value) \
{ \
	dbct_ctx *DBCT = dbct_ctx::instance(); \
	result += (value - result) / DBCT->_DBCT_result_set_size; \
}

dbc_impl_avg(char)
dbc_impl_avg(unsigned char)
dbc_impl_avg(short)
dbc_impl_avg(unsigned short)
dbc_impl_avg(int)
dbc_impl_avg(unsigned int)
dbc_impl_avg(long)
dbc_impl_avg(unsigned long)
dbc_impl_avg(float)
dbc_impl_avg(double)

void dbc_sqlfun_init()
{
	dbct_ctx *DBCT = dbct_ctx::instance();
	DBCT->_DBCT_strcat_buf_index = 0;
}

