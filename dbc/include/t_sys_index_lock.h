#if !defined(__T_SYS_INDEX_LOCK_H__)
#define __T_SYS_INDEX_LOCK_H__

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace ai
{
namespace sg
{

struct t_sys_index_lock
{
	long pid;
	int table_id;
	int index_id;
	time_t lock_date;

	static long equal(const void *key, const void *data);
	static void get_partition(char *key, const void *data);
	static void print(const void *data);
	static void get_hash0(char *key, const void *data);
	static long compare0(const void *key, const void *data);
	static long compare_exact0(const void *key, const void *data);
	static void get_hash1(char *key, const void *data);
	static long compare1(const void *key, const void *data);
	static long compare_exact1(const void *key, const void *data);
};

struct __t_sys_index_lock
{
	long pid;
	int table_id;
	int index_id;
	time_t lock_date;

	static long equal(const void *key, const void *data);
	static void get_partition(char *key, const void *data);
	static void print(const void *data);
	static void get_hash0(char *key, const void *data);
	static long compare0(const void *key, const void *data);
	static long compare_exact0(const void *key, const void *data);
	static void get_hash1(char *key, const void *data);
	static long compare1(const void *key, const void *data);
	static long compare_exact1(const void *key, const void *data);
};

}
}

#endif

