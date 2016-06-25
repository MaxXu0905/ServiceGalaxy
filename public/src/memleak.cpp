#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <iomanip>
#include <iostream>
#include <fstream>
#include "memleak.h"

#if defined(MEM_DEBUG)

#undef new
#undef delete

using namespace std;

enum mem_type
{
	MEM_TYPE_NEW = 0,
	MEM_TYPE_NEW_ARRAY = 1,
	MEM_TYPE_DELETE = 2,
	MEM_TYPE_DELETE_ARRAY = 3
};

struct memleak_node_t
{
	void *address;
	mem_type type;
	size_t size;
	char filename[256];
	int lineno;

	bool operator==(const memleak_node_t& rhs) const;
	memleak_node_t& operator=(const memleak_node_t& rhs);
};

bool memleak_node_t::operator==(const memleak_node_t& rhs) const
{
	return (address == rhs.address && type == rhs.type);
}

memleak_node_t& memleak_node_t::operator=(const memleak_node_t& rhs)
{
	address = rhs.address;
	type = rhs.type;
	size = rhs.size;
	strcpy(filename, rhs.filename);
	lineno = rhs.lineno;
}

class memleak_thread_mutex
{
public:
	memleak_thread_mutex();
	~memleak_thread_mutex();

	void lock();
	int try_lock();
	void unlock();

private:
	pthread_mutex_t mutex;
	pthread_t owner; // thread id of the owner
	int lock_count; // # of locks acquired
};

memleak_thread_mutex::memleak_thread_mutex()
{
	::pthread_mutex_init(&mutex, NULL);
	lock_count = 0; // initialize lock cnt to 0
	owner = 0; // owner's thread id
}

memleak_thread_mutex::~memleak_thread_mutex()
{
	::pthread_mutex_destroy(&mutex);
}

void memleak_thread_mutex::lock()
{
	pthread_t self = ::pthread_self();

	if (::pthread_equal(owner, self) && lock_count) {
		lock_count++;
		return;
	}

	::pthread_mutex_lock(&mutex);
	owner = self;
	lock_count = 1;
}

int memleak_thread_mutex::try_lock()
{
	pthread_t self = ::pthread_self();

	if (lock_count) {
		if (!::pthread_equal(owner, self)) {
			return -1;
		} else {
			lock_count++;
			return 0;
		}
	}

	::pthread_mutex_trylock(&mutex);
	owner = self;
	lock_count = 1;
	return 0;
}

void memleak_thread_mutex::unlock ()
{
	if (!lock_count)
		return;

	pthread_t self = ::pthread_self();
	if (!::pthread_equal(owner, self) && lock_count)
		return;

	--lock_count;
	if (!lock_count) {
		owner = 0;
		::pthread_mutex_unlock(&mutex);
	}
}

static pthread_once_t memleak_once_control = PTHREAD_ONCE_INIT;
static memleak_thread_mutex memleak_mutex;
static int memleak_count = 0;
static int memleak_size = 0;
static memleak_node_t *memleak_array = NULL;
static char g_filename[256];
static int g_lineno = 0;
static bool enable_memdebug = false;

static void memleak_signal(int signo);
static void memleak_dump();

static void memleak_init()
{
	char *ptr = ::getenv("MEM_DEBUG");
	if (ptr) {
		enable_memdebug = true;

		signal(SIGUSR2, memleak_signal);
		atexit(memleak_dump);
	}
}

static void insert(const memleak_node_t& item)
{
	if (memleak_count == memleak_size) {
		if (memleak_array == NULL) {
			memleak_size = 32;
			memleak_array = reinterpret_cast<memleak_node_t *>(::malloc(memleak_size * sizeof(memleak_node_t)));
		} else {
			memleak_size *= 2;
			memleak_array = reinterpret_cast<memleak_node_t *>(::realloc(memleak_array, memleak_size * sizeof(memleak_node_t)));
		}
	}

	memleak_array[memleak_count++] = item;
}

static void remove(memleak_node_t *item)
{
	if (--memleak_count > 0)
		*item = memleak_array[memleak_count];
}

void * operator new(size_t size, const char *filename, int lineno)
{
	memleak_node_t item;

	pthread_once(&memleak_once_control, memleak_init);

	if (!enable_memdebug)
		return ::malloc(size);

	item.address = ::malloc(size);
	item.type = MEM_TYPE_NEW;
	item.size = size;
	strcpy(item.filename, filename);
	item.lineno = lineno;

	memleak_mutex.lock();
	insert(item);
	memleak_mutex.unlock();

	return item.address;
}

void * operator new[](size_t size, const char *filename, int lineno)
{
	memleak_node_t item;

	pthread_once(&memleak_once_control, memleak_init);

	if (!enable_memdebug)
		return ::malloc(size);

	item.address = ::malloc(size);
	item.type = MEM_TYPE_NEW_ARRAY;
	item.size = size;
	strcpy(item.filename, filename);
	item.lineno = lineno;

	memleak_mutex.lock();
	insert(item);
	memleak_mutex.unlock();

	return item.address;
}

#if defined(HPUX)
void operator delete(void *ptr) __THROWSPEC_NULL
#else
void operator delete(void *ptr) throw ()
#endif
{
	memleak_node_t item;

	pthread_once(&memleak_once_control, memleak_init);

	if (!enable_memdebug || g_lineno <= 0) {
		::free(ptr);
		return;
	}

	item.address = ptr;
	item.type = MEM_TYPE_NEW;

	memleak_mutex.lock();
	memleak_node_t *node = std::find(memleak_array, memleak_array + memleak_count, item);
	if (node == memleak_array + memleak_count) {
		item.type = MEM_TYPE_DELETE;
		strcpy(item.filename, g_filename);
		item.lineno = g_lineno;
		insert(item);
	} else {
		remove(node);
	}
	g_lineno = 0;
	memleak_mutex.unlock();

	::free(ptr);
}

#if defined(HPUX)
void operator delete[](void *ptr) __THROWSPEC_NULL
#else
void operator delete[](void *ptr) throw ()
#endif
{
	memleak_node_t item;

	pthread_once(&memleak_once_control, memleak_init);

	if (!enable_memdebug || g_lineno <= 0) {
		::free(ptr);
		return;
	}

	item.address = ptr;
	item.type = MEM_TYPE_NEW_ARRAY;

	memleak_mutex.lock();
	memleak_node_t *node = std::find(memleak_array, memleak_array + memleak_count, item);
	if (node == memleak_array + memleak_count) {
		item.type = MEM_TYPE_DELETE_ARRAY;
		strcpy(item.filename, g_filename);
		item.lineno = g_lineno;
		insert(item);
	} else {
		remove(node);
	}
	g_lineno = 0;
	memleak_mutex.unlock();

	::free(ptr);
}

void handle_delete(const char *filename, int lineno)
{
	strcpy(g_filename, filename);
	g_lineno = lineno;
}

static void memleak_signal(int signo)
{
	memleak_dump();
}

static void memleak_dump()
{
	if (memleak_mutex.try_lock() < 0)
		return;

	char filename[32];
	::sprintf(filename, "memleak-%ld.txt", ::getpid());
	ofstream ofs(filename);
	if (!ofs) {
		cerr <<(_("ERROR: Can't open file")).str(SGLOCALE);
		cerr << "memleak.txt\n";
		memleak_mutex.unlock();
		return;
	}

	for (int i = 0; i < memleak_count; ++i) {
		switch (memleak_array[i].type) {
		case MEM_TYPE_NEW:
			ofs << std::setbase(16) << reinterpret_cast<long>(memleak_array[i].address) << std::setbase(10)
				<< "\tMemory allocated by new but not freed\n"
				<< "\tsize = " << memleak_array[i].size << "\n"
				<< "\tfilename = " << memleak_array[i].filename << "\n"
				<< "\tlineno = " << memleak_array[i].lineno << "\n\n";
			break;
		case MEM_TYPE_NEW_ARRAY:
			ofs << std::setbase(16) << reinterpret_cast<long>(memleak_array[i].address) << std::setbase(10)
				<< "\tMemory allocated by new[] but not freed\n"
				<< "\tsize = " << memleak_array[i].size << "\n"
				<< "\tfilename = " << memleak_array[i].filename << "\n"
				<< "\tlineno = " << memleak_array[i].lineno << "\n\n";
			break;
		case MEM_TYPE_DELETE:
			ofs << std::setbase(16) << reinterpret_cast<long>(memleak_array[i].address) << std::setbase(10)
				<< "\tMemory freed by delete but not allocated\n"
				<< "\tfilename = " << memleak_array[i].filename << "\n"
				<< "\tlineno = " << memleak_array[i].lineno << "\n\n";
			break;
		case MEM_TYPE_DELETE_ARRAY:
			ofs << std::setbase(16) << reinterpret_cast<long>(memleak_array[i].address) << std::setbase(10)
				<< "\tMemory freed by delete[] but not allocated\n"
				<< "\tfilename = " << memleak_array[i].filename << "\n"
				<< "\tlineno = " << memleak_array[i].lineno << "\n\n";
			break;
		}
	}

	memleak_mutex.unlock();
}

#endif

