#if !defined(__DSTREAM_H__)
#define __DSTREAM_H__

#include "machine.h"
#include "string.h"
#include <string>

namespace ai
{
namespace sg
{

using namespace std;

class dstream
{
public:
	dstream();
	~dstream();

	dstream& operator<<(char c);
	dstream& operator<<(signed char c);
	dstream& operator<<(unsigned char c);
	dstream& operator<<(wchar_t c);
	dstream& operator<<(const wchar_t* c);
	dstream& operator<<(const char* c);
	dstream& operator<<(const signed char* c);
	dstream& operator<<(int c);
	dstream& operator<<(long c);
#if defined(_LONG_LONG)
	dstream& operator<<(long long c);
#endif
	dstream& operator<<(double c);
	dstream& operator<<(long double c);
	dstream& operator<<(float c);
	dstream& operator<<(unsigned int c);
	dstream& operator<<(unsigned long c);
#if defined(_LONG_LONG)
	dstream& operator<<(unsigned long long c);
#endif
	dstream& operator<<(const void* c);
	dstream& operator<<(streambuf* c);
	dstream& operator<<(ostream& (*f)(ostream&));
	dstream& operator<<(ios& (*f)(ios&));
	dstream& operator<<(short c);
	dstream& operator<<(unsigned short c);
	dstream& operator<<(const string& s);

#if defined(DEBUG)
	static bool disable_dstream;
#endif
};

#if !defined(__linux__)

template<class _Tm>
#if defined(__hpux)
dstream& operator<<(dstream& _O, const smanip<_Tm>& _M)
#else
dstream& operator<<(dstream& _O, const _Smanip<_Tm>& _M)
#endif
{
#if defined(DEBUG)
	if (!dstream::disable_dstream)
		cout << _M;
#endif

	return (_O);
}

#else

extern dstream& operator<<(dstream& _O, _Setfill<char> _M);
extern dstream& operator<<(dstream& _O, _Setiosflags _M);
extern dstream& operator<<(dstream& _O, _Resetiosflags _M);
extern dstream& operator<<(dstream& _O, _Setbase _M);
extern dstream& operator<<(dstream& _O, _Setprecision _M);
extern dstream& operator<<(dstream& _O, _Setw _M);

#endif

extern dstream dout;

}
}

#endif

