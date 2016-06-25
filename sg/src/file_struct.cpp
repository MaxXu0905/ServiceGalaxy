#include "file_struct.h"

namespace ai
{
namespace sg
{

const char * file_rqst_t::data() const
{
	return reinterpret_cast<const char *>(&placeholder);
}

char * file_rqst_t::data()
{
	return reinterpret_cast<char *>(&placeholder);
}

const char * file_rply_t::data() const
{
	return reinterpret_cast<const char *>(&placeholder);
}

char * file_rply_t::data()
{
	return reinterpret_cast<char *>(&placeholder);
}

}
}

