#if !defined(__SG_BOOT_H__)
#define __SG_BOOT_H__

#include "bs_base.h"

namespace ai
{
namespace sg
{

class sg_boot : public bs_base
{
public:
	sg_boot();
	~sg_boot();

protected:
	virtual int do_admins();
	virtual int do_servers();

private:
	int startadmin(sg_svrent_t& svrent, int who);
	int startproc(sg_svrent_t& svrent, int ptype);

	bool set_bbm(int ptype, int mid, bool value);
	void setgroup(int mid, int grpid);
	int getgroup(int grpid);
	bool doprocloc(const sg_proc_t& proc, vector<int>& ml);

	boost::shared_array<bool> bbm_fail;	// set true if fail
	bool dbbm_fail;						// set true if fail
};

}
}

#endif

