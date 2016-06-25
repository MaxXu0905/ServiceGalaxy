#if !defined(__GPT_CTX_H__)
#define __GPT_CTX_H__

#include "common.h"
#include "log_file.h"
#include <boost/thread.hpp>

namespace ai
{
namespace sg
{

class gpt_ctx
{
public:
	static gpt_ctx * instance();
	~gpt_ctx();

	int _GPT_error_code;
	int _GPT_native_code;

private:
	gpt_ctx();

	static boost::thread_specific_ptr<gpt_ctx> _instance;
};

}
}

#endif

