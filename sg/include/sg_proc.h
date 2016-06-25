#if !defined(__SG_PROC_H__)
#define __SG_PROC_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace ai
{
namespace sg
{

/* sync_proc() rsync_proc() error return codes */
int const BSYNC_OK = 0;				/* Booted ok */
int const BSYNC_DUPENT = -1;		/* Duplicate entry */
int const BSYNC_EXECERR = -2;		/* Error execing */
int const BSYNC_FORKERR = -3;		/* Error forking */
int const BSYNC_NOBBM = -4;			/* No sgmngr available */
int const BSYNC_NODBBM = -5;		/* No sgclst available */
int const BSYNC_NRCVERR = -6;		/* Network rcv error, remote sync only */
int const BSYNC_NSNDERR = -7;		/* Network send error, remote sync only */
int const BSYNC_PIPERR = -8;		/* Error synchronizing over pipes */
int const BSYNC_UNK = -9;			/* Unknown error */
int const BSYNC_APPINIT = -11;		/* Application tpsvrinit() failure */
int const BSYNC_NEEDPCLEAN = -12;	/* sgmngr boot failure .. pclean may help */
int const BSYNC_TIMEOUT = -13;		/* fork/exec/tpsvrinit took too long */

int const SP_SYNC = 0x1;			/* Synchronize with forked process */
const char PARENT_FAIL[] = "9";		/* sent by parent to restarting child waiting on pipe */

int const SGREMOVEQ = 0x1;
int const SGSETQ = 0x2;
int const SGRESETQ = 0x4;
int const SGSTATQ = 0x8;

struct sg_wkinfo_t
{
	int magic;
	pid_t pid;
	int qaddr;

	sg_wkinfo_t() {
		magic = VERSION;
		pid = 0;
		qaddr = 0;
	}
};

class sg_proc : public sg_manager
{
public:
	static sg_proc& instance(sgt_ctx *SGT);

	bool create_wkid(bool amdbbm);
	int get_wkid();
	int getq(int mid, int qaddr, size_t max_num_msg = 0, size_t max_msg_size = 0, int perm = 0);
	void insertq(int qaddr, pid_t pid);
	bool updateq(const sg_ste_t& ste);
	void removeq(int qaddr);
	bool get_wkinfo(int wka, sg_wkinfo_t& wkinfo);
	bool alive(const sg_proc_t& proc);
	bool nwkill(const sg_proc_t& proc, int signo, int timeout);
	bool nwqctl(const sg_qte_t& qte, int cmd, int *arg);
	int sync_proc(char **argv, const sg_mchent_t& mchent, sg_ste_t *ste, int flags, pid_t& pid, bool (*ignintr)());
	int rsync_proc(char **argv, const sg_mchent_t& mchent, int flags, pid_t& pid);
	bool stat(const string& name, struct stat& buf);
	bool chown(const std::string& name, uid_t uid, gid_t gid);
	bool chmod(const std::string& name, mode_t mode);

private:
	sg_proc();
	virtual ~sg_proc();

	friend class sgt_ctx;
};

}
}

#endif

