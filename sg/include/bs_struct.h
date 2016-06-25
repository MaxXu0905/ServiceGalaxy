#if !defined(__BS_STRUCT_H__)
#define __BS_STRUCT_H__

namespace ai
{
namespace sg
{

int const BS_IGCLIENTS = 0x00001;	/* Shutdown ignore clients */
int const BS_KILLBB = 0x00002;		/* Remove BB resources after shut */
int const BS_NOERROR = 0x00004;		/* Ignore process failures */
int const BS_NOEXEC = 0x00008;		/* Run in noexec mode */
int const BS_NOWAIT = 0x00010;		/* Don't wait for sync on boot */
int const BS_RELOC = 0x00020;		/* Shutdown for migration */
int const BS_VERBOSE = 0x00040;		/* Verbose output */
int const BS_BOOTING = 0x00080;		/* Boot vs. shutdown */
int const BS_HIDDEN = 0x00200;		/* Hidden option trigger -% */
int const BS_ALLADMIN = 0x00400;	/* Boot/shut all admin processes */
int const BS_ALLSERVERS = 0x00800;	/* Boot/shut all server processes */
int const BS_HAVEDBBM = 0x1000;		/* Boot flag if sgclst booted */
int const BS_PROCINTS = 0x02000;	/* Processed interrupts flag */
int const BS_ANYNODE = 0x04000;		/* allow to run on any node */
int const BS_PARTITION = 0x08000;	/* Shutdown partitioned node flag */
int const BS_HIDERR = 0x10000;		/* Don't print errors for process */
int const BS_SUSPENDED = 0x20000;	/* Boot all in suspended mode */
int const BS_SUSPAVAIL = 0x40000;	/* Boot server in suspended mode */
int const BS_EXCLUDE = 0x80000;		/* Do not shutdown sgtgws */

// who to  boot
int const BF_DBBM = 0x1;		// boot sgclst
int const BF_BBM = 0x02;		// boot sgmngr
int const BF_ADMIN = BF_DBBM | BF_BBM;	// boot admin processes
int const BF_SERVER = 0x04;	// boot server proceses
int const BF_BSGWS = 0x08;		// boot sghgws
int const BF_GWS = 0x10;		// boot sggws

// boot flags
int const BF_RESTART = 0x1;		// restarting server
int const BF_MIGRATE = 0x2;		// migrating server
int const BF_REPLYQ = 0x4;		// separate reply queue
int const BF_HAVEQ = 0x8;		// already assigned request queue
int const BF_SPAWN = 0x10;		// spawned server
int const BF_SUSPENDED = 0x20;	// boot in suspended mode
int const BF_SUSPAVAIL = 0x40;	// Boot server in suspended mode, and make it available after svrinit
int const BF_PRESERVE_FD = 0x80;// preserve file descriptors

int const CT_SVRGRP = 1;	/* Server group id #, (NOT Name) */
int const CT_SVRID = 2;		/* Server id # */
int const CT_LOC = 3;		/* Server LMID, string value */
int const CT_SVRNM = 4;		/* Server a.out name, string value */
int const CT_NODE = 5;		/* Entire node shutdown, string value (LMID) */
int const CT_SEQ = 6;		/* Server sequence number */
int const CT_GRPNAME = 7;	/* Server group name, string value */
int const CT_MCHID = 8;		/* Server machine id, MCHID type */
int const CT_DONE = 9;		/* Entry processed */

struct sg_tditem_t
{
	int ptype;		// process type
	int ctype;		// search criteria type
	string name;	// name of process
	string clopt;	// clopt
	int flags;		// item flags

	const int& ct_mid() const { return c_int; }
	int& ct_mid() { return c_int; }
	const int& ct_int() const { return c_int; }
	int& ct_int() { return c_int; }
	const string& ct_str() const { return c_str; }
	string& ct_str() { return c_str; }

private:
	int c_int;		// integer value
	string c_str;	// string value
};

}
}

#endif

