#include "semmutex.h"

namespace ai
{
namespace sg
{

semmutex::semmutex()
{
  union semun
  {
    int val;
    struct semid_ds *buf;
    ushort *array;
  } arg;

  if ((_nsemid = ::semget(IPC_PRIVATE, 1, 0660 | IPC_CREAT)) < 0 )
  {
    throw bad_msg(__FILE__,__LINE__,276,(_("ERROR: semget failed.")).str(SGLOCALE));
  }

  arg.val = 0;
  if (::semctl(_nsemid, 0, SETVAL, arg) < 0)
  {
    throw bad_msg(__FILE__,__LINE__,277,(_("ERROR: semctl failed.")).str(SGLOCALE));
  }
}

semmutex::~semmutex()
{
  if ( ::semctl(_nsemid, 0, IPC_RMID) < 0 )
    throw bad_msg(__FILE__,__LINE__,277,(_("ERROR: semctl failed.")).str(SGLOCALE));
}

void semmutex::lock(void)
{
  struct sembuf ops[] = {{0, -1, 0}};

  if ( ::semop(_nsemid, ops, 1) < 0 )
  {
    throw bad_msg(__FILE__,__LINE__,278,(_("ERROR: semop failed.")).str(SGLOCALE));
  }
}

bool semmutex::try_lock(void)
{
  struct sembuf ops[] = {{0, -1, IPC_NOWAIT}};

  return (::semop(_nsemid, ops, 1) == EAGAIN) ? TRUE : FALSE;
}

void semmutex::unlock(void)
{
  struct sembuf ops[] = {{0, 1, 0}};

  if ( ::semop(_nsemid, ops, 1) < 0 )
  {
    throw bad_msg(__FILE__,__LINE__,278,(_("ERROR: semop failed.")).str(SGLOCALE));
  }
}

int semmutex::get_value(void)
{
  return ::semctl(_nsemid,0,GETVAL);
}

}
}

