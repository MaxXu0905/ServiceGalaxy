#include "endian_convert.h"

namespace ai
{
namespace sg
{

short endian_convert::convert_short(short data)
{
	return (data << 8) | (data >> 8);
}

unsigned short endian_convert::convert_ushort(unsigned short data)
{
	return (data << 8) | (data >> 8);
}

int endian_convert::convert_int(int data)
{
	return static_cast<int>(((data & 0x000000ffU) << 24)
		| ((data & 0x0000ff00U) << 8)
		| ((data & 0x00ff0000U) >> 8)
		| ((data & 0xff000000U) >> 24));
}

unsigned int endian_convert::convert_uint(unsigned int data)
{
	return ((data & 0x000000ffU) << 24)
		| ((data & 0x0000ff00U) << 8)
		| ((data & 0x00ff0000U) >> 8)
		| ((data & 0xff000000U) >> 24);
}

long endian_convert::convert_long(long data)
{
#if defined(_TMLONG64)
	return static_cast<long>(((data & 0x00000000000000ffUL) << 56)
		| ((data & 0x000000000000ff00UL) << 40)
		| ((data & 0x0000000000ff0000UL) << 24)
		| ((data & 0x00000000ff000000UL) << 8)
		| ((data & 0x000000ff00000000UL) >> 8)
		| ((data & 0x0000ff0000000000UL) >> 24)
		| ((data & 0x00ff000000000000UL) >> 40)
		| ((data & 0xff00000000000000UL) >> 56));
#else
	return static_cast<long>(((data & 0x000000ffUL) << 24)
		| ((data & 0x0000ff00UL) << 8)
		| ((data & 0x00ff0000UL) >> 8)
		| ((data & 0xff000000UL) >> 24));
#endif
}

unsigned long endian_convert::convert_ulong(unsigned long data)
{
#if defined(_TMLONG64)
	return ((data & 0x00000000000000ffUL) << 56)
		| ((data & 0x000000000000ff00UL) << 40)
		| ((data & 0x0000000000ff0000UL) << 24)
		| ((data & 0x00000000ff000000UL) << 8)
		| ((data & 0x000000ff00000000UL) >> 8)
		| ((data & 0x0000ff0000000000UL) >> 24)
		| ((data & 0x00ff000000000000UL) >> 40)
		| ((data & 0xff00000000000000UL) >> 56);
#else
	return ((data & 0x000000ffUL) << 24)
		| ((data & 0x0000ff00UL) << 8)
		| ((data & 0x00ff0000UL) >> 8)
		| ((data & 0xff000000UL) >> 24);
#endif
}

}
}

