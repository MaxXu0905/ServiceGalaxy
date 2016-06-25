#include "shm_malloc.h"
#include "ushm.h"
#include "usignal.h"

namespace ai
{
namespace sg
{

shm_link_t shm_link_t::shm_link_t::SHM_LINK_NULL = { 0, 0 };

shm_link_t& shm_link_t::operator=(const shm_link_t& rhs)
{
	block_index = rhs.block_index;
	block_offset = rhs.block_offset;
	return *this;
}

bool shm_link_t::operator<(const shm_link_t& rhs) const
{
	if (block_index < rhs.block_index)
		return true;
	else if (block_index > rhs.block_index)
		return false;

	return block_offset < rhs.block_offset;
}

bool shm_link_t::operator<=(const shm_link_t& rhs) const
{
	if (block_index < rhs.block_index)
		return true;
	else if (block_index > rhs.block_index)
		return false;

	return block_offset <= rhs.block_offset;
}

bool shm_link_t::operator>(const shm_link_t& rhs) const
{
	if (block_index > rhs.block_index)
		return true;
	else if (block_index < rhs.block_index)
		return false;

	return block_offset > rhs.block_offset;
}

bool shm_link_t::operator>=(const shm_link_t& rhs) const
{
	if (block_index > rhs.block_index)
		return true;
	else if (block_index < rhs.block_index)
		return false;

	return block_offset >= rhs.block_offset;
}

bool shm_link_t::operator==(const shm_link_t& rhs) const
{
	if (block_index == rhs.block_index && block_offset == rhs.block_offset)
		return true;
	else
		return false;
}

bool shm_link_t::operator!=(const shm_link_t& rhs) const
{
	if (block_index != rhs.block_index || block_offset != rhs.block_offset)
		return true;
	else
		return false;
}

void spinlock_t::init()
{
	spinlock = 0;
	lock_pid = 0;
	lock_time = ::time(0);
}

void spinlock_t::lock()
{
	while (!ipc_sem::tas(&spinlock, SEM_WAIT))
		;
	lock_pid = ::getpid();
	lock_time = ::time(0);
}

void spinlock_t::unlock()
{
	lock_pid = 0;
	ipc_sem::semclear(&spinlock);
}

void shm_user_hdr_t::init()
{
	::memset(key, '\0', sizeof(key));
	// Don't call lock() directly, since spinlock is not initialized yet.
	lock.spinlock = 1;
	lock.lock_pid = ::getpid();
	lock.lock_time = ::time(0);
}

int shm_user_hdr_t::get_flags() const
{
	const shm_data_t *shm_hdr = reinterpret_cast<const shm_data_t *>(
		reinterpret_cast<const char *>(this) - sizeof(shm_data_t) + sizeof(void *));
	return shm_hdr->flags;
}

void shm_user_hdr_t::set_flags(int flags)
{
	shm_data_t *shm_hdr = reinterpret_cast<shm_data_t *>(
		reinterpret_cast<char *>(this) - sizeof(shm_data_t) + sizeof(void *));
	shm_hdr->flags |= flags;
}

void shm_user_hdr_t::clear_flags(int flags)
{
	shm_data_t *shm_hdr = reinterpret_cast<shm_data_t *>(
		reinterpret_cast<char *>(this) - sizeof(shm_data_t) + sizeof(void *));
	shm_hdr->flags &= ~flags;
}

shm_malloc::shm_malloc(bool same_address_)
	: same_address(same_address_)
{
	shm_hdr = NULL;
	for (int i = 0; i < MAX_KEYS; i++)
		shm_addrs[i] = NULL;

	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();
}

shm_malloc::~shm_malloc()
{
	for (int i = 0; i < MAX_KEYS; i++) {
		if (shm_addrs[i] != NULL)
			::shmdt(shm_addrs[i]);
	}
}

int shm_malloc::attach(key_t key, int shmflg, size_t block_size, int block_count)
{
	int shm_id;
	bool created = false;
	int i;
	size_t tmp_block_size;

	// Remove IPC_CREAT if possible
	if (block_size == 0 || block_count == 0)
		shmflg &= ~IPC_CREAT;

	if ((shm_id = ::shmget(key, 0, 0)) == -1) {
		if (errno != ENOENT || !(shmflg & IPC_CREAT)) { // nothing exists yet
			GPT->_GPT_native_code = USHMGET;
			GPT->_GPT_error_code = GPEOS;
			GPP->write_log(__FILE__, __LINE__,(_("ERROR: shmget() fail")).str(SGLOCALE) );
			return -1;
		}

		if ((shm_id = ::shmget(key, sizeof(shm_malloc_t), shmflg)) == -1) {
			GPT->_GPT_native_code = USHMGET;
			GPT->_GPT_error_code = GPEOS;
			GPP->write_log(__FILE__, __LINE__,(_("ERROR: shmget() fail")).str(SGLOCALE) );
			return -1;
		}

		created = true;
	}

	shm_addrs[0] = reinterpret_cast<char *>(::shmat(shm_id, NULL, shmflg));
	if (shm_addrs[0] == reinterpret_cast<char *>(-1)) {
		GPT->_GPT_native_code = USHMAT;
		GPT->_GPT_error_code = GPEOS;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: shmat() fail")).str(SGLOCALE) );
		return -1;
	}

	shm_hdr = reinterpret_cast<shm_malloc_t *>(shm_addrs[0]);
	shm_hdr->flags = SHM_AVAILABLE;

	if (created) {
		int block_index = 0;
		for (tmp_block_size = SHM_MIN_SIZE; tmp_block_size < block_size; tmp_block_size <<= 1)
			block_index++;

		if (tmp_block_size != block_size) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__,(_("ERROR: block_size should be expontential value of 2.")).str(SGLOCALE) );
			return -1;
		}

		shm_hdr->magic = SHM_MALLOC_MAGIC;
		shm_hdr->keys[0] = key;
		shm_hdr->shm_ids[0] = shm_id;
		shm_hdr->shm_addrs[0] = shm_addrs[0];
		for (i = 1; i < MAX_KEYS; i++) {
			shm_hdr->keys[i] = -1;
			shm_hdr->shm_ids[i] = -1;
			shm_hdr->shm_addrs[i] = NULL;
		}
		shm_hdr->shm_count = 1;
		shm_hdr->block_size = block_size;
		shm_hdr->block_count = block_count;
		shm_hdr->bucket_index = block_index;
		shm_hdr->syslock.init();

		for (i = 0; i < SHM_BUCKETS; i++) {
			shm_hdr->free_lock[i].init();
			shm_hdr->used_lock[i].init();
			shm_hdr->free_link[i] = shm_link_t::SHM_LINK_NULL;
			shm_hdr->used_link[i] = shm_link_t::SHM_LINK_NULL;
		}

	} else if (shm_hdr->magic != SHM_MALLOC_MAGIC) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: shared memory insane")).str(SGLOCALE) );
		return -1;
	}

	return 0;
}

void * shm_malloc::malloc(size_t size)
{
	shm_link_t free_link;
	shm_link_t item_link;
	shm_data_t *free_data;
	shm_data_t *item_data;
	int slot;
	int i;
	int shm_count;
	scoped_usignal sigs; // defer signals

	// Attach to first SHM if not attached yet.
	if (shm_hdr == NULL) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Not attached to manage shared memory, call attach() first.")).str(SGLOCALE) );
		return NULL;
	}

	// First find suitable bucket.
	slot = get_slot(size);
	if (slot == SHM_BUCKETS) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Size too big for malloc")).str(SGLOCALE) );
		return NULL;
	}

try_again:
	// First save shm_count, it will be used to check if shared memory is created durating search.
	shm_count = shm_hdr->shm_count;

	// Go through bucket to find a suitable node.
	for (i = slot; i < SHM_BUCKETS; i++) {
		shm_hdr->free_lock[i].lock();
		// If empty, try to find in next level bucket.
		if (shm_hdr->free_link[i] == shm_link_t::SHM_LINK_NULL) {
			shm_hdr->free_lock[i].unlock();
			continue;
		}

		// Found node, so get from head.
		free_link = shm_hdr->free_link[i];
		free_data = get_shm_data(free_link);
		shm_hdr->free_link[i] = free_data->next;
		if (free_data->next != shm_link_t::SHM_LINK_NULL) {
			item_data = get_shm_data(free_data->next);
			item_data->prev = shm_link_t::SHM_LINK_NULL;
		}
		shm_hdr->free_lock[i].unlock();
		for (; i > slot; i--) // split node and insert remain part to free_link
			split(free_data);

		/*
		 * Make sure the first bytes are initialized by struct shm_user_hdr_t.
		 * It can be reused by user if the allocated is not used for key/value pair.
		 * The initialization should be done before inserting into used_link,
		 * so we can make sure only one can manipulate this buffer.
		 */
		shm_user_hdr_t *user_hdr = reinterpret_cast<shm_user_hdr_t *>(&free_data->data);
		user_hdr->init();

		// insert into used_link.
		move2used(free_data, free_link);
		return &free_data->data;
	}

	// Lock it so no other one can create shared memory.
	shm_hdr->syslock.lock();
	if (shm_count != shm_hdr->shm_count) { // If some one has created another shared memory, try again.
		shm_hdr->syslock.unlock();
		goto try_again;
	}

	if (!(shm_hdr->flags & SHM_AVAILABLE)) {
		shm_hdr->syslock.unlock();
		GPT->_GPT_error_code = GPEDIAGNOSTIC;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Shared memory not available")).str(SGLOCALE) );
		return NULL;
	}

	// Reach to maximum shared memory blocks allowed.
	if (shm_hdr->shm_count == shm_hdr->block_count) {
		shm_hdr->syslock.unlock();
		GPT->_GPT_native_code = USBRK;
		GPT->_GPT_error_code = GPEOS;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Out of memory")).str(SGLOCALE) );
		return NULL;
	}

	item_link.block_index = shm_hdr->shm_count;;
	item_link.block_offset = 0;
	item_data = get_shm_data(item_link);
	if (item_data == NULL) {
		shm_hdr->syslock.unlock();
		GPT->_GPT_native_code = USBRK;
		GPT->_GPT_error_code = GPEOS;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Out of memory")).str(SGLOCALE) );
		return NULL;
	}
	item_data->magic = SHM_DATA_MAGIC;
	item_data->flags = 0;
	item_data->bucket_index = shm_hdr->bucket_index;

	for (i = shm_hdr->bucket_index; i > slot; i--)
		split(item_data);

	// unlock() after split, so we can make sure one shared memory is allocated at a time.
	shm_hdr->syslock.unlock();

	/*
	 * Make sure the first bytes are initialized by struct shm_user_hdr_t.
	 * It can be reused by user if the allocated is not used for key/value pair.
	 * The initialization should be done before inserting into used_link,
	 * so we can make sure only one can manipulate this buffer.
	 */
	shm_user_hdr_t *user_hdr = reinterpret_cast<shm_user_hdr_t *>(&item_data->data);
	user_hdr->init();

	// insert into used_link.
	move2used(item_data, item_link);
	return &item_data->data;
}

void shm_malloc::free(void *addr)
{
	if (addr == NULL)
		return;

	bool found = false;
	shm_link_t free_link;
	shm_link_t used_link;
	shm_link_t item_link;
	shm_link_t prev_link;
	shm_data_t *prev_data;
	shm_data_t *next_data;
	shm_data_t *free_data;
	shm_data_t *used_data;
	shm_data_t *item_data = reinterpret_cast<shm_data_t *>(reinterpret_cast<char *>(addr)
		- sizeof(shm_data_t) + sizeof(void *));
	scoped_usignal sigs; // defer signals

	if (item_data->magic != SHM_DATA_MAGIC) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Given wrong address or memory corrupted")).str(SGLOCALE) );
		return;
	}

	shm_hdr->used_lock[item_data->bucket_index].lock();

	// First, go through used_list to find an entry
	used_link = shm_hdr->used_link[item_data->bucket_index];
	for (; used_link != shm_link_t::SHM_LINK_NULL; used_link = used_data->next) {
		used_data = get_shm_data(used_link);
		if (used_data == NULL) {
			shm_hdr->used_lock[item_data->bucket_index].unlock();
			return;
		}

		if (used_data == item_data) {
			// Remove from used_link
			if (used_data->prev == shm_link_t::SHM_LINK_NULL) {
				shm_hdr->used_link[item_data->bucket_index] = used_data->next;
			} else {
				prev_data = get_shm_data(used_data->prev);
				prev_data->next = used_data->next;
			}

			if (used_data->next != shm_link_t::SHM_LINK_NULL) {
				next_data = get_shm_data(used_data->next);
				next_data->prev = used_data->prev;
			}

			found = true;
			break;
		}
	}

	shm_hdr->used_lock[item_data->bucket_index].unlock();

	if (!found) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Memory already freed")).str(SGLOCALE) );
		return;
	}

	item_link = get_shm_link(item_data);

	shm_hdr->free_lock[item_data->bucket_index].lock();

	// Go through free_list and merge if possible
	free_link = shm_hdr->free_link[item_data->bucket_index];
	prev_link = shm_link_t::SHM_LINK_NULL;
	while (free_link != shm_link_t::SHM_LINK_NULL) {
		free_data = get_shm_data(free_link);
		if (free_data == NULL) {
			shm_hdr->free_lock[item_data->bucket_index].unlock();
			return;
		}

		size_t data_size = (SHM_MIN_SIZE << free_data->bucket_index);
		if ((free_link.block_index == item_link.block_index
			&& (free_link.block_offset + data_size == item_link.block_offset))
			|| (free_link.block_offset - data_size == item_link.block_offset)) { // Merge
			// Remove from free_link
			if (free_data->prev == shm_link_t::SHM_LINK_NULL) { // First node
				shm_hdr->free_link[item_data->bucket_index] = free_data->next;
			} else {
				prev_data = get_shm_data(free_data->prev);
				prev_data->next = free_data->next;
			}

			if (free_data->next != shm_link_t::SHM_LINK_NULL) {
				next_data = get_shm_data(free_data->next);
				next_data->prev = free_data->prev;
			}

			shm_hdr->free_lock[item_data->bucket_index].unlock();

			if (free_link.block_offset < item_link.block_offset) {
				item_data = free_data;
				item_link = free_link;
			}

			item_data->bucket_index++;

			// Try next bucket.
			shm_hdr->free_lock[item_data->bucket_index].lock();
			free_link = shm_hdr->free_link[item_data->bucket_index];
			prev_link = shm_link_t::SHM_LINK_NULL;
			continue;
		} else if (free_link > item_link) { // Insert front
			item_data->prev = free_data->prev;
			item_data->next = free_link;
			free_data->prev = item_link;
			if (item_data->prev == shm_link_t::SHM_LINK_NULL) { // Fist node
				shm_hdr->free_link[item_data->bucket_index] = item_link;
			} else {
				prev_data = get_shm_data(item_data->prev);
				prev_data->next = free_data->prev;
			}

			shm_hdr->free_lock[item_data->bucket_index].unlock();
			return;
		}

		prev_link = free_link;
		free_link = free_data->next;
	}

	// Insert back
	item_data->prev = prev_link;
	item_data->next = shm_link_t::SHM_LINK_NULL;
	if (prev_link == shm_link_t::SHM_LINK_NULL) {
		shm_hdr->free_link[item_data->bucket_index] = item_link;
	} else {
		prev_data = get_shm_data(prev_link);
		prev_data->next = item_link;
	}

	shm_hdr->free_lock[item_data->bucket_index].unlock();
}

void * shm_malloc::realloc(void *addr, size_t size)
{
	shm_data_t *item_data = reinterpret_cast<shm_data_t *>(reinterpret_cast<char *>(addr)
		- sizeof(shm_data_t) + sizeof(void *));
	scoped_usignal sigs; // defer signals

	if (item_data->magic != SHM_DATA_MAGIC) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Given wrong address or memory corrupted")).str(SGLOCALE) );
		return NULL;
	}

	size_t data_size = (SHM_MIN_SIZE << item_data->bucket_index);
	if (data_size - sizeof(shm_data_t) + sizeof(void *) >= size)
		return addr;

	void *new_addr = malloc(size);
	if (new_addr == NULL)
		return NULL;

	::memcpy(new_addr, addr, size);
	free(addr);
	return new_addr;
}

void * shm_malloc::get_user_data(const shm_link_t& shm_link)
{
	if (shm_link.block_index < 1 || shm_link.block_index >= shm_hdr->block_count
		|| shm_link.block_offset < 0 || shm_link.block_offset >= shm_hdr->block_size)
		return NULL;

	shm_data_t *shm_data = get_shm_data(shm_link);
	if (shm_data == NULL)
		return NULL;

	return &shm_data->data;
}

shm_link_t shm_malloc::get_shm_link(void *addr)
{
	shm_data_t *item_data = reinterpret_cast<shm_data_t *>(reinterpret_cast<char *>(addr)
		- sizeof(shm_data_t) + sizeof(void *));

	if (item_data->magic != SHM_DATA_MAGIC) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Given wrong address or memory corrupted")).str(SGLOCALE) );
		return shm_link_t::SHM_LINK_NULL;
	}

	return get_shm_link(item_data);
}

shm_link_t shm_malloc::get_shm_link(const shm_data_t *shm_data)
{
	for (int i = 1; i < MAX_KEYS; i++) {
		if (shm_addrs[i] == reinterpret_cast<char *>(-1))
			continue;
		if (shm_addrs[i] <= reinterpret_cast<const char *>(shm_data)
			&& shm_addrs[i] + shm_hdr->block_size > reinterpret_cast<const char *>(shm_data)) {
			shm_link_t shm_link;
			shm_link.block_index = i;
			shm_link.block_offset = reinterpret_cast<const char *>(shm_data) - shm_addrs[i];
			return shm_link;
		}
	}

	return shm_link_t::SHM_LINK_NULL;
}

void * shm_malloc::find(const string& key)
{
	shm_link_t used_link;
	shm_data_t *used_data;

	for (int i = 0; i <= shm_hdr->bucket_index; i++) {
		shm_hdr->used_lock[i].lock();

		// First, go through used_list to find an entry
		used_link = shm_hdr->used_link[i];
		for (; used_link != shm_link_t::SHM_LINK_NULL; used_link = used_data->next) {
			used_data = get_shm_data(used_link);
			if (used_data == NULL) {
				shm_hdr->used_lock[i].unlock();
				return NULL;
			}

			shm_user_hdr_t *user_hdr = reinterpret_cast<shm_user_hdr_t *>(&used_data->data);
			if ((used_data->flags & SHM_KEY_INUSE) && key == user_hdr->key) {
				shm_hdr->used_lock[i].unlock();
				return &used_data->data;
			}
		}

		shm_hdr->used_lock[i].unlock();
	}

	return NULL;
}

int shm_malloc::get_slot(size_t size)
{
	int slot;
	size_t alloc_size = size + sizeof(shm_data_t) - sizeof(void *);

	// Check size, at most a block_size is allocated once a time.
	if (alloc_size > shm_hdr->block_size) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Size too big for malloc")).str(SGLOCALE) );
		return SHM_BUCKETS;
	}

	for (slot = 0; slot < SHM_BUCKETS; slot++) {
		if (alloc_size <= SHM_MIN_SIZE)
			break;
		alloc_size >>= 1;
	}

	return slot;
}

shm_data_t * shm_malloc::get_shm_data(const shm_link_t& shm_link)
{
	if (shm_addrs[shm_link.block_index] == NULL && attach_data(shm_link.block_index) == -1)
		return NULL;

	return reinterpret_cast<shm_data_t *>(shm_addrs[shm_link.block_index] + shm_link.block_offset);
}

void shm_malloc::split(shm_data_t *item_data)
{
	item_data->bucket_index--;
	size_t data_size = (SHM_MIN_SIZE << item_data->bucket_index);
	shm_data_t *split_data = reinterpret_cast<shm_data_t *>(reinterpret_cast<char *>(item_data) + data_size);
	split_data->magic = SHM_DATA_MAGIC;
	split_data->bucket_index = item_data->bucket_index;
	move2free(split_data, get_shm_link(split_data));
}

int shm_malloc::attach_data(int block_index)
{
	key_t start_key = -1;
	int shm_id;
	int i;

	if (block_index >= shm_hdr->block_count || block_index < 1) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Too many shared memory blocks")).str(SGLOCALE) );
		return -1;
	}

	if (shm_hdr->shm_ids[block_index] != -1) {
		shm_id = shm_hdr->shm_ids[block_index];
	} else {
		start_key = shm_hdr->keys[block_index - 1];
		if (start_key == -1) {
			GPT->_GPT_error_code = GPEPROTO;
			GPP->write_log(__FILE__, __LINE__,(_("ERROR: Shared memory must be created in order")).str(SGLOCALE) );
			return -1;
		}

		for (i = 0; i < 100; i++) {
			if ((shm_id = ::shmget(++start_key, shm_hdr->block_size, IPC_CREAT | 0660)) != -1)
				break;
		}
	}

	if (shm_id == -1) {
		GPT->_GPT_native_code = USHMGET;
		GPT->_GPT_error_code = GPEOS;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: shmget() fail")).str(SGLOCALE) );
		return -1;
	}

	if (same_address)
		shm_addrs[block_index] = reinterpret_cast<char *>(::shmat(shm_id, shm_hdr->shm_addrs[block_index], 0));
	else
		shm_addrs[block_index] = reinterpret_cast<char *>(::shmat(shm_id, NULL, 0));
	if (shm_addrs[block_index] == reinterpret_cast<char *>(-1)) {
		GPT->_GPT_native_code = USHMAT;
		GPT->_GPT_error_code = GPEOS;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: shmat() fail")).str(SGLOCALE) );
		return -1;
	}
	if (shm_hdr->shm_ids[block_index] == -1) { // Created
		shm_hdr->keys[block_index] = start_key;
		shm_hdr->shm_ids[block_index] = shm_id;
		shm_hdr->shm_addrs[block_index] = shm_addrs[block_index];
		shm_hdr->shm_count++;
	}

	return 0;
}

void shm_malloc::move2free(shm_data_t *item_data, const shm_link_t& item_link)
{
	shm_link_t free_link;
	shm_data_t *free_data;
	shm_data_t *prev_data;

	shm_hdr->free_lock[item_data->bucket_index].lock();

	// add node to free_link in order
	free_link = shm_hdr->free_link[item_data->bucket_index];
	if (free_link == shm_link_t::SHM_LINK_NULL) { // Link is empty
		shm_hdr->free_link[item_data->bucket_index] = item_link;
		item_data->prev = shm_link_t::SHM_LINK_NULL;
		item_data->next = shm_link_t::SHM_LINK_NULL;
		shm_hdr->free_lock[item_data->bucket_index].unlock();
		return;
	}

	free_data = get_shm_data(free_link);
	if (free_data == NULL) {
		shm_hdr->free_lock[item_data->bucket_index].unlock();
		return;
	}

	for (; free_data->next != shm_link_t::SHM_LINK_NULL && free_link < item_link; free_link = free_data->next) {
		free_data = get_shm_data(free_link);
		if (free_data == NULL) {
			shm_hdr->free_lock[item_data->bucket_index].unlock();
			return;
		}
	}

	if (free_link >= item_link) { // Insert front
		item_data->prev = free_data->prev;
		item_data->next = free_link;
		free_data->prev = item_link;
		if (free_data->prev == shm_link_t::SHM_LINK_NULL) { // Fist node
			shm_hdr->free_link[item_data->bucket_index] = item_link;
		} else {
			prev_data = get_shm_data(item_data->prev);
			prev_data->next = item_link;
		}
	} else { // Reach to last, insert back
		item_data->prev = free_link;
		item_data->next = shm_link_t::SHM_LINK_NULL;
		free_data->next = item_link;
	}

	shm_hdr->free_lock[item_data->bucket_index].unlock();
}

void shm_malloc::move2used(shm_data_t *item_data, const shm_link_t& item_link)
{
	shm_link_t used_link;
	shm_data_t *used_data;
	shm_data_t *prev_data;

	shm_hdr->used_lock[item_data->bucket_index].lock();

	// add node to used_link in order
	used_link = shm_hdr->used_link[item_data->bucket_index];
	if (used_link == shm_link_t::SHM_LINK_NULL) { // Link is empty
		item_data->prev = shm_link_t::SHM_LINK_NULL;
		item_data->next = shm_link_t::SHM_LINK_NULL;
		shm_hdr->used_link[item_data->bucket_index] = item_link;
		shm_hdr->used_lock[item_data->bucket_index].unlock();
		return;
	}

	used_data = get_shm_data(used_link);
	if (used_data == NULL) {
		shm_hdr->used_lock[item_data->bucket_index].unlock();
		return;
	}

	while (used_data->next != shm_link_t::SHM_LINK_NULL && used_link < item_link) {
		used_link = used_data->next;
		used_data = get_shm_data(used_link);
		if (used_data == NULL) {
			shm_hdr->used_lock[item_data->bucket_index].unlock();
			return;
		}
	}

	if (used_link >= item_link) { // Insert front
		item_data->prev = used_data->prev;
		item_data->next = used_link;
		used_data->prev = item_link;
		if (item_data->prev == shm_link_t::SHM_LINK_NULL) { // Fist node
			shm_hdr->used_link[item_data->bucket_index] = item_link;
		} else {
			prev_data = get_shm_data(item_data->prev);
			prev_data->next = item_link;
		}
	} else { // Reach to last, insert back
		item_data->prev = used_link;
		item_data->next = shm_link_t::SHM_LINK_NULL;
		used_data->next = item_link;
	}

	shm_hdr->used_lock[item_data->bucket_index].unlock();
}

}
}

