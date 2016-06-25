#include "machine.h"

#if defined(__hpux) && !defined(_HP_NAMESPACE_STD)

namespace std
{

ostringstream::ostringstream()
{
	ostr = new ostrstream();
}

ostringstream::~ostringstream()
{
	delete ostr;
}
	
string ostringstream::str() const
{
	string __str;
	char *cstr = ostr->str();
	if (cstr != NULL) {
		__str = cstr;
		::free(cstr);
	}
	return __str;
}

void ostringstream::str(const string& __s)
{
	delete ostr;
	ostr = new ostrstream();
	*ostr << __s;
}
	
ostringstream& ostringstream::operator<<(char c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(signed char c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(unsigned char c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(wchar_t c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(const wchar_t* c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(const char* c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(const signed char* c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(int c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(long c)
{
	*ostr << c;
	return *this;
}

#if defined(_LONG_LONG)
ostringstream& ostringstream::operator<<(long long c)
{
	*ostr << c;
	return *this;
}
#endif

ostringstream& ostringstream::operator<<(double c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(float c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(unsigned int c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(unsigned long c)
{
	*ostr << c;
	return *this;
}

#if defined(_LONG_LONG)
ostringstream& ostringstream::operator<<(unsigned long long c)
{
	*ostr << c;
	return *this;
}
#endif

ostringstream& ostringstream::operator<<(const void* c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(streambuf* c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(ostream& (*f)(ostream&))
{
	f(*ostr);
	return *this;
}

ostringstream& ostringstream::operator<<(ios& (*f)(ios&))
{
	f(*ostr);
	return *this;
}

ostringstream& ostringstream::operator<<(short c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(unsigned short c)
{
	*ostr << c;
	return *this;
}

ostringstream& ostringstream::operator<<(const string& s)
{
	*ostr << s;
	return *this;
}

ostringstream& ostringstream::operator<<(const SMANIP(int)& _M)
{
	*ostr << _M;
	return *this;
}

ostringstream& ostringstream::operator<<(const SMANIP(long)& _M)
{
	*ostr << _M;
	return *this;
}

}

#endif

