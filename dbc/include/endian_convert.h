#if !defined(__ENDIAN_CONVERT_H__)
#define __ENDIAN_CONVERT_H__

namespace ai
{
namespace sg
{

class endian_convert
{
public:
	static short convert_short(short data);
	static unsigned short convert_ushort(unsigned short data);
	static int convert_int(int data);
	static unsigned int convert_uint(unsigned int data);
	static long convert_long(long data);
	static unsigned long convert_ulong(unsigned long data);
};

}
}

#endif

