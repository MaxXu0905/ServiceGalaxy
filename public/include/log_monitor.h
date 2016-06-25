#if !defined(__LOG_MONITOR_H__)
#define __LOG_MONITOR_H__

#include "machine.h"

namespace ai
{
namespace sg
{

int const MONITOR_KEY_LEN = 127;

struct monitor_config_t
{
	int monitor_count;
	int monitor_years;
	int monitor_months;
	int monitor_days;
	int monitor_hours;
	int monitor_keys;
	int monitor_key_len;
};

struct monitor_header_t
{
	ipc_sem sem;
	int key_count;
	long placeholder;
};

class log_monitor
{
public:
	log_monitor(key_t ipckey, bool creat = false);
	~log_monitor();

	bool open(const string& monitor_key);
	int find(const string& field_key);
	void set_value(int index, double value);
	double get_value(int index);

private:
	monitor_config_t cfg;
	map<string, int> key_map;
	double *field_addr;
};

}
}

#endif

