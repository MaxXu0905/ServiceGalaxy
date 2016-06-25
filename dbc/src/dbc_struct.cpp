#include "dbc_internal.h"

namespace ai
{
namespace sg
{

const char *STAT_NAMES[TOTAL_STAT] = { "C", "R", "C1", "C2" };

dbc_bbmeters_t::dbc_bbmeters_t()
{
	memset(&sqlite_states, 0, sizeof(sqlite_states));
	tran_id = 0;

	load_ready = false;
	currtes = 0;
	curmaxte = 0;
	additional_count = 0;

	for (int i = 0; i < TOTAL_STAT; i++)
		stat_array[i].init();
}

dbc_bbmeters_t::~dbc_bbmeters_t()
{
}

void dbc_bboard_t::init(const dbc_bbparms_t& config_bbparms)
{
	dbcp_ctx *DBCP = dbcp_ctx::instance();

	bbparms = config_bbparms;
	bbparms.proc_type = DBCP->_DBCP_proc_type;
	(void)new (&syslock) bi::interprocess_recursive_mutex();
	(void)new (&memlock) bi::interprocess_recursive_mutex();
	(void)new (&nomem_mutex) bi::interprocess_mutex();
	(void)new (&nomem_cond) bi::interprocess_condition();
	(void)new (&bbmeters) dbc_bbmeters_t();
	bbmap.mem_free = bbparms.initial_size;

	// 初始化空闲内存列表
	dbc_mem_block_t *mem_block = long2block(bbmap.mem_free);
	mem_block->magic = DBC_MEM_MAGIC;
	mem_block->flags = MEM_IN_USE;
	mem_block->size = bbparms.total_size - bbparms.initial_size - sizeof(dbc_mem_block_t);
	mem_block->prev = -1;
	mem_block->next = -1;

	// 最后初始化MAGIC，防止初始化未完成时有客户端进入
	bbparms.magic = DBC_MAGIC;
}

long dbc_bboard_t::alloc(size_t size)
{
	while (1) {
		bi::scoped_lock<bi::interprocess_recursive_mutex> lock(memlock);
		long mem_offset = bbmap.mem_free;

		while (mem_offset != -1) {
			dbc_mem_block_t *mem_block = long2block(mem_offset);
			if (mem_block->size > size) {
				// Remove from list
				if (mem_block->prev != -1) {
					dbc_mem_block_t *prev = long2block(mem_block->prev);
					prev->next = mem_block->next;
				} else {
					bbmap.mem_free = mem_block->next;
				}

				if (mem_block->next != -1) {
					dbc_mem_block_t *next = long2block(mem_block->next);
					next->prev = mem_block->prev;
				}

				if (mem_block->size > size + sizeof(dbc_mem_block_t)) {
					dbc_mem_block_t *remain_block = reinterpret_cast<dbc_mem_block_t *>(reinterpret_cast<char *>(mem_block) + sizeof(dbc_mem_block_t) + size);
					remain_block->magic = DBC_MEM_MAGIC;
					remain_block->size = mem_block->size - size - sizeof(dbc_mem_block_t);
					free_internal(block2long(remain_block));
					mem_block->size = size;
				}

				mem_block->flags = MEM_IN_USE;
				return block2long(mem_block) + sizeof(dbc_mem_block_t);
			}

			mem_offset = mem_block->next;
		}

		gpp_ctx *GPP = gpp_ctx::instance();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Out of memory, please use dbc_admin to extend shared memory")).str(SGLOCALE));

		bi::scoped_lock<bi::interprocess_mutex> nomemlock(nomem_mutex);

		// 释放系统锁，等获取到通知后再获取系统锁，防止死锁
		int lock_level = 0;
		try {
			while (1) {
				syslock.unlock();
				lock_level++;
			}
		} catch (bi::interprocess_exception& ex) {
		}

		lock.unlock();
		nomem_cond.wait(nomemlock);
		lock.lock();

		while (lock_level--)
			syslock.lock();
	}
}

void dbc_bboard_t::free(long offset)
{
	bi::scoped_lock<bi::interprocess_recursive_mutex> lock(memlock);

	free_internal(offset);
	nomem_cond.notify_all();
}

dbc_mem_block_t * dbc_bboard_t::long2block(long offset)
{
	return reinterpret_cast<dbc_mem_block_t *>(reinterpret_cast<char *>(this) + offset);
}

long dbc_bboard_t::block2long(const dbc_mem_block_t *mem_block)
{
	return reinterpret_cast<const char *>(mem_block) - reinterpret_cast<const char *>(this);
}

void dbc_bboard_t::free_internal(long offset)
{
	dbc_mem_block_t *free_block = long2block(offset);
	if (free_block->magic != DBC_MEM_MAGIC)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: mem_block magic doesn't match")).str(SGLOCALE));

	free_block->flags &= ~MEM_IN_USE;

	// First check if we can find a block to combine a big block.
	long mem_offset;
	dbc_mem_block_t *mem_block;

AGAIN:
	if (bbmap.mem_free == -1) {
		bbmap.mem_free = offset;
		free_block->prev = -1;
		free_block->next = -1;
		return;
	}

	mem_offset = bbmap.mem_free;
	for (; mem_offset != -1; mem_offset = mem_block->next) {
		mem_block = long2block(mem_offset);
		if (mem_offset + mem_block->size + sizeof(dbc_mem_block_t) == offset) {
			// Remove from list
			if (mem_block->prev != -1) {
				dbc_mem_block_t *prev = long2block(mem_block->prev);
				prev->next = mem_block->next;
			} else {
				bbmap.mem_free = mem_block->next;
			}

			if (mem_block->next != -1) {
				dbc_mem_block_t *next = long2block(mem_block->next);
				next->prev = mem_block->prev;
			}

			mem_block->size += free_block->size + sizeof(dbc_mem_block_t);
			offset = mem_offset;
			free_block = mem_block;
			goto AGAIN;
		} else if (offset + free_block->size + sizeof(dbc_mem_block_t) == mem_offset) {
			// Remove from list
			if (mem_block->prev != -1) {
				dbc_mem_block_t *prev = long2block(mem_block->prev);
				prev->next = mem_block->next;
			} else {
				bbmap.mem_free = mem_block->next;
			}

			if (mem_block->next != -1) {
				dbc_mem_block_t *next = long2block(mem_block->next);
				next->prev = mem_block->prev;
			}

			free_block->size += mem_block->size + sizeof(dbc_mem_block_t);
			goto AGAIN;
		}
	}

	mem_offset = bbmap.mem_free;
	while (1) {
		mem_block = long2block(mem_offset);
		if (mem_block->size <= free_block->size) {
			// Insert front
			if (mem_block->prev == -1) {
				mem_block->prev = offset;
				free_block->prev = -1;
				free_block->next = mem_offset;
				bbmap.mem_free = offset;
			} else {
				dbc_mem_block_t *prev = long2block(mem_block->prev);
				prev->next = offset;
				mem_block->prev = offset;
				free_block->prev = block2long(prev);
				free_block->next = mem_offset;
			}
			return;
		}

		if (mem_block->next == -1) {
			// Insert last
			mem_block->next = offset;
			free_block->prev = mem_offset;
			free_block->next = -1;
			return;
		}

		mem_offset = mem_block->next;
	}
}

short row_link_t::inc_ref()
{
	ipc_sem::tas(&accword, SEM_WAIT);
	short result = ++ref_count;
	ipc_sem::semclear(&accword);
	return result;
}

short row_link_t::dec_ref()
{
	ipc_sem::tas(&accword, SEM_WAIT);
	short result = --ref_count;
	ipc_sem::semclear(&accword);
	return result;
}

bool row_link_t::lock(dbct_ctx *DBCT)
{
	bool retval = true;
#if defined(DEBUG)
	scoped_debug<bool> debug(20, __PRETTY_FUNCTION__, "", &retval);
#endif
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();

	ipc_sem::tas(&accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&accword)) {
		ipc_sem::semclear(&accword);
	} BOOST_SCOPE_EXIT_END

	while (1) {
		if (rte_id == rte->rte_id) {
			lock_level++;
			break;
		}

		if (rte_id == -1) {
			// 没有会话加记录锁，则占用该记录锁
			lock_level = 1;
			rte_id = rte->rte_id;
			break;
		}

		wait_level++;

		// 获取加记录锁的会话
		dbc_rte_t *locked_rte = rte_mgr.base() + rte_id;

		// 对加记录锁的会话加会话锁
		bi::scoped_lock<bi::interprocess_mutex> lock(locked_rte->mutex);

		ipc_sem::semclear(&accword);

		// 等待会话解锁
		locked_rte->cond.wait(lock);

		ipc_sem::tas(&accword, SEM_WAIT);
		wait_level--;
	}

	return retval;
}

void row_link_t::unlock(dbct_ctx *DBCT)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(20, __PRETTY_FUNCTION__, "", NULL);
#endif

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();

	ipc_sem::tas(&accword, SEM_WAIT);

	if (--lock_level == 0)
		rte_id = -1;

	if (wait_level) {
		bi::scoped_lock<bi::interprocess_mutex> lock(rte->mutex);
		rte->cond.notify_one();
	}

	ipc_sem::semclear(&accword);
}

row_bucket_t::row_bucket_t()
{
	root = -1;
}

row_bucket_t::~row_bucket_t()
{
}

bool dbc_index_key_t::operator<(const dbc_index_key_t& rhs) const
{
	if (table_id < rhs.table_id)
		return true;
	else if (table_id > rhs.table_id)
		return false;

	return index_id < rhs.index_id;
}

dbc_inode_t& dbc_inode_t::operator=(const dbc_inode_t& rhs)
{
	rowid = rhs.rowid;

	return *this;
}

dbc_snode_t& dbc_snode_t::operator=(const dbc_snode_t& rhs)
{
	rowid = rhs.rowid;

	return *this;
}

}
}

