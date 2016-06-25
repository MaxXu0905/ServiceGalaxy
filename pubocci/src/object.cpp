/**
 * @file object.cpp
 * @brief The implement of the object class.
 *
 * @author Dejin Bao
 * @author Former author list: Genquan Xu
 * @version 3.5.0
 * @date 2005-12-07
 */

#include "object.h"

namespace ai
{
namespace sg
{

static runtimeclass* m_classList = 0;
static pthread_rwlock_t g_rwlock = PTHREAD_RWLOCK_INITIALIZER;

object::object()
{
}

object::~object()
{
}

int object::run()
{
    return 0;
}

int object::run_ext()
{
    return 0;
}

int object::run_service()
{
    return 0;
}

int object::start()
{
    return 0;
}

int object::stop()
{
    return 0;
}

int object::status()
{
    return 0;
}

int object::kill()
{
    return 0;
}

int object::reprocess()
{
  return 0;
}

int object::dump_fee(const string& taskid)
{
  return 0;
}

int object::refresh_data(const vector<int>& tableids,int nAction)
{
    return 0;
}

int object::rollback(const string& taskid)
{
  return 0;
}

object* runtimeclass::create_object(const param* para)
{
    if (m_pfnCreateObject == 0)
    {
        dout << (_("Error: Trying to create object which is not DECLARE_DYNCREATE ")).str(SGLOCALE) << m_lpszClassName << endl;
        return 0;
    }

    object* pObject = 0;
    try
    {
        pObject = (*m_pfnCreateObject)(para);
    }
    catch(...)
    {
        throw;
    }

    return pObject;
}

void class_init(runtimeclass* pNewClass)
{
    pthread_rwlock_wrlock(&g_rwlock);
    pNewClass->m_pNextClass = m_classList;
    m_classList = pNewClass;
    pthread_rwlock_unlock(&g_rwlock);
}

object* class_get(const char* classname,const param* para)
{
    pthread_rwlock_rdlock(&g_rwlock);
    runtimeclass* p = m_classList;

    while ( p != 0 )
    {
        if ( strcmp(p->m_lpszClassName,classname) == 0 )
        {
            pthread_rwlock_unlock(&g_rwlock);
            return p->create_object(para);
        }
        p = p->m_pNextClass;
    }
    pthread_rwlock_unlock(&g_rwlock);
    return 0;
}

void class_delete(const char* classname)
{
    pthread_rwlock_rdlock(&g_rwlock);
    runtimeclass* p = m_classList;
    runtimeclass* prev = 0;

    while ( p != 0 )
    {
        if ( strcmp(p->m_lpszClassName,classname) == 0 )
        {
            //Í·½ÚµãÂú×ã
            if ( prev == 0 )
                m_classList = p->m_pNextClass;
            else
                prev->m_pNextClass = p->m_pNextClass;
            break;
        }
        prev = p;
        p = p->m_pNextClass;
    }
    pthread_rwlock_unlock(&g_rwlock);
}

}
}

