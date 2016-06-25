#include <sys/ipc.h>
#include <sys/shm.h>
#include "shm_pair.h"
#include "uhash.h"

namespace ai
{
namespace sg
{

bool shp_data_t::valid() const
{
	return ((flags & (SHP_INVALID_FLAG | SHP_INUSE_FLAG)) == SHP_INUSE_FLAG);
}

void shp_data_t::invalidate(shm_pair *shp)
{
	shp->lock();
	flags |= SHP_INVALID_FLAG;
	shp->unlock();
	shp->erase(this);
}

void shp_data_t::clear(shm_pair *shp)
{
	shp->lock();
	ref_count = 0;
	shp->unlock();
	shp->erase(this);
}

void * shp_data_t::get_data()
{
	return &data;
}

shm_pair::shm_pair(shm_malloc& shm_mgr_, const string& com_key, int hash_size, int data_size, int data_len)
	: shm_mgr(shm_mgr_)
{
	GPT = gpt_ctx::instance();

	shp_data_t *shp_data;
	int i;

	if (hash_size % 2)
		hash_size++;

	if (data_size % 2)
		data_size++;

	data_len = common::round_up(data_len, static_cast<int>(sizeof(long)));

	size_t smsize = sizeof(shp_hdr) + sizeof(int) * hash_size + (offsetof(shp_data_t, data) + data_len) * data_size;

	shp_hdr = reinterpret_cast<shp_hdr_t *>(shm_mgr.find(com_key));
	if (shp_hdr == NULL) { // Not found, so we create one.
		if (hash_size == 0 || data_size == 0 || data_len == 0) {
			GPT->_GPT_error_code = GPEINVAL;
			throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: hash_size, data_size or data_len is 0")).str(SGLOCALE));
		}

		shp_hdr = reinterpret_cast<shp_hdr_t *>(shm_mgr.malloc(smsize));
		if (shp_hdr == NULL)
			throw bad_system(__FILE__, __LINE__, bad_system::bad_other, 0, (_("ERROR: malloc() failed")).str(SGLOCALE));

		// It's locked by malloc automatically, so we can initialize it safely.
		shp_hdr->set_flags(SHM_KEY_INUSE);
		::strcpy(shp_hdr->key, com_key.c_str());
		shp_hdr->magic = SHP_MAGIC;
		shp_hdr->ref_count = 1;
		shp_hdr->total_size = smsize;
		shp_hdr->hash_size = hash_size;
		shp_hdr->data_size = data_size;
		shp_hdr->data_len = data_len;
		shp_hdr->free_link = 0;

		hash_table = reinterpret_cast<int *>(reinterpret_cast<char *>(shp_hdr) + sizeof(shp_hdr_t));
		shp_table = reinterpret_cast<char *>(hash_table + shp_hdr->hash_size);

		for (i = 0; i < hash_size; i++)
			hash_table[i] = -1;

		for (i = 0; i < data_size; i++) {
			shp_data = at(i);
			shp_data->flags = 0;
			shp_data->ref_count = 0;
			shp_data->prev = i - 1;
			shp_data->next = i + 1;
			shp_data->bucket = -1;
			shp_data->key_name[0] = '\0';
		}
		shp_data = at(data_size - 1);
		shp_data->next = -1;

		// Unlock it so others can use it.
		shp_hdr->lock.unlock();

		// Find again to see if someone has created another node with same key.
		shp_hdr_t *shp_hdr2 = reinterpret_cast<shp_hdr_t *>(shm_mgr.find(com_key));
		if (shp_hdr != shp_hdr2) { // Yes, someone created, so we free ourselves
			shm_mgr.free(shp_hdr);
			shp_hdr = shp_hdr2;
			// Wait for someone to finish initialization.
			shp_hdr->lock.lock();
			shp_hdr->ref_count++;
			shp_hdr->lock.unlock();
			hash_table = reinterpret_cast<int *>(reinterpret_cast<char *>(shp_hdr) + sizeof(shp_hdr_t));
			shp_table = reinterpret_cast<char *>(hash_table + shp_hdr->hash_size);
		}
	} else {
		hash_table = reinterpret_cast<int *>(reinterpret_cast<char *>(shp_hdr) + sizeof(shp_hdr_t));
		shp_table = reinterpret_cast<char *>(hash_table + shp_hdr->hash_size);
	}

	if ((shp_hdr->magic != SHP_MAGIC) || (hash_size != 0 && data_size != 0 && data_len != 0
		&& (shp_hdr->total_size != smsize || shp_hdr->hash_size != hash_size
		 || shp_hdr->data_size != data_size || shp_hdr->data_len != data_len))) {
		GPT->_GPT_error_code = GPESYSTEM;
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: shared memory insane")).str(SGLOCALE));
	}
}

shm_pair::~shm_pair()
{
}

shp_data_t * shm_pair::find(const string& svc_key)
{
	int bucket = uhash::hashname(svc_key.c_str(), shp_hdr->hash_size);

	lock();

	int shp_index = hash_table[bucket];
	if (shp_index == -1) {
		unlock();
		return NULL;
	}

	shp_data_t *shp_data = at(shp_index);
	while (1) {
		if (!(shp_data->flags & SHP_INVALID_FLAG) && svc_key == shp_data->key_name) {
			shp_data->ref_count++;
			shp_hdr->lock.unlock();
			return shp_data;
		}

		if (shp_data->next == -1) {
			unlock();
			return NULL;
		}

		shp_data = at(shp_data->next);
	}

	unlock();
	return NULL;
}

shp_data_t * shm_pair::put(const string& svc_key, const void *data, int data_len)
{
	if (svc_key.length() > SHP_KEY_SIZE) {
		GPT->_GPT_error_code = GPEINVAL;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: svc_key size must be not bigger than {1}") % SHP_KEY_SIZE).str(SGLOCALE) );
		return NULL;
	}

	if (data_len > shp_hdr->data_len) {
		GPT->_GPT_error_code = GPEINVAL;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: data_len given to create SDC is too small")).str(SGLOCALE) );
		return NULL;
	}

	lock();

	if (shp_hdr->free_link == -1) {
		unlock();
		GPT->_GPT_error_code = GPELIMIT;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: No free slot available.")).str(SGLOCALE) );
		return NULL;
	}

	int item_index = shp_hdr->free_link;
	shp_data_t *item = at(item_index);
	shp_hdr->free_link = item->next;
	int bucket = uhash::hashname(svc_key.c_str(), shp_hdr->hash_size);

	item->flags = SHP_INUSE_FLAG;
	item->ref_count = 1;
  	item->prev = -1;
	item->next = hash_table[bucket];
	item->bucket = bucket;
	hash_table[bucket] = item_index;
	::strcpy(item->key_name, svc_key.c_str());
	::memcpy(&item->data, data, data_len);

	unlock();
	return item;
}

int shm_pair::erase(shp_data_t *shp_data)
{
	if (shp_data < at(0) || shp_data >= at(shp_hdr->data_size)) {
		GPT->_GPT_error_code = GPEINVAL;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Invalid address provided.")).str(SGLOCALE) );
		return SHM_PAIR_ERROR;
	}

	lock();

	if (--shp_data->ref_count > 0) {
		unlock();
		return SHM_PAIR_LOGICAL;
	}

	// Remove from link
	if (shp_data->prev != -1) { // Not first node
		shp_data_t *prev = at(shp_data->prev);
		prev->next = shp_data->next;
	} else {
		hash_table[shp_data->bucket] = shp_data->next;
	}

	if (shp_data->next != -1) {
		shp_data_t *next = at(shp_data->next);
		next->prev = shp_data->prev;
	}

	// Add to free link
	shp_data->next = shp_hdr->free_link;
	shp_hdr->free_link = (reinterpret_cast<char *>(shp_data) - shp_table) / (offsetof(shp_data_t, data) + shp_hdr->data_len);

	unlock();
	return SHM_PAIR_PHYSICAL;
}

void shm_pair::lock()
{
	shp_hdr->lock.lock();
}

void shm_pair::unlock()
{
	shp_hdr->lock.unlock();
}

shp_data_t * shm_pair::at(int index)
{
	return reinterpret_cast<shp_data_t *>(shp_table + (offsetof(shp_data_t, data) + shp_hdr->data_len) * index);
}

}
}

