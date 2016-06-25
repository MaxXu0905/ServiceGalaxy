#if !defined(__SQL_MACRO_H__)
#define __SQL_MACRO_H__

namespace ai
{
namespace scci
{

// maximum message length 
#if defined(SQL_MAX_MESSAGE_LENGTH)
#undef SQL_MAX_MESSAGE_LENGTH
#endif

int const SQL_MAX_MESSAGE_LENGTH = 512;

typedef int RETCODE;				// SUCCEED or FAIL
typedef void* HENV;
typedef void* HDBC;
typedef void* HSTMT;

}
}

#endif

