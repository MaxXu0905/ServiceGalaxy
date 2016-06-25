#include <sys/time.h>
#include "machine.h"
#include "perf_stat.h"

namespace ai
{
namespace sg
{

using namespace std;

#if defined(PERF_STAT)

boost::unordered_map<string, stat_t> perf_stat::stat_map;
boost::unordered_map<string, int> perf_stat::count_map;
pthread_once_t perf_stat::perf_once_control = PTHREAD_ONCE_INIT;

void perf_stat::perf_init()
{
	signal(SIGUSR1, perf_stat::perf_signal);
	atexit(perf_dump);
}

void perf_stat::perf_signal(int signo)
{
	perf_dump();
}

void perf_stat::perf_dump()
{
	for (boost::unordered_map<string, stat_t>::const_iterator iter = perf_stat::stat_map.begin(); iter != perf_stat::stat_map.end(); ++iter) {
		int count = count_map[iter->first];
		cout << (_("perf_name = [{1}] loop count = [{2}] clock seconds elapsed = [{3}] seconds elapsed = [{4}s {5}us]")
			% iter->first
			% count
			% (static_cast<double>(iter->second.clocks) / CLOCKS_PER_SEC)
			% iter->second.tv_sec
			% iter->second.tv_usec ).str(SGLOCALE) << std::endl;
	}
}

#endif

perf_stat::perf_stat(const string& perf_name_, int print_batch_)
{
#if defined(PERF_STAT)
	pthread_once(&perf_once_control, perf_init);

	perf_name = perf_name_;
	print_batch = print_batch_;
	count_map[perf_name]++;
	begin_clock = clock();
	gettimeofday(&begin_time, NULL);
#endif
}

perf_stat::~perf_stat()
{
#if defined(PERF_STAT)
	clock_t end_clock = clock();
	timeval end_time;
	gettimeofday(&end_time, NULL);
	stat_t& value = stat_map[perf_name];
	value.clocks += end_clock - begin_clock;
	value.tv_sec += end_time.tv_sec - begin_time.tv_sec;
	value.tv_usec += end_time.tv_usec - begin_time.tv_usec;
	if (value.tv_usec < 0) {
		value.tv_sec--;
		value.tv_usec += 1000000;
	} else {
		value.tv_sec += value.tv_usec / 1000000;
		value.tv_usec = value.tv_usec % 1000000;
	}

	int count = count_map[perf_name];
	if (count % print_batch == 0) {
		cout << (_("perf_name = [{1}] loop count = [{2}] clock seconds elapsed = [{3}] seconds elapsed = [{4}s {5}us]")
			% perf_name
			% count
			% (static_cast<double>(value.clocks) / CLOCKS_PER_SEC)
			% value.tv_sec
			% value.tv_usec ).str(SGLOCALE) << std::endl;
	}
#endif
}

}
}

