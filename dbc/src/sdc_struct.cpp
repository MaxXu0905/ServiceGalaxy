#include "sdc_struct.h"

namespace ai
{
namespace sg
{

const char * sdc_rqst_t::data() const
{
	return reinterpret_cast<const char *>(&placeholder);
}

char * sdc_rqst_t::data()
{
	return reinterpret_cast<char *>(&placeholder);
}

const char * sdc_rply_t::data() const
{
	return reinterpret_cast<const char *>(&placeholder);
}

char * sdc_rply_t::data()
{
	return reinterpret_cast<char *>(&placeholder);
}

}
}

