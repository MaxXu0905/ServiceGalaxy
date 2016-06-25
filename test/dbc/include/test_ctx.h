#if !defined(__TEST_CTX_H__)
#define __TEST_CTX_H__

#include "dbc_api.h"
#include "oracle_control.h"
#include "sql_control.h"
#include "datastruct_structs.h"
#include "search_h.inl"

namespace ai
{
namespace sg
{

class test_ctx
{
public:
	static test_ctx * self();

	void * get_void(long offset);
	long * get_long(long offset);
	bool * get_bool(long offset);
	t_bds_area_code * get_struct(long offset);
	static bool match(const t_bds_area_code& left, const t_bds_area_code& right);

	t_bds_area_code bds_area_code[10];
	t_bds_area_code find_result;
	long rowid;
	bool ret_bool;
	long ret_long;

private:
	test_ctx();
	~test_ctx();

	void init();
	static void freectx(void *key);
};

}
}

#endif

