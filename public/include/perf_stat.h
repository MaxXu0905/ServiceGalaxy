#if !defined(__PERF_STAT_H__)
#define __PERF_STAT_H__

#include <map>
#include <string>
#include <boost/scope_exit.hpp>
#include <boost/chrono.hpp>
#include <boost/timer.hpp>
#include <boost/unordered_map.hpp>

namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

using namespace std;

struct stat_t
{
	clock_t clocks;
	time_t tv_sec;
	long tv_usec;

	stat_t()
	{
		clocks = 0;
		tv_sec = 0;
		tv_usec = 0;
	}
};

class perf_stat
{
public:
	perf_stat(const string& perf_name_, int print_batch_ = 1000);
	~perf_stat();

private:
#if defined(PERF_STAT)
	string perf_name;
	clock_t begin_clock;
	timeval begin_time;
	int print_batch;
	static boost::unordered_map<string, stat_t> stat_map;
	static boost::unordered_map<string, int> count_map;
	static pthread_once_t perf_once_control;

	static void perf_init();
	static void perf_signal(int signo);
	static void perf_dump();
#endif
};

#define FAST_PERF_STAT(perf_name, print_batch) \
	static int __count = 0; \
	static double __clocks = 0.0; \
	static double __elapsed = 0.0; \
	const char *__perf_name = perf_name; \
	timeval __begin_time; \
	gettimeofday(&__begin_time, NULL); \
	clock_t __begin_clock = clock(); \
	BOOST_SCOPE_EXIT((&__perf_name)(&__count)(&__clocks)(&__elapsed)(&__begin_clock)(&__begin_time)) { \
		clock_t end_clock = clock(); \
		timeval end_time; \
		gettimeofday(&end_time, NULL); \
		__count++; \
		__clocks += end_clock - __begin_clock; \
		__elapsed += (end_time.tv_sec - __begin_time.tv_sec) + (end_time.tv_usec - __begin_time.tv_usec) / 1000000.0; \
		if ((__count % print_batch) == 0) { \
			cout << (_("perf_name = [{1}] loop count = [{2}] clock seconds elapsed = [{3}] seconds elapsed = [{4}s]\n")\
			% __perf_name  \
			% __count  \
			% __clocks / CLOCKS_PER_SEC) \
			% __elapsed).str(SGLOCALE); \
		} \
	} BOOST_SCOPE_EXIT_END

}
}

#endif

