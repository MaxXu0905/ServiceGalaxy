#include <limits>
#include "dbc_stat.h"
#include "ushm.h"
#include <boost/scope_exit.hpp>

namespace ai
{
namespace sg
{

void dbc_stat_t::init()
{
	accword = 0;
	times = 0;
	clocks = 0;
	elapsed = 0.0;
	min_time = numeric_limits<double>::max();
	max_time = numeric_limits<double>::min();
}

dbc_stat_t& dbc_stat_t::operator=(dbc_stat_t& rhs)
{
	accword = 0;
	times = rhs.times;
	clocks = rhs.clocks;
	elapsed = rhs.elapsed;
	min_time = rhs.min_time;
	max_time = rhs.max_time;

	return *this;
}

dbc_stat::dbc_stat(int stat_level_, dbc_stat_t& result_)
	: stat_level(stat_level_),
	  result(result_)
{
	if (stat_level <= 0)
		return;

	begin_clock = clock();
	gettimeofday(&begin_time, NULL);
}

dbc_stat::~dbc_stat()
{
	if (stat_level <= 0)
		return;

	clock_t end_clock = clock();
	timeval end_time;
	gettimeofday(&end_time, NULL);
	double elapsed = (end_time.tv_sec - begin_time.tv_sec) + (end_time.tv_usec - begin_time.tv_usec) / 1000000.0;

	ipc_sem::tas(&result.accword, SEM_WAIT);
	BOOST_SCOPE_EXIT((&result)) {
		ipc_sem::semclear(&result.accword);
	} BOOST_SCOPE_EXIT_END

	result.times++;
	result.clocks += end_clock - begin_clock;
	result.elapsed += elapsed;

	if (stat_level >= 10) {
		if (result.min_time > elapsed)
			result.min_time = elapsed;
		if (result.max_time < elapsed)
			result.max_time = elapsed;
	}
}

}
}

