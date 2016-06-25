#if !defined(__SG_META_H__)
#define __SG_META_H__

#include "sg_struct.h"
#include "sg_message.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

// various flags passed from sgshutdown to sgmngr's
int const SD_KEEPBB = 0x1;		/* don't remove IPC resources */
int const SD_RELOC = 0x2;		/* relocating servers */
int const SD_IGNOREC = 0x4;		/* ignore clients */

class sg_meta : public sg_manager
{
public:
	static sg_meta& instance(sgt_ctx *SGT);

	bool metasnd(message_pointer& msg, sg_proc_t *where, int flags);
	bool metarcv(message_pointer& msg);

private:
	sg_meta();
	virtual ~sg_meta();

	bool doadv(message_pointer& msg);
	bool doshut(message_pointer& msg);
	int getqcnt();
	bool failure(message_pointer& msg, const string& problem);
	bool RETURN(message_pointer& msg, int status);

	friend class sgt_ctx;
};

}
}

#endif

