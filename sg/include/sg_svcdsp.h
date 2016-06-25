#if !defined(__SG_SVCDSP_H__)
#define __SG_SVCDSP_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

class sg_svcdsp : public sg_manager
{
public:
	static sg_svcdsp& instance(sgt_ctx *SGT);

	void svcdsp(message_pointer& msg, int flags);
	int svr_done();

private:
	sg_svcdsp();
	virtual ~sg_svcdsp();

	sg_svc * strt_work(sg_message& msg);
	bool done_work();
	sg_svc * get_svc(sg_ste_t *step, sg_message& msg);

	friend class sgt_ctx;
};

}
}

#endif

