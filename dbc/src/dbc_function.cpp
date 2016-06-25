#include "dbc_function.h"
#include "common.h"
#include "dbct_ctx.h"

using namespace ai::sg;

string& get_strcat_buf()
{
	dbct_ctx *DBCT = dbct_ctx::instance();
	if (DBCT->_DBCT_strcat_buf_index >= DBCT->_DBCT_strcat_buf.size())
		DBCT->_DBCT_strcat_buf.resize(DBCT->_DBCT_strcat_buf_index + 1);
	return DBCT->_DBCT_strcat_buf[DBCT->_DBCT_strcat_buf_index++];
}

const char * to_char(time_t t, const char *format)
{
	string& result = get_strcat_buf();

	string dts;
	datetime dt(t);
	dt.iso_string(dts);
	if (strcasecmp(format, "YYYYMMDDHH24MISS") == 0)
		result = dts;
	else if (strcasecmp(format, "YYYYMMDD") == 0)
		result = dts.substr(0, 8);
	else if (strcasecmp(format, "HH24MISS") == 0)
		result = dts.substr(8);
	else
		result = "";

	return result.c_str();
}

time_t to_date(const char *dts, const char *format)
{
	if (strcasecmp(format, "YYYYMMDDHH24MISS") == 0) {
		datetime dt(dts);
		return dt.duration();
	} else if (strcasecmp(format, "YYYYMMDD") == 0) {
		date dt(dts);
		return dt.duration();
	} else if (strcasecmp(format, "HH24MISS") == 0) {
		ptime dt(dts);
		return dt.duration();
	} else {
		return 0;
	}
}

long to_number(const char *str)
{
	return atol(str);
}

time_t number_to_date(long t)
{
	return static_cast<time_t>(t);
}

time_t number_to_time(long t)
{
	return static_cast<time_t>(t);
}

long date_to_number(time_t t)
{
	return static_cast<long>(t);
}

long time_to_number(time_t t)
{
	return static_cast<long>(t);
}

const char * ltrim(const char *value)
{
	string& result = get_strcat_buf();

	while (*value == ' ')
		value++;

	result = value;
	return result.c_str();
}

const char * rtrim(const char *value)
{
	string& result = get_strcat_buf();

	result = value;
	string::size_type pos = result.find_last_not_of(' ');
	result.resize(pos);
	return result.c_str();
}

const char * trim(const char *value)
{
	string& result = get_strcat_buf();

	while (*value == ' ')
		value++;

	result = value;
	string::size_type pos = result.find_last_not_of(' ');
	result.resize(pos);
	return result.c_str();
}

int length(const char *value)
{
	return strlen(value);
}

const char * lower(const char *value)
{
	string& result = get_strcat_buf();

	result = "";
	while (*value != '\0')
		result += ::tolower(*value);
	return result.c_str();
}

const char * upper(const char *value)
{
	string& result = get_strcat_buf();

	result = "";
	while (*value != '\0')
		result += ::toupper(*value);
	return result.c_str();
}

const char * chr(int value)
{
	string& result = get_strcat_buf();

	result = static_cast<char>(value);
	return result.c_str();
}

int instr(const char *str1, const char *str2, int pos, int nth)
{
	const char *ptr = str1 + pos - 1;
	while (nth--) {
		ptr = strstr(ptr, str2);
		if (ptr == NULL)
			return 0;
		ptr++;
	}

	return static_cast<int>(ptr - str1);
}

const char * lpad(const char *str, int len, const char *pad)
{
	string& result = get_strcat_buf();

	int str_len = strlen(str);
	if (str_len > len) {
		result = str;
	} else {
		result = "";
		for (int i = 0; i < len - str_len; i++)
			result += pad;
		result += str;
	}

	return result.c_str();
}

const char * rpad(const char *str, int len, const char *pad)
{
	string& result = get_strcat_buf();

	int str_len = strlen(str);
	if (str_len > len) {
		result = str;
	} else {
		result = str;
		for (int i = 0; i < len - str_len; i++)
			result += pad;
	}

	return result.c_str();
}

const char * substr(const char *str, int pos, int len)
{
	string& result = get_strcat_buf();

	result = string(str + pos - 1, len);
	return result.c_str();
}

long chartorowid(const char *crowid)
{
	return atol(crowid);
}

const char * rowidtochar(long rowid)
{
	string& result = get_strcat_buf();

	ostringstream fmt;
	fmt << rowid;
	result = fmt.str();
	return result.c_str();
}

long get_currval(const char *seq_name)
{
	dbct_ctx *DBCT = dbct_ctx::instance();
	dbc_se& se_mgr = dbc_se::instance(DBCT);

	return se_mgr.get_currval(seq_name);
}

long get_nextval(const char *seq_name)
{
	dbct_ctx *DBCT = dbct_ctx::instance();
	dbc_se& se_mgr = dbc_se::instance(DBCT);

	return se_mgr.get_nextval(seq_name);
}

