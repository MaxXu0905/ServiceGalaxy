#include <dlfcn.h>
#include "shm_compiler.h"

namespace ai
{
namespace sg
{

const char * shm_compiler::com_key = "SHM_COMPILER";

shm_compiler::shm_compiler(shm_malloc& shm_mgr, const string& svc_key_, search_type_enum search_type_, int hash_size, int data_size)
	: compiler(search_type_, false),
	  svc_key(svc_key_)
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();
	shp = new shm_pair(shm_mgr, com_key, hash_size, data_size);

	// Try to find data by svc_key.
	shp_data = shp->find(svc_key);
	if (shp_data) {
		shm_func_t *sf = reinterpret_cast<shm_func_t *>(shp_data->get_data());
		set_func(sf->func_size, sf->lib_name);
	}
}

shm_compiler::~shm_compiler()
{
	if (shp_data) {
		if (shp->erase(shp_data) == shm_pair::SHM_PAIR_PHYSICAL) // Erase physically
			cleanup();
		shp_data = NULL;
	}

	delete shp;
}

bool shm_compiler::valid() const
{
	return (shp_data != NULL && shp_data->valid());
}

void shm_compiler::reset()
{
	if (fd != -1) {
		::close(fd);
		fd = -1;
	}

	if (handle) {
		::dlclose(handle);
		handle = NULL;
	}

	if (do_cleanup)
		cleanup();

	func_size = 0;
	inited = false;
	built = false;

	if (shp_data) {
		if (shp->erase(shp_data) == shm_pair::SHM_PAIR_PHYSICAL) // Erase physically
			cleanup();
		shp_data = NULL;
	}
}

void shm_compiler::put()
{
	shm_func_t sf;

	if (!built) {
		GPT->_GPT_error_code = GPEPROTO;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Must call build() first before put().")).str(SGLOCALE) );
		::exit(1);
	}

	string lib_name = file_name.substr(0, file_name.length() - 1) + "so";
	if (lib_name.length() > LIB_NAME_LEN) {
		GPT->_GPT_error_code = GPEINVAL;
		GPP->write_log(__FILE__, __LINE__,(_("ERROR: Lib name length too long")).str(SGLOCALE) );
		::exit(1);
	}

	sf.func_size = func_size;
	::strcpy(sf.lib_name, lib_name.c_str());

	shp_data = shp->put(svc_key, &sf, sizeof(sf));
}

int shm_compiler::inc_index()
{
	return func_size++;
}

}
}

