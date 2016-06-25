#if !defined(__SG_ALIGN_H__)
#define __SG_ALIGN_H__

namespace ai
{
namespace sg
{

struct short_align_t
{
	char padding;
	short value;
};

struct int_align_t
{
	char padding;
	int value;
};

struct long_align_t
{
	char padding;
	long value;
};

struct float_align_t
{
	char padding;
	float value;
};

struct double_align_t
{
	char padding;
	double value;
};

struct time_t_align_t
{
	char padding;
	time_t value;
};


int const CHAR_ALIGN = sizeof(char);
int const SHORT_ALIGN = offsetof(short_align_t, value);
int const INT_ALIGN = offsetof(int_align_t, value);
int const LONG_ALIGN = offsetof(long_align_t, value);
int const FLOAT_ALIGN = offsetof(float_align_t, value);
int const DOUBLE_ALIGN = offsetof(double_align_t, value);
int const TIME_T_ALIGN = offsetof(time_t_align_t, value);

}
}

#endif

