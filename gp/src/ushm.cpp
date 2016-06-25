#include "ushm.h"
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/format.hpp>

namespace bi = boost::interprocess;

#if defined(HPUX)

extern "C" int _gp_tas(int *x);
extern "C" void _gp_semclear(int *word);

#elif defined(AIX)

extern "C" int cs(int *x, int a, int b);

#endif

namespace ai
{
namespace sg
{

boost::mutex ipc_shm::shm_mutex;
vector<shminf_t> ipc_shm::shm_db;
vector<ushm> ipc_shm::ushm_list;

#if defined(AIX)
void _sync_op();
#pragma mc_func _sync_op { "7c0004ac"  /* sync */ }
#endif

ipc_msg::ipc_msg(key_t key, int msgflg)
{
	this->key = key;
	msq_id = ::msgget(key, msgflg);
}

ipc_msg::~ipc_msg()
{
}

int ipc_msg::msgctl(int cmd, msqid_ds *buf)
{
	return ::msgctl(msq_id, cmd, buf);
}

int ipc_msg::msgsnd(msgbuf *msgp, int msgsz, int msgflg)
{
	return ::msgsnd(msq_id, msgp, msgsz, msgflg);
}

int ipc_msg::msgrcv(msgbuf *msgp, int msgsz, long msgtyp, int msgflg)
{
	return ::msgrcv(msq_id, msgp, msgsz, msgtyp, msgflg);
}

ipc_sem::ipc_sem(key_t key, int nsems, int semflg)
{
	this->key = key;
	sem_id = ::semget(key, nsems, semflg);
}

ipc_sem::~ipc_sem()
{
}

int ipc_sem::semctl(int semnum, int cmd, semun un)
{
	return ::semctl(sem_id, semnum, cmd, un);
}

int ipc_sem::semop(sembuf *sops, int nsops)
{
	return ::semop(sem_id, sops, nsops);
}

void ipc_sem::semclear(int *word)
{
#if defined(AIX)

	_sync_op();
	*((volatile int *) word) = 0;
	_sync_op();

#elif defined(SOLARIS)

#ifdef __sparcv9
	asm("ldx [%fp+0x7f7],%o0");
#else
	asm("ld [%fp+0x44],%o0");   /* x into reg o0 */
#endif
	/* g0 always returns 0 */
	asm("stub %g0,[%o0]");      /* clear the lock */
	asm("ldstub [%sp],%g0")

#elif defined(HPUX)

	_gp_semclear(word);

#else

#if defined(__amd64)
	asm("   movl    $0,%eax");  /* this 0 inworking reg will set *x */
	asm("   xchgl   %eax,(%rdi)");  /* atomic swap a one with *x */
#else
	asm("	movl	$0,%eax");	/* this 0 inworking reg will set *x */
	asm("	pushl	%ebx");		/* save the register; eax already saved */
	asm("	movl	8(%ebp),%ebx");	/* move the address of x */
	asm("	xchg	%eax,(%ebx)");	/* atomic swap a one with *x */
	asm("	popl	%ebx");		/* restore the register */
#endif

#endif
}

int ipc_sem::issemclear(int word)
{
	if (word == 0)
		return 1;
	else
		return 0;
}

void ipc_sem::initsem(int *word)
{
	semclear(word);
}

int ipc_sem::tas(int *x, int flag)
{
#ifdef __STDC__
	volatile int r; /* volatile so optimizers won't get
					 * rid of variable - some assembly code
					 * depends on offsets with this on
					 * the stack.
					 */
#else
	int r;			/* var to return explicitly */
#endif

#if defined(AIX)
	do {
		/* AIX BOS Runtime Subroutine - compare and swap data */
		r = cs(x, 0, 1) ? 0 : 1;
	} while (!r && flag);

#elif defined(SOLARIS)

	r = 0;

	do {
#ifdef __sparcv9
		asm("ldx [%fp+0x7f7],%o0");
#else
		asm("ld [%fp+0x44],%o0");	/* load x from stack into reg o0 */
#endif
						/* ldstub stores FF in the src */
		asm("ldstub [%o0],%l0");	/* load x into reg L0 & set o0 */
		asm("tst %l0");
		asm("bne 0f");			/* x not zero ==> the lock is taken */
		asm("nop");			/* stuff in "delay slot" */

		asm("1:");			/* label for success exit */
		r = 1;

		asm("0:");			/* label for failure exit */
	} while (!r && flag);

#elif defined(HPUX)

	do {
		r = _gp_tas(x);
	} while (!r && flag);

#else

	r = 0;				/* always set return to 0   */

	do {
#if defined(__amd64)
		asm("   movl    $1,%eax");  /* this 1 inworking reg will set *x */
		asm("   xchgl   %eax,(%rdi)");  /* atomic swap a one with *x */
		asm("   cmpl    $0,%eax");  /* test what was in *x */
		asm("	jnz	.A9999");	/* jump if *x was already set */
		r = 1;				/* indicate set occurred */
		asm("	.A9999:");
#else
		asm("	movl	$1,%eax");	/* this 1 inworking reg will set *x */
		asm("	pushl	%ebx");		/* save the register; eax already saved */
		asm("	movl	8(%ebp),%ebx");	/* move the address of x */
		asm("	xchg	%eax,(%ebx)");	/* atomic swap a one with *x */
		asm("	cmp	$0,%eax");	/* test what was in *x */
		asm("	jnz	.A9999");	/* jump if *x was already set */
		r = 1;				/* indicate set occurred */
		asm("	.A9999:");
		asm("	popl	%ebx");		/* restore the register */
#endif
	} while (!r && flag);

#endif

	return r;
}

ipc_shm::ipc_shm()
{
	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();
}

ipc_shm::~ipc_shm()
{
}

int ipc_shm::shmget(key_t key, size_t smsize, int shmflg)
{
	bool exists = false;
	int count = 0;
	int cr = 0;
	int *shm_ids;
	ushm_map  *mp = NULL;
	vector<ushm>::iterator iter;

	this->key = key;

	bi::scoped_lock<boost::mutex> lock(shm_mutex);

	if (key == IPC_PRIVATE) {
		/// treat it as if it doesn't exist - also implies create
		iter = ushm_list.end();
		shmflg |= IPC_CREAT;
	} else {
		// remember the entry behind the found one for ease of unlinking
		iter = ushm_list.begin();
	}

	for (; iter != ushm_list.end(); ++iter) {
		if (iter->key == key) {
			exists = true;
			break;
		}
	}

	if (exists) {
		if (smsize == 0) {
			smsize = iter->smsize;
		} else if (iter->smsize != smsize) {
			// Cache entry doesn't agree, void it.
			delete [] iter->ids;
			iter->ids = NULL;
			exists = false;
		}
	} else {
		ushm ushm_mgr;
		ushm_mgr.ids = NULL;
		ushm_list.push_back(ushm_mgr);
		iter = ushm_list.begin() + ushm_list.size() - 1;
	}

	// 'up' now points to current entry or new one
	int idslen;
	int maplen;
	bool indirect = false;
	int remlen;
	int len;

	// Determine whether indirect segment is being used
	if (key == IPC_PRIVATE) {
		shm_id = -1;
	} else if ((shm_id = ::shmget(key, 0, 0)) == -1) {
		if (errno != ENOENT) {	// nothing exists yet
			GPT->_GPT_native_code = USHMGET;
			goto abend;
		}
	} else {
		size_t sz;
		bool attached = false;
		shmid_ds buf;

		// Verify size
		if (::shmctl(shm_id, IPC_STAT, &buf) == -1) {
			GPT->_GPT_native_code = USHMCTL;
			goto abend;
		}

		// Precaution for references thru mp
		if (buf.shm_segsz > sizeof(ushm_map)) {
			if ((mp = reinterpret_cast<ushm_map *>(::shmat(shm_id, 0, 0))) == reinterpret_cast<ushm_map *>(-1)) {
				GPT->_GPT_native_code = USHMAT;
				goto abend;
			}
			attached = true;
			if (mp->magic == USHM_MAGIC)
				indirect = true;
		}
		/*
		 * Enforce the UNIX semantics that larger sizes fail,
		 * and smaller sizes are ok. Must get the size from the
		 * right source.
		 */
		sz = indirect ? mp->size : buf.shm_segsz;
		if (smsize > sz) {
			if (attached)
				::shmdt(mp);
			errno = EINVAL;
			GPT->_GPT_native_code = USHMGET;
			goto abend;
                } else if (indirect || !exists) {
                        smsize = sz;
                }

		if (attached) {
			if (::shmdt(mp) == -1) {
				GPT->_GPT_native_code = USHMGET;
				goto abend;
			}
		}
	}

	/*
	 * The number of segments can never be 2: either a direct segment
	 * is used, and count == 1; or, an indirect segment is used and
	 * at least two other segments exist. In the latter case, count
	 * must be bumped to take into account the indirect segment.
	 */
	if (smsize == 0)
		smsize = 1;

	count = (smsize + _TMSHMSEGSZ - 1) / _TMSHMSEGSZ;  // number of segments
	if (count > 1)
		count++;
	idslen = count * sizeof(int);
	// maplen only used with indirect, thus subtract one int
	maplen = sizeof(ushm_map) - sizeof(mp->ids) + idslen - sizeof(int);

	if (!exists) {
		iter->ids = new int[idslen];
		if (iter->ids == NULL) {
			errno = ENOMEM;
			GPT->_GPT_native_code = USBRK;
			goto abend;
		}
	}
	shm_ids = iter->ids;

	/*
	 * If getting only one segment (i.e., no indirection), use
	 * size of segment. Otherwise, get the indirect segment with
	 * size equal to the map plus the ids.
	 */
	len = (count <= 1) ? smsize : maplen;

	/*
	 * Ensure that caller either creates all or none of the
	 * segments. Must play with the flags a bit (no pun) first.
	 */
	shm_id = -1;
	if ( (!(shmflg & IPC_CREAT) || !(shmflg & IPC_EXCL)) && key != IPC_PRIVATE) {
		// try to get id without creating it
		if ((shm_id = ::shmget(key, len, shmflg & ~IPC_CREAT)) == -1) {
			if (errno != ENOENT || !(shmflg & IPC_CREAT)) {
				GPT->_GPT_native_code = USHMGET;
				goto abend;
			}
		} else {
			// Found it: turn off create flag for all future segs
			shmflg &= ~IPC_CREAT;
		}
	}

	if (shm_id == -1) {
    		// doesn't exist - must create it
    		shmflg |= IPC_EXCL;
    		if ((shm_id = ::shmget(key, len, shmflg)) == -1) {
			GPT->_GPT_native_code = USHMGET;
			goto abend;
    		}
    		++cr; // count created segments
	}
	*shm_ids = shm_id;

	if (count <= 1)  { // easy part: no indirection
		goto done;
	} else if (shmflg & IPC_CREAT) {
		// Create all other segments with IPC_PRIVATE
		remlen = smsize;

#ifdef _TMMEMGROWSUP
		/* allocate partial segment at end - lowest address first */
		len = std::min(smsize, _TMSHMSEGSZ);
#else
		/*
		 * DEFAULT: allocate partial segment first - highest
		 * address allocated first.
		 */
		len = ( (smsize-1) % _TMSHMSEGSZ ) + 1;
#endif
		for(int i = 1; i < count; i++) {
			if ((*(++shm_ids) = ::shmget(IPC_PRIVATE, len, shmflg)) == -1) {
				GPT->_GPT_native_code = USHMGET;
				goto abend;
			}
			++cr;
			remlen -= len; // reset remaining length
			len = std::min(remlen, static_cast<int>(_TMSHMSEGSZ));
		}
	}

	/*
	 * Attach to segment which holds ids, and either get
	 * them, or populate the segment with the ids. Note that
	 * the first id is not in the indirect segment.
	 */
	if ((mp = reinterpret_cast<ushm_map *>(::shmat(*iter->ids, 0, 0))) == reinterpret_cast<ushm_map *>(-1)) {
		GPT->_GPT_native_code = USHMAT;
		goto abend;
	}

	if (!(shmflg & IPC_CREAT)) {
		// copy ids from shmem to process space
		::memcpy(reinterpret_cast<char *>(iter->ids + 1), reinterpret_cast<char *>(&mp->ids), idslen - sizeof(int));
	} else {
		/* set up indirect segment & copy ids into shmem */
		mp->magic = USHM_MAGIC;
		mp->size = smsize;
		::memcpy(reinterpret_cast<char *>(&mp->ids), reinterpret_cast<char *>(iter->ids + 1), idslen - sizeof(int));
	}

	if (::shmdt(mp) < 0) {
		GPT->_GPT_native_code = USHMDT;
		goto abend;
	}

done:
	if (!exists) {
		iter->key = key;
		iter->adr = NULL;
		iter->smsize = smsize;
		iter->cnt = count;
	}
	return *(iter->ids);

abend:
	// if segment(s) just created, delete
	for (shm_ids = iter->ids; cr > 0; cr--, shm_ids++)
		::shmctl(*shm_ids, IPC_RMID, NULL);
	delete []iter->ids;
	ushm_list.erase(iter);
	return -1;
}

char *ipc_shm::shmat(char *shmaddr, int shmflg)
{
	int *shm_idp;
	int i;
	int count;
	vector<ushm>::iterator iter;
	caddr_t attach_addr;
	bi::scoped_lock<boost::mutex> lock(shm_mutex);

	for (iter = ushm_list.begin();  iter != ushm_list.end(); ++iter) {
		if (*(iter->ids) == shm_id)
			break;
	}
	if (iter == ushm_list.end()) {
		if (setup() < 0) {
			errno = EINVAL;
			GPT->_GPT_native_code = USHMAT;
			return reinterpret_cast<char *>(-1);
		}
		iter = ushm_list.begin() + ushm_list.size() - 1;
	}

	if (iter->cnt == 1) { // attach directly to segment
		count = 1;
		shm_idp = iter->ids;
	} else { // don't attach to indirect segment
		count = iter->cnt - 1;
		shm_idp = iter->ids + 1;
	}

	auto_ptr<char *> auto_shmadr(new char *[count + 1]);
	char **shmadr = auto_shmadr.get();
	if (shmadr == NULL) {
		errno = ENOMEM;
		GPT->_GPT_native_code = USBRK;
		return reinterpret_cast<char *>(-1);
	}

	/*
	 * use mmap() to grab a large enough address
	 * space within the process to ensure contiguous
	 * addressing for indirect segments during shmat
	 */
#if defined(LINUX)
	attach_addr = reinterpret_cast<char *>(::mmap(0, count * _TMSHMSEGSZ, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
#else
	int devzero_fd = ::open("/dev/zero", O_RDONLY);
	if (devzero_fd == -1) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Open /dev/zero failed, errno = {1}") % errno).str(SGLOCALE));
		return reinterpret_cast<char *>(-1);
	}

	attach_addr = reinterpret_cast<char *>(::mmap(0, count * _TMSHMSEGSZ, PROT_READ, MAP_PRIVATE, devzero_fd, 0 ));
	::close(devzero_fd);
#endif

	if (attach_addr == reinterpret_cast<char *>(-1)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: process mmap failed, errno = {1}") % errno).str(SGLOCALE));
		return reinterpret_cast<char *>(-1);
	}

	// Now unmap the memory to free it up for our use with shmat
	if (::munmap(attach_addr, count * _TMSHMSEGSZ) == -1) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: process unmmap failed, errno = {1}") % errno).str(SGLOCALE));
		return reinterpret_cast<char *>(-1);
	}

	for (i = 0; i < count; i++, shm_idp++) {
		// Need to insure that all segments will be contiguous in the address space.
		if (i == 0) {
#ifdef _TMMEMGROWSUP
			shmaddr = reinterpret_cast<char*>(attach_addr);
#else
			if (shmaddr != 0)
				shmaddr += (count - 1) * _TMSHMSEGSZ;
			else
				shmaddr = reinterpret_cast<char*>(attach_addr + ((count - 1) * _TMSHMSEGSZ));
#endif
			if ((shmadr[i] = reinterpret_cast<char*>(ll_shmat(*shm_idp, shmaddr, shmflg))) == reinterpret_cast<char *>(-1))
				goto abend;
		} else {
#ifdef _TMMEMGROWSUP
			// allocate at next segment address
			if ((shmadr[i] = reinterpret_cast<char*>(ll_shmat(*shm_idp, shmadr[i - 1] + _TMSHMSEGSZ, shmflg & ~SHM_RND)))
				== reinterpret_cast<char *>(-1))
				goto abend;
#else
			// allocate at preceding segment address
			if ((shmadr[i] = reinterpret_cast<char*>(ll_shmat(*shm_idp, shmadr[i - 1] - _TMSHMSEGSZ, shmflg & ~SHM_RND)))
				== reinterpret_cast<char *>(-1))
				goto abend;
#endif
		}
	}

#ifdef _TMMEMGROWSUP
	// data section grows toward end of memory - thus the address of the first segment allocated is desired
	iter->adr = shmadr[0];
#else
	// DEFAULT:  data section grows toward lower addresses - thus the address of the last segment allocated is desired.
	iter->adr = shmadr[i - 1];
#endif

	return iter->adr;

abend: // one of attaches failed - detach any that were just attached
	int err = errno;
	while (i > 0)
		ll_shmdt(shmadr[--i]);
	errno = err;
	GPT->_GPT_native_code = USHMAT;
	return reinterpret_cast<char *>(-1);
}

int ipc_shm::shmdt(char *shmadr)
{
	vector<ushm>::iterator iter;
 	int count;
	int err = 0;
	bi::scoped_lock<boost::mutex> lock(shm_mutex);

	for (iter = ushm_list.begin(); iter != ushm_list.end(); ++iter) {
		if (iter->adr == shmadr)
			break;
	}

	if (iter == ushm_list.end())
		return -1;

	/*
	 * The reason the detaches don't worry about which way
	 * the memory was attached is due to the pains taken in
	 * _gp_shmget & _gp_shmat to ensure this specific order.
	 */
	count = (iter->cnt == 1) ? 1 : iter->cnt - 1;
	for (int i = 0; i < count; i++) {
		/* on some platforms (namely AS/400), an address
		 * pointer is invalidated as soon as the object
		 * it points to goes away. So, the value of shmadr
		 * is undefined after the shmdt call.
		 */
		char *nextadr = shmadr + _TMSHMSEGSZ;

		if (ll_shmdt(shmadr) == -1)
			err = -1;

		shmadr = nextadr;
	}

	return err;
}

int ipc_shm::shmctl(int cmd, shmid_ds *buf)
{
	int i;
	int err = 0;
	int *shm_idp;
	vector<ushm>::iterator iter;
	bi::scoped_lock<boost::mutex> lock(shm_mutex);

	for (iter = ushm_list.begin(); iter != ushm_list.end(); ++iter) {
		if (*(iter->ids) == shm_id)
			break;
	}
	if (iter == ushm_list.end()) {
		if (setup() < 0) {
			errno = EINVAL;
			GPT->_GPT_native_code = USHMAT;
			return -1;
		}
		iter = ushm_list.begin() + ushm_list.size() - 1;
	}

	// All segments are affected by call.
	for (i = 0, shm_idp = iter->ids; i < iter->cnt; i++, shm_idp++) {
		if (::shmctl(*shm_idp, cmd, buf) == -1)
			err = 1;
	}

	if (err)
		return -1;

	if (cmd == IPC_RMID) {
		if (iter->ids)
			delete []iter->ids;
		ushm_list.erase(iter);
	} else if (cmd == IPC_STAT) {
#if defined(__linux__)
		buf->shm_perm.__key = iter->key;
#else
		buf->shm_perm.key = iter->key;
#endif
		buf->shm_segsz = iter->smsize;
	}

 	return 0;
}

/*
 * Check to see what id's exist. It is the caller's responsibility to free
 * the space which it gets upon successsful return.
 */
int ipc_shm::shmch(key_t key, int **ids, int *idcount)
{
	vector<ushm>::iterator iter;
	int id;
	int sz = 0;
	bool indirect = false;
	bool attached = false;
	shmid_ds buf;
	ushm_map *mp = NULL;
	bi::scoped_lock<boost::mutex> lock(shm_mutex);

	if (key == IPC_PRIVATE) {
		for (iter = ushm_list.begin(); iter != ushm_list.end(); ++iter) {
			if (iter->key == key && iter->ids != NULL)
				break;
		}
		if (iter == ushm_list.end()) {
			errno = ENOENT;
			GPT->_GPT_native_code = USHMGET;
			return -1;
		}
		id = *iter->ids;
	} else {
		if ((id = ::shmget(key, 0, 0)) == -1) {
			GPT->_GPT_native_code = USHMGET;
			return -1;
		}
	}
	if (::shmctl(id, IPC_STAT, &buf) == -1) {
		GPT->_GPT_native_code = USHMAT;
		return -1;
	}
	if (buf.shm_segsz > sizeof(ushm_map)) {
		mp = reinterpret_cast<ushm_map *>(ll_shmat(id, 0, 0));
 		attached = true;
		if (mp->magic == USHM_MAGIC)
			indirect = true;
	}
	if (indirect)
		sz = buf.shm_segsz - (sizeof(mp->magic) + sizeof(mp->size));

	*ids = new int[sz + 1];
	if (*ids == NULL) {
		if (attached)
			ll_shmdt(mp);
		errno = ENOMEM;
		GPT->_GPT_native_code = USBRK;
	}
	**ids = id;
	if (indirect)
		::memcpy(*ids + 1, &mp->ids, sz);

	if (attached) {
		if (ll_shmdt(mp) == -1) {
			GPT->_GPT_native_code = USHMDT;
			return -1;
		}
	}

	*idcount = sz / sizeof(int) + 1;
	return 0;
}

/*
 * Delete segments associated with identifers -
 * invalidate identifiers of segments successfully deleted.
 * Also, if a cache entry exists for these ids, remove it.
 */
int ipc_shm::shmdlt(int ids[], int idcount)
{
	vector<ushm>::iterator iter;
	bi::scoped_lock<boost::mutex> lock(shm_mutex);

	for (iter = ushm_list.begin(); iter != ushm_list.end(); ++iter) {
		if (*(iter->ids) == ids[0])
			break;
	}
	if (iter != ushm_list.end()) {
		delete []iter->ids;
		ushm_list.erase(iter);
	}

	int err = 0;
	for (int i = 0; i < idcount; i++) {
		if (::shmctl(ids[i], IPC_RMID, NULL) == -1)
			err = -1;
		else
			ids[i] = -1;
	}

	return err;
}

int ipc_shm::get_shm_id() const
{
	return shm_id;
}

void * ipc_shm::ll_shmat(int shm_id, void* shmaddr, int shmflg)
{
	int free_slot = -1;
	void *addr;
	bi::scoped_lock<boost::mutex> lock(shm_mutex);

	for (int i = 0; i < shm_db.size(); i++) {
		if (shm_id == shm_db[i].shmid) {
			shm_db[i].refcnt++;
			return shm_db[i].shmaddr;
		} else if (free_slot == -1 && shm_db[i].shmid == -1) {
			free_slot = i;
		}
	}
	addr = ::shmat(shm_id, shmaddr, shmflg);
	if (addr == reinterpret_cast<void *>(-1))
		return addr;

	if (free_slot != -1) {
		shm_db[free_slot].shmid  = shm_id;
		shm_db[free_slot].refcnt = 1;
		shm_db[free_slot].shmaddr= addr;
	} else {
		shminf_t node;
		node.shmid = shm_id;
		node.refcnt = 1;
		node.shmaddr = addr;
		shm_db.push_back(node);
	}

	return addr;
}

int ipc_shm::ll_shmdt(void *shmaddr)
{
	bi::scoped_lock<boost::mutex> lock(shm_mutex);

	for (int i = 0;i < shm_db.size(); i++) {
		if (shm_db[i].shmaddr == shmaddr) {
			if (--shm_db[i].refcnt == 0) {
				shm_db[i].shmid   = -1;
				shm_db[i].shmaddr = reinterpret_cast<void *>(-1);
				return ::shmdt(shmaddr);
			}
			break;
		}
	}

	return 0;
}

int ipc_shm::setup()
{
	shmid_ds buf;
	ushm_map *mp;
	bool indirect;
	int sz;
	size_t size;
	bi::scoped_lock<boost::mutex> lock(shm_mutex);

	if (::shmctl(shm_id, IPC_STAT, &buf) == -1) {
		GPT->_GPT_native_code = USHMCTL;
		return -1;
	}

	if (buf.shm_segsz <=  sizeof(ushm_map)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: shm_segsz <= sizeof(ushm_map)")).str(SGLOCALE));
		return -1;
	}

	sz = 0;
	indirect = false;

	mp = reinterpret_cast<ushm_map *>(ll_shmat(shm_id, 0, 0));
	if (mp->magic == USHM_MAGIC)
		indirect = true;

	if (indirect)
		sz = buf.shm_segsz - (sizeof(mp->magic) + sizeof(mp->size));

	size = mp->size;
	ushm ushm_mgr;
	ushm_mgr.key = 0;
	ushm_mgr.adr = NULL;
	ushm_mgr.smsize = size;
	ushm_mgr.cnt = sz / sizeof(int) + 1;
	if (indirect) {
		ushm_mgr.ids = new int[sz / sizeof(int) + 1];
		ushm_mgr.ids[0] = shm_id;
		::memcpy(ushm_mgr.ids + 1, &mp->ids, sz);
	}

	ushm_list.push_back(ushm_mgr);
	return 0;
}

}
}

