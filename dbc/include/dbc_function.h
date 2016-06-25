#if !defined(__DBC_FUNCTION_H__)
#define __DBC_FUNCTION_H__

#include "dbc_int_function.h"

using namespace std;

string& get_strcat_buf();

template<typename V>
const char * to_char(const V& value)
{
	string& result = get_strcat_buf();

	ostringstream fmt;
	fmt << value;
	result = fmt.str();
	return result.c_str();
}

template<typename K, typename V>
V decode(const K& key, const K& key1, const V& value1, const V& value)
{
	if (key == key1)
		return value1;
	else
		return value;
}

template<typename K, typename V>
V decode(const K& key, const K& key1, const V& value1, const K& key2, const V& value2,
	const V& value)
{
	if (key == key1)
		return value1;
	else if (key == key2)
		return value2;
	else
		return value;
}

template<typename K, typename V>
V decode(const K& key, const K& key1, const V& value1, const K& key2, const V& value2,
	const K& key3, const V& value3, const V& value)
{
	if (key == key1)
		return value1;
	else if (key == key2)
		return value2;
	else if (key == key3)
		return value3;
	else
		return value;
}

template<typename V>
V mod(const V& m_value, const V& n_value)
{
	return m_value % n_value;
}

const char * to_char(time_t t, const char *format);
time_t to_date(const char *dts, const char *format);
long to_number(const char *str);
time_t number_to_date(long t);
time_t number_to_time(long t);
long date_to_number(time_t t);
long time_to_number(time_t t);
const char * ltrim(const char *value);
const char * rtrim(const char *value);
const char * trim(const char *value);
int length(const char *value);
const char * lower(const char *value);
const char * upper(const char *value);
const char * chr(int value);
int instr(const char *str1, const char *str2, int pos = 1, int nth = 1);
const char * lpad(const char *str, int len, const char *pad);
const char * rpad(const char *str, int len, const char *pad);
const char * substr(const char *str, int pos, int len);
long chartorowid(const char *crowid);
const char * rowidtochar(long rowid);
long get_currval(const char *seq_name);
long get_nextval(const char *seq_name);

#endif

