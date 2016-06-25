#include "dstream.h"

namespace ai
{
namespace sg
{

#if defined(DEBUG)
bool dstream::disable_dstream = false;
#endif

dstream::dstream()
{
#if defined(DEBUG)
	char *ptr;
	if ((ptr = ::getenv("DISABLE_DSTREAM")) != NULL) {
		if (strcasecmp(ptr, "Y") == 0)
			disable_dstream = true;
	}
#endif
}

dstream::~dstream()
{
}

dstream& dstream::operator<<(char c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(signed char c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(unsigned char c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(wchar_t c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(const wchar_t* c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(const char* c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(const signed char* c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(int c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(long c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

#if defined(_LONG_LONG)
dstream& dstream::operator<<(long long c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}
#endif

dstream& dstream::operator<<(double c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(long double c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}
dstream& dstream::operator<<(float c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(unsigned int c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(unsigned long c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

#if defined(_LONG_LONG)
dstream& dstream::operator<<(unsigned long long c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}
#endif

dstream& dstream::operator<<(const void* c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(streambuf* c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(ostream& (*f)(ostream&))
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << f;
#endif

	return *this;
}

dstream& dstream::operator<<(ios& (*f)(ios&))
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << f;
#endif

	return *this;
}

dstream& dstream::operator<<(short c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(unsigned short c)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << c;
#endif

	return *this;
}

dstream& dstream::operator<<(const string& s)
{
#if defined(DEBUG)
	if (!disable_dstream)
		cout << s;
#endif

	return *this;
}

#if defined(__linux__)

dstream& operator<<(dstream& _O, _Setfill<char> _M)
{
#if defined(DEBUG)
	if (!dstream::disable_dstream)
		cout << _M;
#endif

	return (_O);
}

dstream& operator<<(dstream& _O, _Setiosflags _M)
{
#if defined(DEBUG)
	if (!dstream::disable_dstream)
		cout << _M;
#endif

	return (_O);
}

dstream& operator<<(dstream& _O, _Resetiosflags _M)
{
#if defined(DEBUG)
	if (!dstream::disable_dstream)
		cout << _M;
#endif

	return (_O);
}

dstream& operator<<(dstream& _O, _Setbase _M)
{
#if defined(DEBUG)
	if (!dstream::disable_dstream)
		cout << _M;
#endif

	return (_O);
}

dstream& operator<<(dstream& _O, _Setprecision _M)
{
#if defined(DEBUG)
	if (!dstream::disable_dstream)
		cout << _M;
#endif

	return (_O);
}

dstream& operator<<(dstream& _O, _Setw _M)
{
#if defined(DEBUG)
	if (!dstream::disable_dstream)
		cout << _M;
#endif

	return (_O);
}

#endif

dstream dout;

}
}

