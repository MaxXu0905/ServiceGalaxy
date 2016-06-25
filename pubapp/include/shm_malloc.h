#if !defined(__SHM_MALLOC_H__)
#define __SHM_MALLOC_H__

#include <string>
#include "machine.h"
#include "gpp_ctx.h"
#include "gpt_ctx.h"

namespace ai
{
namespace sg
{

using namespace std;

int const MAX_KEYS = 1024;
int const SHM_MALLOC_MAGIC = 0x0073BE4F;
int const SHM_DATA_MAGIC = 0x008379F8;
int const SHM_MIN_BITS = 6;
int const SHM_MAX_BITS = 30;
int const SHM_MIN_SIZE = (1 << SHM_MIN_BITS);
int const SHM_MAX_SIZE = (1 << SHM_MAX_BITS);
int const SHM_BUCKETS = SHM_MAX_BITS - SHM_MIN_BITS + 1;
int const SHM_USER_KEY_SIZE = 15;
int const SHM_AVAILABLE = 0x1;
int const SHM_KEY_INUSE = 0x2;

enum shm_locktype_enum
{
	SHM_LOCKTYPE_SYS = 0,
	SHM_LOCKTYPE_FREE = 1,
	SHM_LOCKTYPE_USED = 2
};

struct shm_link_t
{
	unsigned long block_index: (sizeof(unsigned long) * 8 - SHM_MAX_BITS);
	unsigned long block_offset: SHM_MAX_BITS;

	shm_link_t& operator=(const shm_link_t& rhs);
	bool operator<(const shm_link_t& rhs) const;
	bool operator<=(const shm_link_t& rhs) const;
	bool operator>(const shm_link_t& rhs) const;
	bool operator>=(const shm_link_t& rhs) const;
	bool operator==(const shm_link_t& rhs) const;
	bool operator!=(const shm_link_t& rhs) const;

	static shm_link_t SHM_LINK_NULL;
};

struct spinlock_t
{
	int spinlock;
	pid_t lock_pid;
	time_t lock_time;

	void init();
	void lock();
	void unlock();
};

struct shm_malloc_t
{
	unsigned int magic:24;
	unsigned int flags:8;
	key_t keys[MAX_KEYS];
	int shm_ids[MAX_KEYS];
	char *shm_addrs[MAX_KEYS];
	int shm_count;
	int block_size;
	int block_count;
	int bucket_index;			// Bucket slot index for the new allocated shared memory
	spinlock_t syslock;
	spinlock_t free_lock[SHM_BUCKETS];
	spinlock_t used_lock[SHM_BUCKETS];
	shm_link_t free_link[SHM_BUCKETS];
	shm_link_t used_link[SHM_BUCKETS];
};

struct shm_data_t
{
	unsigned int magic:24;
	unsigned int flags:8;
	int bucket_index;
	shm_link_t prev;
	shm_link_t next;
	void *data;
};

struct shm_user_hdr_t
{
	char key[SHM_USER_KEY_SIZE + 1];
	spinlock_t lock;

	void init();
	int get_flags() const;
	void set_flags(int flags);
	void clear_flags(int flags);
};

class shm_admin;

class shm_malloc
{
public:
	shm_malloc(bool same_address_ = false);
	~shm_malloc();

	int attach(key_t key, int shmflg, size_t block_size = 0, int block_count = 0);
	void * malloc(size_t size);
	void * realloc(void *addr, size_t size);
	void free(void *addr);
	void * get_user_data(const shm_link_t& shm_link);
	shm_link_t get_shm_link(void *addr);
	void * find(const string& key);

private:
	int get_slot(size_t size);
	shm_data_t * get_shm_data(const shm_link_t& shm_link);
	shm_link_t get_shm_link(const shm_data_t *shm_data);
	void split(shm_data_t *item_data);
	int attach_data(int block_index);
	void move2free(shm_data_t *item_data, const shm_link_t& item_link);
	void move2used(shm_data_t *item_data, const shm_link_t& item_link);

	bool same_address;
	shm_malloc_t *shm_hdr;
	char *shm_addrs[MAX_KEYS];

	gpp_ctx *GPP;
	gpt_ctx *GPT;

	friend class shm_admin;
};

}
}

#endif

