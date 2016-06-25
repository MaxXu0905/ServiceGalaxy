#if !defined(__SHM_COMPILER_H__)
#define __SHM_COMPILER_H__

#include "common.h"
#include "dstream.h"
#include "compiler.h"
#include "shm_pair.h"
#include "gpenv.h"

namespace ai
{
namespace sg
{

using namespace std;

int const LIB_NAME_LEN = 1023;

struct shm_func_t
{
	int func_size;
	char lib_name[LIB_NAME_LEN + 1];
};

class shm_compiler : public compiler
{
public:
	shm_compiler(shm_malloc& shm_mgr_, const string& svc_key_, search_type_enum search_type_ = SEARCH_TYPE_NONE, int hash_size = 0, int data_size = 0);
	~shm_compiler();

	bool valid() const;
	void reset();
	void put();
	int inc_index();

private:
	string svc_key;
	shm_pair *shp;
	shp_data_t *shp_data;

	gpp_ctx *GPP;
	gpt_ctx *GPT;

	static const char *com_key;
};

}
}

#endif

