#include <string>
#include <map>
#include <stdlib.h>
#include "sys.h"
#include <boost/format.hpp>
namespace ai {
namespace test {
using namespace std;
int Sys::noutsys(const char * cmd, const char *logname ) {
		string s = (boost::format("%1% >>%2% 2>&1 ") % cmd % logname).str();
		int re = system(s.c_str());
		return WEXITSTATUS(re);
	}

}
}
