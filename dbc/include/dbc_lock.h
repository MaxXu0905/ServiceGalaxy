#if !defined(__DBC_LOCK_H__)
#define __DBC_LOCK_H__

#include "ushm.h"

namespace ai
{
namespace sg
{

struct dbc_upgradable_mutex_t
{
	int accword;
	volatile short read_count;
	volatile short write_count;

	dbc_upgradable_mutex_t();
	~dbc_upgradable_mutex_t();

	void lock();
	void unlock();
	void lock_sharable();
	void unlock_sharable();
};

}
}

#endif

