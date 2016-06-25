#if !defined(__SHM_PAIR_H__)
#define __SHM_PAIR_H__

#include <string>
#include "machine.h"
#include "shm_malloc.h"

namespace ai
{
namespace sg
{

using namespace std;

int const SHP_MAGIC = 0x73BE467F;
int const SHP_KEY_SIZE = 15;
int const SHP_INVALID_FLAG = 0x1;
int const SHP_INUSE_FLAG = 0x2;

struct shp_hdr_t : public shm_user_hdr_t
{
	int magic;
	int ref_count;
	int total_size;
	int hash_size;
	int data_size;
	int data_len;
	int free_link;
};

class shm_pair;
class shm_admin;

class shp_data_t
{
public:
	bool valid() const;
	void invalidate(shm_pair *shp);
	void clear(shm_pair *shp);
	void * get_data();
	
private:
	int flags;
	int ref_count;
	int prev;
	int next;
	int bucket;
	char key_name[SHP_KEY_SIZE + 1];
	void *data;

	friend class shm_pair;
	friend class shm_admin;
};

class shm_pair
{
public:
	enum shm_pair_enum
	{
		SHM_PAIR_ERROR = -1,
		SHM_PAIR_LOGICAL = 0,
		SHM_PAIR_PHYSICAL = 1
	};
	
	shm_pair(shm_malloc& shm_mgr_, const string& com_key, int hash_size = 0, int data_size = 0, int data_len = 0);
	~shm_pair();

	shp_data_t * find(const string& svc_key);
	shp_data_t * put(const string& svc_key, const void *data, int data_len);
	// -1: error
	// 0: erase logical
	// 1: erase physically
	int erase(shp_data_t *shp_data);
	void lock();
	void unlock();

private:
	shp_data_t * at(int index);
	
	shm_malloc& shm_mgr;
	shp_hdr_t *shp_hdr;
	int *hash_table;
	char *shp_table;

	gpp_ctx *GPP;
	gpt_ctx *GPT;
	ostringstream fmt;

	friend class shm_admin;
};

}
}

#endif

