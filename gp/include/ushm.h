#if !defined(__USHM_H__)
#define __USHM_H__

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <vector>
#include "gpenv.h"
#include "gpp_ctx.h"
#include "gpt_ctx.h"
#include "usignal.h"
#include "uunix.h"

namespace ai
{
namespace sg
{

int const USHM_MAGIC = 0x1BAD1DEA;
int const SEM_NOWAIT = 0;
int const SEM_WAIT = 1;

struct shminf_t
{
	int shmid;
	int refcnt;
	void *shmaddr;
};

// Cache Entry
struct ushm
{
	key_t key; // base key
	char	*adr; // beginning of segment address
	size_t smsize; // size of extended shared memory
	int cnt; // count of ids; 0 if malloc'ed
	int *ids; // identifiers for shared memory segments
};

// Indirect Segment Structure
struct ushm_map
{
	long magic;	/* magic number to distinuish indirect segments */
	size_t size;	/* total size of extended shared memory */
	int *ids;		/* array of extended memory ids attached here */
};

class ipc_msg
{
public:
	ipc_msg(key_t key, int msgflg);
	~ipc_msg();
	int msgctl(int cmd, msqid_ds *buf);
	int msgsnd(msgbuf *msgp, int msgsz, int msgflg);
	int msgrcv(msgbuf *msgp, int msgsz, long msgtyp, int msgflg);

private:
	key_t key;
	int msq_id;
};

class ipc_sem
{
public:
	ipc_sem(key_t key, int nsems, int semflg);
	~ipc_sem();
	int semctl(int semnum, int cmd, semun un);
	int semop(sembuf *sops, int nsops);

	static void semclear(int *word);
	static int issemclear(int word);
	static void initsem(int *word);
	static int tas(int *x, int flag);

private:
	key_t key;
	int sem_id;
};

class ipc_shm
{
public:
	ipc_shm();
	~ipc_shm();
	int shmget(key_t key, size_t smsize, int shmflg);
	char *shmat(char *shmaddr, int shmflg);
	int shmdt(char *shmadr);
	int shmctl(int cmd, shmid_ds *buf);
	int shmch(key_t key, int **ids, int *idcount);
	int shmdlt(int ids[], int idcount);
	int get_shm_id() const;

private:
	void* ll_shmat(int shm_id, void* shmaddr, int shmflg);
	int ll_shmdt(void *shmaddr);
	int setup();

	key_t key;
	int shm_id;
	gpp_ctx *GPP;
	gpt_ctx *GPT;

	static boost::mutex shm_mutex;
	static std::vector<shminf_t> shm_db;
	static std::vector<ushm> ushm_list;
};

}
}

#endif

