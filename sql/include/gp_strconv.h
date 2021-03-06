#ifndef GP_H_STRINGCONV
#define GP_H_STRINGCONV
#include <string>
#include <sstream>
#include <stdexcept>
#include <math.h>
#include "user_exception.h"
namespace ai {
namespace scci {
namespace gp {
using namespace std;
using namespace ai::sg;
template<typename T> struct string_traits {
};

namespace internal {
/// Throw exception for attempt to convert null to given type.
void throw_null_conversion(const string &type);
}

#define GP_DECLARE_STRING_TRAITS_SPECIALIZATION(T)			\
template<> struct  string_traits<T>			\
{									\
  typedef T subject_type;						\
  static const char *name() { return #T; }				\
  static bool has_null() { return false; }				\
  static bool is_null(T) { return false; }				\
  static T null() 							\
    { internal::throw_null_conversion(name()); return subject_type(); }	\
  static void from_string(const char Str[], T &Obj);			\
  static string to_string(T Obj);				\
};

GP_DECLARE_STRING_TRAITS_SPECIALIZATION(bool)

GP_DECLARE_STRING_TRAITS_SPECIALIZATION(short)
GP_DECLARE_STRING_TRAITS_SPECIALIZATION(unsigned short)
GP_DECLARE_STRING_TRAITS_SPECIALIZATION(int)
GP_DECLARE_STRING_TRAITS_SPECIALIZATION(unsigned int)
GP_DECLARE_STRING_TRAITS_SPECIALIZATION(long)
GP_DECLARE_STRING_TRAITS_SPECIALIZATION(unsigned long)
GP_DECLARE_STRING_TRAITS_SPECIALIZATION(float)
GP_DECLARE_STRING_TRAITS_SPECIALIZATION(double)
#undef GP_DECLARE_STRING_TRAITS_SPECIALIZATION

/// String traits for C-style string ("pointer to const char")
template<> struct string_traits<const char *> {
	static const char *name() {
		return "const char *";
	}
	static bool has_null() {
		return true;
	}
	static bool is_null(const char *t) {
		return !t;
	}
	static const char *null() {
		return NULL;
	}
	static void from_string(const char Str[], const char *&Obj) {
		Obj = Str;
	}
	static string to_string(const char *Obj) {
		return Obj;
	}
};

/// String traits for non-const C-style string ("pointer to char")
template<> struct string_traits<char *> {
	static const char *name() {
		return "char *";
	}
	static bool has_null() {
		return true;
	}
	static bool is_null(const char *t) {
		return !t;
	}
	static const char *null() {
		return NULL;
	}

	// Don't allow this conversion since it breaks const-safety.
	// static void from_string(const char Str[], char *&Obj);

	static string to_string(char *Obj) {
		return Obj;
	}
};

/// String traits for C-style string constant ("array of char")
template<size_t N> struct string_traits<char[N]> {
	static const char *name() {
		return "char[]";
	}
	static bool has_null() {
		return true;
	}
	static bool is_null(const char t[]) {
		return !t;
	}
	static const char *null() {
		return NULL;
	}
	static string to_string(const char Obj[]) {
		return Obj;
	}
};

/// String traits for "array of const char."
/** Visual Studio 2010 isn't happy without this redundant specialization.
 * Other compilers shouldn't need it.
 */
template<size_t N> struct string_traits<const char[N]> {
	static const char *name() {
		return "char[]";
	}
	static bool has_null() {
		return true;
	}
	static bool is_null(const char t[]) {
		return !t;
	}
	static const char *null() {
		return NULL;
	}
	static string to_string(const char Obj[]) {
		return Obj;
	}
};

template<> struct string_traits<string> {
	static const char *name() {
		return "string";
	}
	static bool has_null() {
		return false;
	}
	static bool is_null(const string &) {
		return false;
	}
	static string null() {
		internal::throw_null_conversion(name());
		return string();
	}
	static void from_string(const char Str[], string &Obj) {
		Obj = Str;
	}
	static string to_string(const string &Obj) {
		return Obj;
	}
};

template<> struct string_traits<const string> {
	static const char *name() {
		return "const string";
	}
	static bool has_null() {
		return false;
	}
	static bool is_null(const string &) {
		return false;
	}
	static const string null() {
		internal::throw_null_conversion(name());
		return string();
	}
	static const string to_string(const string &Obj) {
		return Obj;
	}
};

template<> struct string_traits<stringstream> {
	static const char *name() {
		return "stringstream";
	}
	static bool has_null() {
		return false;
	}
	static bool is_null(const stringstream &) {
		return false;
	}
	static stringstream null() {
		internal::throw_null_conversion(name());
		// No, dear compiler, we don't need a return here.
		throw 0;
	}
	static void from_string(const char Str[], stringstream &Obj) {
		Obj.clear();
		Obj << Str;
	}
	static string to_string(const stringstream &Obj) {
		return Obj.str();
	}
};

template<> struct string_traits<char> {
	static const char *name() {
		return "char";
	}
	static bool has_null() {
		return false;
	}
	static bool is_null(const char &) {
		return false;
	}
	static char null() {
		internal::throw_null_conversion(name());
		// No, dear compiler, we don't need a return here.
		throw 0;
	}
	static void from_string(const char Str[], char &Obj) {
		Obj = Str[0];
	}
	static string to_string(const char &Obj) {
		return string(1, Obj);
	}
};

// TODO: Implement date conversions

/// Attempt to convert postgres-generated string to given built-in type
/** If the form of the value found in the string does not match the expected
 * type, e.g. if a decimal point is found when converting to an integer type,
 * the conversion fails.  Overflows (e.g. converting "9999999999" to a 16-bit
 * C++ type) are also treated as errors.  If in some cases this behaviour should
 * be inappropriate, convert to something bigger such as @c long @c int first
 * and then truncate the resulting value.
 *
 * Only the simplest possible conversions are supported.  No fancy features
 * such as hexadecimal or octal, spurious signs, or exponent notation will work.
 * No whitespace is stripped away.  Only the kinds of strings that come out of
 * PostgreSQL and out of to_string() can be converted.
 */
template<typename T>
inline void from_string(const char Str[], T &Obj) {
	if (!Str)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Attempt to read NULL string")).str(SGLOCALE));
	string_traits<T>::from_string(Str, Obj);
}

/// Conversion with known string length (for strings that may contain nuls)
/** This is only used for strings, where embedded nul bytes should not determine
 * the end of the string.
 *
 * For all other types, this just uses the regular, nul-terminated version of
 * from_string().
 */
template<typename T> inline void from_string(const char Str[], T &Obj, size_t) {
	return from_string(Str, Obj);
}

template<>
inline void from_string<string>(const char Str[], string &Obj, size_t len) //[t0]
		{
	if (!Str)
		throw bad_param(__FILE__, __LINE__, 0, (_("ERROR: Attempt to read NULL string")).str(SGLOCALE));
	Obj.assign(Str, len);
}

template<typename T>
inline void from_string(const string &Str, T &Obj) //[t45]
		{
	from_string(Str.c_str(), Obj);
}

template<typename T>
inline void from_string(const stringstream &Str, T &Obj) //[t0]
		{
	from_string(Str.str(), Obj);
}

template<> inline void from_string(const string &Str, string &Obj) //[t46]
		{
	Obj = Str;
}

namespace internal {
/// Compute numeric value of given textual digit (assuming that it is a digit)
inline int digit_to_number(char c) throw () {
	return c - '0';
}
inline char number_to_digit(int i) throw () {
	return static_cast<char>(i + '0');
}
} // namespace pqxx::internal

/// Convert built-in type to a readable string that PostgreSQL will understand
/** No special formatting is done, and any locale settings are ignored.  The
 * resulting string will be human-readable and in a format suitable for use in
 * SQL queries.
 */
template<typename T> inline string to_string(const T &Obj) {
	return string_traits<T>::to_string(Obj);
}

//@}

}
}
}

#endif

