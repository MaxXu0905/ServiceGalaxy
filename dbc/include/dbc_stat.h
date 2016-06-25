#if !defined(__DBC_STAT_H__)
#define __DBC_STAT_H__

#include <sys/time.h>
#include "machine.h"

namespace ai
{
namespace sg
{

struct dbc_stat_t
{
	int accword;
	int times;
	clock_t clocks;
	double elapsed;
	double min_time;
	double max_time;

	void init();
	dbc_stat_t& operator=(dbc_stat_t& rhs);
};

class dbc_stat
{
public:
	dbc_stat(int stat_level_, dbc_stat_t& result_);
	~dbc_stat();

private:
	int stat_level;
	dbc_stat_t& result;
	clock_t begin_clock;
	timeval begin_time;
};

}
}

#endif

