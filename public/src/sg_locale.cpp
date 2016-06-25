#include "sg_locale.h"
#include "user_exception.h"

namespace ai
{
namespace sg
{

const char DOMAIN_NAME[] = "sg";

sg_locale sg_locale::_instance;

sg_locale::sg_locale()
{
	char *ptr = ::getenv("SGROOT");
	if (ptr == NULL)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: SGROOT environment not set")).str(SGLOCALE));

	std::string path = ptr;
	path += "/etc";

	gen.add_messages_path(path);
	gen.add_messages_domain(DOMAIN_NAME);

	ptr = ::getenv("SGLANG");
	if (ptr == NULL || strcasecmp(ptr, "Y") != 0)
		::putenv(strdup("LANG=zh_CN.GBK"));
	_locale = gen("");
}

sg_locale::~sg_locale()
{
}

std::locale& sg_locale::locale()
{
	return _instance._locale;
}

}
}

