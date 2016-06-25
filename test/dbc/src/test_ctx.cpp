#include "test_ctx.h"

namespace ai
{
namespace sg
{

test_ctx::test_ctx()
{
}

test_ctx::~test_ctx()
{
}

void test_ctx::init()
{
	strcpy(bds_area_code[0].area_code, "00244");
	bds_area_code[0].default_region_code = '5';
	bds_area_code[0].eff_date = 1199116800;
	bds_area_code[0].exp_date = 2114352000;

	strcpy(bds_area_code[1].area_code, "0087481");
	bds_area_code[1].default_region_code = 'H';
	bds_area_code[1].eff_date = 1199116800;
	bds_area_code[1].exp_date = 2114352000;

	// 该节点不存在
	strcpy(bds_area_code[2].area_code, "00214");
	bds_area_code[2].default_region_code = 'U';
	bds_area_code[2].eff_date = 1199116800;
	bds_area_code[2].exp_date = 2114352000;

	strcpy(bds_area_code[3].area_code, "0413");
	bds_area_code[3].default_region_code = '4';
	bds_area_code[3].eff_date = 1199116800;
	bds_area_code[3].exp_date = 2114352000;

	strcpy(bds_area_code[4].area_code, "0570");
	bds_area_code[4].default_region_code = '6';
	bds_area_code[4].eff_date = 1199116800;
	bds_area_code[4].exp_date = 2114352000;

	strcpy(bds_area_code[5].area_code, "0087078");
	bds_area_code[5].default_region_code = 'K';
	bds_area_code[5].eff_date = 1199116800;
	bds_area_code[5].exp_date = 2114352000;

	strcpy(bds_area_code[6].area_code, "0087439");
	bds_area_code[6].default_region_code = 'L';
	bds_area_code[6].eff_date = 1199116800;
	bds_area_code[6].exp_date = 2114352000;

	strcpy(bds_area_code[7].area_code, "0033");
	bds_area_code[7].default_region_code = 'B';
	bds_area_code[7].eff_date = 1199116800;
	bds_area_code[7].exp_date = 2114352000;

	strcpy(bds_area_code[8].area_code, "00505");
	bds_area_code[8].default_region_code = 'E';
	bds_area_code[8].eff_date = 1199116800;
	bds_area_code[8].exp_date = 2114352000;

	strcpy(bds_area_code[9].area_code, "0084");
	bds_area_code[9].default_region_code = 'P';
	bds_area_code[9].eff_date = 1199116800;
	bds_area_code[9].exp_date = 2114352000;
}

test_ctx * test_ctx::self()
{
	static bool once = false;
	static pthread_key_t key;
	// use static initialization
	static pthread_mutex_t keylock = PTHREAD_MUTEX_INITIALIZER;

	// this test needs to be protected or put in "do once" section
	if (!once) {
		::pthread_mutex_lock(&keylock);
		if (!once) {
			if (::pthread_key_create(&key, &freectx) == -1) {
				::pthread_mutex_unlock(&keylock);
				return NULL;
			}
			once = true;
		}
		::pthread_mutex_unlock(&keylock);
	}

	char *value = reinterpret_cast<char *>(::pthread_getspecific(key));
	if (value == NULL) {
		test_ctx *_self = new test_ctx();
		::pthread_setspecific(key, reinterpret_cast<void *>(_self));
		_self->init();
		return _self;
	} else {
		return reinterpret_cast<test_ctx *>(value);
	}
}

void * test_ctx::get_void(long offset)
{
	return reinterpret_cast<void *>(reinterpret_cast<char *>(this) + offset);
}

long * test_ctx::get_long(long offset)
{
	return reinterpret_cast<long *>(reinterpret_cast<char *>(this) + offset);
}

bool * test_ctx::get_bool(long offset)
{
	return reinterpret_cast<bool *>(reinterpret_cast<char *>(this) + offset);
}

t_bds_area_code * test_ctx::get_struct(long offset)
{
	return reinterpret_cast<t_bds_area_code *>(reinterpret_cast<char *>(this) + offset);
}

bool test_ctx::match(const t_bds_area_code& left, const t_bds_area_code& right)
{
	if (strcmp(left.area_code, right.area_code) != 0)
		return false;

	if (left.default_region_code != right.default_region_code)
		return false;

	if (left.eff_date != right.eff_date)
		return false;

	if (left.exp_date != right.exp_date)
		return false;

	return true;
}

void test_ctx::freectx(void *key)
{
	char *value = reinterpret_cast<char *>(key);
	if (value != NULL)
		delete reinterpret_cast<test_ctx *>(value);
}

}
}

