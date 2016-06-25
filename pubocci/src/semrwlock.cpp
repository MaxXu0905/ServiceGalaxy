#include "semrwlock.h"

namespace ai
{
namespace sg
{

semrwlock::semrwlock(int nIpcKey)
  : _nIpcKey(nIpcKey)
{
  _bServer = false;
  for(int i = 0 ; i < LOCK_NUM ; i ++ )
    _nSemId[i] = -1;
}

semrwlock::~semrwlock()
{
  for( int i = 0 ; i < LOCK_NUM ; i ++ )
  {
    if ( _nSemId[i] >= 0 && _bServer )
    {
      if ( ::semctl(_nSemId[i],0,IPC_RMID) < 0 )
        throw bad_msg(__FILE__,__LINE__,277,(_("ERROR: semctl failed.")).str(SGLOCALE));
    }
  }
}

int semrwlock::create_rwlock()
{
  _bServer = true;
  int ntry = 0;

  for ( int i = 0 ; i < LOCK_NUM ; i ++ )
  {
    _nSemId[i] = ::semget(_nIpcKey + i, 0, 0600);

    if ( _nSemId[i] >= 0 )
    {
      if ( ::semctl(_nSemId[i],0,IPC_RMID) < 0 )
      {
        throw bad_msg(__FILE__,__LINE__,277,(_("ERROR: remove semphore failed.")).str(SGLOCALE));
      }
      _nSemId[i] = -1;
    }

    if ( _nSemId[i] < 0 )
    {
      while ( (_nSemId[i] = ::semget(_nIpcKey + i, 1, IPC_CREAT|0660)) < 0 )
      {
        usleep(10);
        ntry ++;
        if ( ntry > 1000 )
          throw bad_msg(__FILE__,__LINE__,276,(_("ERROR: semget failed, key = {1}") % (_nIpcKey + i)).str(SGLOCALE));
      }

      union semun {
        int val;
        struct semid_ds *buf;
        ushort *array;
      }arg;

      if ( i == WRITER_COUNT )
          arg.val = 0;//writer count
      else if ( i == READER_COUNT )
        arg.val = 0;//writer count
      else  if ( i == MUTEX_LOCK )
        arg.val = 1;//writer count
      while ( semctl(_nSemId[i],0,SETVAL,arg) < 0 )
      {
        usleep(10);
        ntry ++;
        if ( ntry > 1000 )
          throw bad_msg(__FILE__,__LINE__,277,(_("ERROR: semctl failed, key = {1}") % (_nIpcKey + i)).str(SGLOCALE));
      }
    }
  }

  return 0;
}

int semrwlock::get_rwlock()
{
  _bServer = FALSE;
  int ntry = 0;

  for( int i = 0 ; i < LOCK_NUM ; i ++ )
  {
    while ( (_nSemId[i] = semget(_nIpcKey + i, 0, 0600) ) < 0 )
    {
      dout << i << "get_rwlock failed.\n";
      usleep(10);
      ntry ++;
      if ( ntry > 1000 )
	  	throw bad_msg(__FILE__,__LINE__,277,(_("ERROR: semget failed, key = {1}") % (_nIpcKey + i)).str(SGLOCALE));
    }
  }

  return 0;
}

int semrwlock::lock(int nSemNo)
{
  int ntry = 0;

  struct sembuf ops[] = {{0,-1, SEM_UNDO}};

  if ( _nSemId[nSemNo] < 0 )
    throw bad_msg(__FILE__,__LINE__,280,(_("ERROR: invalid semid.")).str(SGLOCALE));

  if ( nSemNo < 0 || nSemNo > 2 )
    throw bad_msg(__FILE__,__LINE__,279,(_("ERROR: invalid semno.")).str(SGLOCALE));

  int value = -1;
  while( (value = semop(_nSemId[nSemNo],ops,1)) < 0 )
  {
    dout << (_("{1},{2} lock failed.\n") % nSemNo % _nSemId[nSemNo] ).str(SGLOCALE);
    usleep(10);
    ntry ++;
    if ( ntry > 1000 )
    {
      throw bad_msg(__FILE__,__LINE__,278,(_("ERROR: semop failed, nSemNo = {3}") % nSemNo ).str(SGLOCALE));
    }
  }
  return value;
}

int semrwlock::unlock(int nSemNo)
{
  int ntry = 0;

  struct sembuf ops[] = {{0, 1, SEM_UNDO}};

  if ( _nSemId[nSemNo] < 0 )
    throw bad_msg(__FILE__,__LINE__,280,(_("ERROR: invalid semid.")).str(SGLOCALE));

  if ( nSemNo < 0 || nSemNo > 2 )
    throw bad_msg(__FILE__,__LINE__,279,(_("ERROR: invalid sem no.")).str(SGLOCALE));

  int value = -1;
  while( (value = semop(_nSemId[nSemNo],ops,1)) < 0 )
  {
    dout << (_("{1},{2} lock failed.\n") % nSemNo % _nSemId[nSemNo] ).str(SGLOCALE);
    usleep(10);
    ntry ++;
    if ( ntry > 1000 )
		throw bad_msg(__FILE__,__LINE__,277,(_("ERROR: semop failed, semno = {1}") % nSemNo).str(SGLOCALE));
  }

  return 0;
}

int semrwlock::get_value(int nSemNo)
{
  int nReaders = 0;
  int ntry = 0;

  if ( _nSemId[nSemNo] < 0 )
    return -1;

  if ( nSemNo < 0 || nSemNo > 2 )
    throw bad_msg(__FILE__,__LINE__,279,(_("ERROR: invalid sem no.")).str(SGLOCALE));

  while ( (nReaders = semctl(_nSemId[nSemNo],0,GETVAL)) < 0 )
  {
    dout << (_("{1},{2} get_value failed.") % nSemNo % _nSemId[nSemNo]).str(SGLOCALE) << "\n\n";
    usleep(10);
    ntry ++;
    if ( ntry > 1000 )
      throw bad_msg(__FILE__,__LINE__,277,(_("ERROR: semctl failed, semno = {1}") % nSemNo).str(SGLOCALE));
  }
  return nReaders;
}

int semrwlock::read_lock()
{
  int nwriter;
#if defined(DEBUG)
  int nreader;
#endif
  for( ; ; )
  {
    if ( lock(MUTEX_LOCK) < 0 )
      throw bad_msg(__FILE__,__LINE__,281,(_("ERROR: invalid semid.")).str(SGLOCALE));
    nwriter = get_value(WRITER_COUNT);
#if defined(DEBUG)
    nreader = get_value(READER_COUNT);
#endif
    if ( nwriter == 0 )
      break;
    if ( unlock(MUTEX_LOCK) < 0 )
      return -1;
  }
  if ( unlock(READER_COUNT) < 0 )
    return -1;
  if ( unlock(MUTEX_LOCK) < 0 )
    return -1;

#if defined(DEBUG)
  dout << (_("read_lock: writers = {1}: readers = {2}") % nwriter % nreader ).str(SGLOCALE) << endl;
  print_time("read_lock");
#endif
  return 0;
}

int semrwlock::read_unlock()
{
  if ( lock(READER_COUNT) < 0 )
    return -1;

#if defined(DEBUG)
  print_time("read_unlock");
#endif
  return 0;
}

int semrwlock::write_lock()
{
  int nreader;
#if defined(DEBUG)
  int nwriter;
#endif
  if ( unlock(WRITER_COUNT) < 0 )
    return -1;

  for( ; ; )
  {
    if ( lock(MUTEX_LOCK) < 0 )
      return -1;
    nreader = get_value(READER_COUNT);
#if defined(DEBUG)
    nwriter = get_value(WRITER_COUNT);
#endif
    if ( nreader == 0 )
      break;
    if ( unlock(MUTEX_LOCK) < 0 )
      return -1;
  }

#if defined(DEBUG)
  dout << (_("write_lock: reades = {1}: writer = {2}") % nreader % nwriter ).str(SGLOCALE) << endl;
  print_time("write_lock");
#endif
  return 0;
}

int semrwlock::write_unlock()
{
  if ( lock(WRITER_COUNT) < 0 )
    return -1;
  if ( unlock(MUTEX_LOCK) < 0 )
    return -1;

#if defined(DEBUG)
  print_time("write_unlock");
#endif
  return 0;
}

void semrwlock::print_time(const char* func_name)
{
  time_t now;
  struct tm now_tm;
  time(&now);
  now_tm = *localtime(&now);
  printf("[%s] %04d-%02d-%02d %02d:%02d:%02d\n",func_name,now_tm.tm_year + 1900,now_tm.tm_mon + 1,now_tm.tm_mday,now_tm.tm_hour,now_tm.tm_min,now_tm.tm_sec);
}

}
}

