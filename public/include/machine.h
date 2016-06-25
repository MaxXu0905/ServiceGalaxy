#if !defined(__MACHINE_H__)
#define __MACHINE_H__

#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <exception>
#include "sg_locale.h"
#include "memleak.h"

#if !defined(__hpux) || defined(_HP_NAMESPACE_STD)

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#else

#include <iostream.h>
#include <fstream.h>
#include <strstream.h>
#include <iomanip.h>

namespace std
{

class ostringstream
{
public:
	ostringstream();
	~ostringstream();

	string str() const;
	void str(const string& __s);

	ostringstream& operator<<(char c);
	ostringstream& operator<<(signed char c);
	ostringstream& operator<<(unsigned char c);
	ostringstream& operator<<(wchar_t c);
	ostringstream& operator<<(const wchar_t* c);
	ostringstream& operator<<(const char* c);
	ostringstream& operator<<(const signed char* c);
	ostringstream& operator<<(int c);
	ostringstream& operator<<(long c);
#if defined(_LONG_LONG)
	ostringstream& operator<<(long long c);
#endif
	ostringstream& operator<<(double c);
	ostringstream& operator<<(float c);
	ostringstream& operator<<(unsigned int c);
	ostringstream& operator<<(unsigned long c);
#if defined(_LONG_LONG)
	ostringstream& operator<<(unsigned long long c);
#endif
	ostringstream& operator<<(const void* c);
	ostringstream& operator<<(streambuf* c);
	ostringstream& operator<<(ostream& (*f)(ostream&));
	ostringstream& operator<<(ios& (*f)(ios&));
	ostringstream& operator<<(short c);
	ostringstream& operator<<(unsigned short c);
	ostringstream& operator<<(const string& s);
	ostringstream& operator<<(const SMANIP(int)& _M);
	ostringstream& operator<<(const SMANIP(long)& _M);

private:
	ostrstream *ostr;
};

using ::string;
using ::vector;
using ::list;
using ::map;
using ::multimap;
using ::set;
using ::multiset;
using ::stack;
using ::queue;
using ::deque;
using ::bitset;
using ::iterator;

using ::endl;
using ::ws;
using ::ios;
using ::flush;

using ::resetiosflags;
using ::setiosflags;
using ::setbase;
using ::setfill;
using ::setprecision;
using ::setw;

using ::getline;
using ::min;
using ::max;
using ::sort;

using ::exception;
using ::logic_error;
using ::domain_error;
using ::invalid_argument;
using ::length_error;
using ::out_of_range;
using ::runtime_error;
using ::range_error;
using ::overflow_error;
using ::underflow_error;

using ::fstream;
using ::ifstream;
using ::ofstream;
using ::ios;
using ::istream;
using ::ostream;
using ::iostream;

}

#endif

#endif

