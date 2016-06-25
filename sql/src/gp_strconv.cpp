#include <cstring>
#include <limits>
#include "gp_strconv.h"

using namespace std;
using namespace ai::scci::gp::internal;
namespace {
using namespace ai::sg;
template<typename T> void set_to_NaN(T &);

template<> inline void set_to_NaN(float &t) {
	t = std::numeric_limits<float>::infinity();
}
template<> inline void set_to_NaN(double &t) {
	t = std::numeric_limits<double>::infinity();
}

template<typename T> inline void set_to_Inf(T &t, int sign = 1) {
	T value = INFINITY;
	if (sign < 0)
		value = -value;
	t = value;
}

template<typename L, typename R>
inline L absorb_digit(L value, R digit) throw () {
	return L(L(10) * value + L(digit));
}

template<typename T> void from_string_signed(const char Str[], T &Obj) {
	int i = 0;
	T result = 0;

	if (!isdigit(Str[i])) {
		if (Str[i] != '-')
			throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Could not convert string to integer: {1}") % Str).str(SGLOCALE));
		for (++i; isdigit(Str[i]); ++i) {
			const T newresult = absorb_digit(result, -digit_to_number(Str[i]));
			if (newresult > result)
				throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Integer too small to read: {1}") % Str).str(SGLOCALE));
			result = newresult;
		}
	} else
		for (; isdigit(Str[i]); ++i) {
			const T newresult = absorb_digit(result, digit_to_number(Str[i]));
			if (newresult < result)
				throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Integer too large to read: {1}") % Str).str(SGLOCALE));
			result = newresult;
		}

	if (Str[i])
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Unexpected text after integer: {1}") % Str).str(SGLOCALE));

	Obj = result;
}

template<typename T> void from_string_unsigned(const char Str[], T &Obj) {
	int i = 0;
	T result = 0;

	if (!isdigit(Str[i]))
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Could not convert string to unsigned integer: {1}") % Str).str(SGLOCALE));

	for (; isdigit(Str[i]); ++i) {
		const T newres = absorb_digit(result, digit_to_number(Str[i]));
		if (newres < result)
			throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Unsigned integer too large to read: {1}") % Str).str(SGLOCALE));
		result = newres;
	}

	if (Str[i])
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Unexpected text after integer: '{1}'") % Str).str(SGLOCALE));
	Obj = result;
}

bool valid_infinity_string(const char str[]) {
	// TODO: Also accept less sensible case variations.
	return strcmp("infinity", str) == 0 || strcmp("Infinity", str) == 0
			|| strcmp("INFINITY", str) == 0 || strcmp("inf", str) == 0;
}

/* These are hard.  Sacrifice performance of specialized, nonflexible,
 * non-localized code and lean on standard library.  Some special-case code
 * handles NaNs.
 */
template<typename T> inline void from_string_float(const char Str[], T &Obj) {
	bool ok = false;
	T result;

	switch (Str[0]) {
	case 'N':
	case 'n':
		// Accept "NaN," "nan," etc.
		ok = ((Str[1] == 'A' || Str[1] == 'a')
				&& (Str[2] == 'N' || Str[2] == 'n') && !Str[3]);
		set_to_NaN(result);
		break;

	case 'I':
	case 'i':
		ok = valid_infinity_string(Str);
		set_to_Inf(result);
		break;

	default:
		if (Str[0] == '-' && valid_infinity_string(&Str[1])) {
			ok = true;
			set_to_Inf(result, -1);
		} else {
			stringstream S(Str);
			ok = (S >> result);
		}
		break;
	}

	if (!ok)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Could not convert string to numeric value: '{1}") % Str).str(SGLOCALE));

	Obj = result;
}

template<typename T> inline string to_string_unsigned(T Obj) {
	if (!Obj)
		return "0";

	char buf[4 * sizeof(T) + 1];

	char *p = &buf[sizeof(buf)];
	*--p = '\0';
	while (Obj > 0) {
		*--p = number_to_digit(int(Obj % 10));
		Obj /= 10;
	}
	return p;
}

template<typename T> inline string to_string_fallback(T Obj) {
	stringstream S;
	S.precision(16);
	S << Obj;
	return S.str();
}

template<typename T> inline bool is_NaN(T Obj) {
	return
	!(Obj <= Obj + 1000);
}

template<typename T> inline bool is_Inf(T Obj) {
	return
	Obj >= Obj + 1 && Obj <= 2 * Obj && Obj >= 2 * Obj;
}

template<typename T> inline string to_string_float(T Obj) {
	return to_string_fallback(Obj);
}

template<typename T> inline string to_string_signed(T Obj) {
	if (Obj < 0) {
		T Neg(-Obj);
		const bool negatable = Neg > 0;
		if (negatable)
			return '-' + to_string_unsigned(-Obj);
		else
			return to_string_fallback(Obj);
	}

	return to_string_unsigned(Obj);
}

} // namespace

namespace ai {
namespace scci {
namespace gp {
namespace internal
{
void throw_null_conversion(const string &type)
{
	throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Attempt to convert null to {1}") % type).str(SGLOCALE));
}
} // namespace pqxx::internal
void string_traits<bool>::from_string(const char Str[], bool &Obj) {
	bool OK, result = false;

	switch (Str[0]) {
	case 0:
		result = false;
		OK = true;
		break;

	case 'f':
	case 'F':
		result = false;
		OK = !(Str[1] && (strcmp(Str + 1, "alse") != 0)
				&& (strcmp(Str + 1, "ALSE") != 0));
		break;

	case '0': {
		int I;
		string_traits<int>::from_string(Str, I);
		result = (I != 0);
		OK = ((I == 0) || (I == 1));
	}
		break;

	case '1':
		result = true;
		OK = !Str[1];
		break;

	case 't':
	case 'T':
		result = true;
		OK = !(Str[1] && (strcmp(Str + 1, "rue") != 0)
				&& (strcmp(Str + 1, "RUE") != 0));
		break;

	default:
		OK = false;
		break;
	}

	if (!OK)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Failed conversion to bool: '{1}'") % Str).str(SGLOCALE));

	Obj = result;
}

string string_traits<bool>::to_string(bool Obj) {
	return Obj ? "true" : "false";
}

void string_traits<short>::from_string(const char Str[], short &Obj) {
	from_string_signed(Str, Obj);
}

string string_traits<short>::to_string(short Obj) {
	return to_string_signed(Obj);
}



void string_traits<unsigned short>::from_string(const char Str[],
		unsigned short &Obj) {
	from_string_unsigned(Str, Obj);
}

string string_traits<unsigned short>::to_string(unsigned short Obj) {
	return to_string_unsigned(Obj);
}

void string_traits<int>::from_string(const char Str[], int &Obj) {
	from_string_signed(Str, Obj);
}

string string_traits<int>::to_string(int Obj) {
	return to_string_signed(Obj);
}

void string_traits<unsigned int>::from_string(const char Str[],
		unsigned int &Obj) {
	from_string_unsigned(Str, Obj);
}

string string_traits<unsigned int>::to_string(unsigned int Obj) {
	return to_string_unsigned(Obj);
}

void string_traits<long>::from_string(const char Str[], long &Obj) {
	from_string_signed(Str, Obj);
}

string string_traits<long>::to_string(long Obj) {
	return to_string_signed(Obj);
}

void string_traits<unsigned long>::from_string(const char Str[],
		unsigned long &Obj) {
	from_string_unsigned(Str, Obj);
}

string string_traits<unsigned long>::to_string(unsigned long Obj) {
	return to_string_unsigned(Obj);
}


void string_traits<float>::from_string(const char Str[], float &Obj) {
	from_string_float(Str, Obj);
}

string string_traits<float>::to_string(float Obj) {
	return to_string_float(Obj);
}

void string_traits<double>::from_string(const char Str[], double &Obj) {
	from_string_float(Str, Obj);
}

string string_traits<double>::to_string(double Obj) {
	return to_string_float(Obj);
}
}
}
}

