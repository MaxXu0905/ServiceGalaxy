#if !defined(__SG_HANDLE_H__)
#define __SG_HANDLE_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

class sg_handle : public sg_manager
{
public:
	static sg_handle& instance(sgt_ctx *SGT);

	int mkhandle(sg_message& msg, int flags);
	bool chkhandle(sg_message& msg);
	void drphandle(sg_message& msg);
	int mkstale(sg_message *msg);
	void rcvstales();
	void rcvstales_internal(int count);
	void rcvcstales();
	int mkprios(sg_message& msg, sg_scte_t *scte, int flag);
	int msg2cd(sg_message& msg);

private:
	sg_handle();
	virtual ~sg_handle();

	int startpoint;
	int check4stales;
	message_pointer rcvstales_sysmsg;
	message_pointer rcvcstales_sysmsg;

	friend class sgt_ctx;
};

}
}

#endif

