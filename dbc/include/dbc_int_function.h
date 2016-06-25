#if !defined(__DBC_INT_FUNCTION_H__)
#define __DBC_INT_FUNCTION_H__

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <iomanip>
#include <string>
#include <sstream>
#include "common.h"

using namespace ai::sg;
using namespace std;

// Assign functions

void dbc_assign(char& result, const char *value);
void dbc_assign(unsigned char& result, const char *value);
void dbc_assign(short& result, const char *value);
void dbc_assign(unsigned short& result, const char *value);
void dbc_assign(int& result, const char *value);
void dbc_assign(unsigned int& result, const char *value);
void dbc_assign(long& result, const char *value);
void dbc_assign(unsigned long& result, const char *value);
void dbc_assign(float& result, const char *value);
void dbc_assign(double& result, const char *value);
void dbc_assign(char *result, const char *value, int max_len);

template<typename T1, typename T2>
void dbc_assign(T1& result, T2 value)
{
	result = static_cast<T1>(value);
}

template<typename T>
void dbc_assign(char *result, T value, int max_len)
{
	ostringstream fmt;
	fmt << value;

	string str = fmt.str();
	int len = std::min(static_cast<int>(str.length()), max_len);

	memcpy(result, str.c_str(), len);
	result[len] = '\0';
}

void dbc_date_assign(time_t& result, time_t value);
void dbc_date_assign(time_t& result, const char *value);
void dbc_time_assign(time_t& result, time_t value);
void dbc_time_assign(time_t& result, const char *value);
void dbc_datetime_assign(time_t& result, time_t value);
void dbc_datetime_assign(time_t& result, const char *value);

const char * dbc_vdata(const char *base, int offset);

// compare functions

int dbc_compare(char *value1, char *value2);
int dbc_compare(const char *value1, const char *value2);

template<typename V>
int dbc_compare(V value1, V value2)
{
	if (value1 < value2)
		return -1;
	else if (value1 > value2)
		return 1;
	else
		return 0;
}

// Relation functions

#define dbc_decl_strcat1(type1, type2) \
const char * dbc_strcat(type1 value1, type2 value2);

#define dbc_decl_strcat2(type) \
dbc_decl_strcat1(type, char) \
dbc_decl_strcat1(type, unsigned char) \
dbc_decl_strcat1(type, char *) \
dbc_decl_strcat1(type, unsigned char *) \
dbc_decl_strcat1(type, const char *) \
dbc_decl_strcat1(type, const unsigned char *)

#define dbc_decl_strcat \
dbc_decl_strcat2(char) \
dbc_decl_strcat2(unsigned char) \
dbc_decl_strcat2(char *) \
dbc_decl_strcat2(unsigned char *) \
dbc_decl_strcat2(const char *) \
dbc_decl_strcat2(const unsigned char *)

dbc_decl_strcat

#define dbc_decl_op1(function, type1, type2, op) \
bool function(type1 value1, type2 value2);

#define dbc_decl_op2(function, type1, type2, op) \
bool function(type1 value1, type2 value2);

#define dbc_decl_op3(function, type1, type2, op) \
bool function(type1 value1, type2 value2);

#define dbc_decl_op(function, op) \
dbc_decl_op1(function, char, char *, op) \
dbc_decl_op1(function, char, const char *, op) \
dbc_decl_op1(function, char, unsigned char *, op) \
dbc_decl_op1(function, char, const unsigned char *, op) \
dbc_decl_op1(function, unsigned char, char *, op) \
dbc_decl_op1(function, unsigned char, const char *, op) \
dbc_decl_op1(function, unsigned char, unsigned char *, op) \
dbc_decl_op1(function, unsigned char, const unsigned char *, op) \
dbc_decl_op2(function, char *, char, op) \
dbc_decl_op2(function, const char *, char, op) \
dbc_decl_op2(function, unsigned char *, char, op) \
dbc_decl_op2(function, const unsigned char *, char, op) \
dbc_decl_op2(function, char *, unsigned char, op) \
dbc_decl_op2(function, const char *, unsigned char, op) \
dbc_decl_op2(function, unsigned char *, unsigned char, op) \
dbc_decl_op2(function, const unsigned char *, unsigned char, op) \
dbc_decl_op3(function, char *, char *, op) \
dbc_decl_op3(function, char *, const char *, op) \
dbc_decl_op3(function, char *, unsigned char *, op) \
dbc_decl_op3(function, char *, const unsigned char *, op) \
dbc_decl_op3(function, const char *, char *, op) \
dbc_decl_op3(function, const char *, const char *, op) \
dbc_decl_op3(function, const char *, unsigned char *, op) \
dbc_decl_op3(function, const char *, const unsigned char *, op) \
dbc_decl_op3(function, unsigned char *, char *, op) \
dbc_decl_op3(function, unsigned char *, const char *, op) \
dbc_decl_op3(function, unsigned char *, unsigned char *, op) \
dbc_decl_op3(function, unsigned char *, const unsigned char *, op) \
dbc_decl_op3(function, const unsigned char *, char *, op) \
dbc_decl_op3(function, const unsigned char *, const char *, op) \
dbc_decl_op3(function, const unsigned char *, unsigned char *, op) \
dbc_decl_op3(function, const unsigned char *, const unsigned char *, op)

dbc_decl_op(dbc_gt, >)
dbc_decl_op(dbc_lt, <)
dbc_decl_op(dbc_ge, >=)
dbc_decl_op(dbc_le, <=)
dbc_decl_op(dbc_eq, ==)
dbc_decl_op(dbc_ne, !=)

#define dbc_op_template(function, op) \
template<typename T1, typename T2> \
bool function(const T1& value1, const T2& value2) \
{ \
	return value1 op value2; \
}

dbc_op_template(dbc_gt, >)
dbc_op_template(dbc_lt, <)
dbc_op_template(dbc_ge, >=)
dbc_op_template(dbc_le, <=)
dbc_op_template(dbc_eq, ==)
dbc_op_template(dbc_ne, !=)

template<typename K, typename V>
bool dbc_in(const K& key, const V& value1)
{
	if (key == value1)
		return true;
	else
		return false;
}

template<typename K, typename V>
bool dbc_in(const K& key, const V& value1, const V& value2)
{
	if (key == value1)
		return true;
	else if (key == value2)
		return true;
	else
		return false;
}

template<typename K, typename V>
bool dbc_in(const K& key, const V& value1, const V& value2, const V& value3)
{
	if (key == value1)
		return true;
	else if (key == value2)
		return true;
	else if (key == value3)
		return true;
	else
		return false;
}

template<typename K, typename V>
bool dbc_in(const K& key, const V& value1, const V& value2, const V& value3, const V& value4)
{
	if (key == value1)
		return true;
	else if (key == value2)
		return true;
	else if (key == value3)
		return true;
	else if (key == value4)
		return true;
	else
		return false;
}

template<typename K, typename V>
bool dbc_in(const K& key, const V& value1, const V& value2, const V& value3, const V& value4,
	const V& value5)
{
	if (key == value1)
		return true;
	else if (key == value2)
		return true;
	else if (key == value3)
		return true;
	else if (key == value4)
		return true;
	else if (key == value5)
		return true;
	else
		return false;
}

template<typename K, typename V>
bool dbc_in(const K& key, const V& value1, const V& value2, const V& value3, const V& value4,
	const V& value5, const V& value6)
{
	if (key == value1)
		return true;
	else if (key == value2)
		return true;
	else if (key == value3)
		return true;
	else if (key == value4)
		return true;
	else if (key == value5)
		return true;
	else if (key == value6)
		return true;
	else
		return false;
}

template<typename K, typename V>
bool dbc_in(const K& key, const V& value1, const V& value2, const V& value3, const V& value4,
	const V& value5, const V& value6, const V& value7)
{
	if (key == value1)
		return true;
	else if (key == value2)
		return true;
	else if (key == value3)
		return true;
	else if (key == value4)
		return true;
	else if (key == value5)
		return true;
	else if (key == value6)
		return true;
	else if (key == value7)
		return true;
	else
		return false;
}

template<typename K, typename V>
bool dbc_in(const K& key, const V& value1, const V& value2, const V& value3, const V& value4,
	const V& value5, const V& value6, const V& value7, const V& value8)
{
	if (key == value1)
		return true;
	else if (key == value2)
		return true;
	else if (key == value3)
		return true;
	else if (key == value4)
		return true;
	else if (key == value5)
		return true;
	else if (key == value6)
		return true;
	else if (key == value7)
		return true;
	else if (key == value8)
		return true;
	else
		return false;
}

template<typename K, typename V>
bool dbc_between(const K& key, const V& value1, const V& value2)
{
	if (key >= value1 && key <= value2)
		return true;
	else
		return false;
}

// Aggregation functions

template<typename T>
void dbc_count(T& result, const T& value)
{
	result++;
}

template<typename T>
void dbc_sum(T& result, const T& value)
{
	result += value;
}

void dbc_min(char *result, const char *value);

template<typename T>
void dbc_min(T& result, const T& value)
{
	if (result > value)
		result = value;
}

void dbc_max(char *result, const char *value);

template<typename T>
void dbc_max(T& result, const T& value)
{
	if (result < value)
		result = value;
}

#define dbc_decl_avg(type) \
void dbc_avg(type& result, type value);

dbc_decl_avg(char)
dbc_decl_avg(unsigned char)
dbc_decl_avg(short)
dbc_decl_avg(unsigned short)
dbc_decl_avg(int)
dbc_decl_avg(unsigned int)
dbc_decl_avg(long)
dbc_decl_avg(unsigned long)
dbc_decl_avg(float)
dbc_decl_avg(double)

void dbc_sqlfun_init();

#endif

