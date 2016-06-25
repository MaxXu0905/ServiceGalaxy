#if !defined(__SQL_EXTERN_H__)
#define __SQL_EXTERN_H__

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "sql_base.h"

extern FILE *yyin;
extern FILE *yyout;
extern int yyparse(void);

namespace ai
{
namespace scci
{

using namespace std;

class sql_statement;

extern int errcnt;
extern string print_prefix;

}
}

#endif

