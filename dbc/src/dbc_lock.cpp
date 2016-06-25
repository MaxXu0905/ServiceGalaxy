#include "dbc_lock.h"
#include "ushm.h"
#include "dbct_ctx.h"
#include "dbc_rte.h"
#include "dstream.h"

namespace ai
{
namespace sg
{

dbc_upgradable_mutex_t::dbc_upgradable_mutex_t()
{
	accword = 0;
	read_count = 0;
	write_count = 0;
}

dbc_upgradable_mutex_t::~dbc_upgradable_mutex_t()
{
}

void dbc_upgradable_mutex_t::lock()
{
	ipc_sem::tas(&accword, SEM_WAIT);

	++write_count;
	while (read_count > 0) {
		ipc_sem::semclear(&accword);
		while (read_count > 0)
			;
		ipc_sem::tas(&accword, SEM_WAIT);
	}

	--write_count;
	while (write_count > 0) {
		ipc_sem::semclear(&accword);
		while (write_count > 0)
			;
		ipc_sem::tas(&accword, SEM_WAIT);
	}

	++write_count;
	ipc_sem::semclear(&accword);
}

void dbc_upgradable_mutex_t::unlock()
{
	ipc_sem::tas(&accword, SEM_WAIT);
	--write_count;
	ipc_sem::semclear(&accword);
}

void dbc_upgradable_mutex_t::lock_sharable()
{
	ipc_sem::tas(&accword, SEM_WAIT);

	while (write_count) {
		ipc_sem::semclear(&accword);
		while (write_count)
			;
		ipc_sem::tas(&accword, SEM_WAIT);
	}

	++read_count;
	ipc_sem::semclear(&accword);
}

void dbc_upgradable_mutex_t::unlock_sharable()
{
	ipc_sem::tas(&accword, SEM_WAIT);
	--read_count;
	ipc_sem::semclear(&accword);
}

}
}

