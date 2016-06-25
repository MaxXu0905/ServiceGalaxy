#if !defined(__METAREP_STRUCT_H__)
#define __METAREP_STRUCT_H__

namespace ai
{
namespace sg
{

struct sg_metarep_t
{
	int version;
	char key[1];
};

int const MREINVAL = 1;
int const MRENOENT = 2;
int const MRENOFILE = 3;
int const MREOS = 4;

const char METAREP_SVC_NAME[] = "METAREP";

}
}

#endif

