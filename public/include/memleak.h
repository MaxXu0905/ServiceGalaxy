#if !defined(__MEMLEAK_H__)
#define __MEMLEAK_H__

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <memory>
#include <string>

#if defined(MEM_DEBUG)

#undef new
#undef delete

void * operator new(size_t size, const char *filename, int lineno);
void * operator new[](size_t size, const char *filename, int lineno);

#if defined(HPUX)
void operator delete(void *ptr) __THROWSPEC_NULL;
void operator delete[](void *ptr) __THROWSPEC_NULL;
#else
void operator delete(void *ptr) throw();
void operator delete[](void *ptr) throw();
#endif

void handle_delete(const char *filename, int lineno);

#define new new(__FILE__, __LINE__)
#define delete handle_delete(__FILE__, __LINE__),delete

#endif

#endif

