#if !defined(__SG_LOCALE_H__)
#define __SG_LOCALE_H__

#include <boost/locale.hpp>

#if defined(_)
#undef _
#endif

#define _(text) boost::locale::format(boost::locale::translate(text))
#define SGLOCALE ai::sg::sg_locale::locale()

namespace ai
{
namespace sg
{

class sg_locale
{
public:
	sg_locale();
	~sg_locale();

	static std::locale& locale();

private:
	boost::locale::generator gen;
	std::locale _locale;

	static sg_locale _instance;
};

}
}

#endif

