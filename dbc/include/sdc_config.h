#if !defined(__SDC_CONFIG_H__)
#define __SDC_CONFIG_H__

#include "dbc_struct.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

struct sdc_config_t
{
	std::string table_name;
	int mid;
	int table_id;
	int partition_id;

	bool operator<(const sdc_config_t& rhs) const {
		if (mid < rhs.mid)
			return true;
		else if (mid > rhs.mid)
			return false;

		if (table_id < rhs.table_id)
			return true;
		else if (table_id > rhs.table_id)
			return false;

		return partition_id < rhs.partition_id;
	}
};

class sdc_config : public dbc_manager
{
public:
	static sdc_config& instance(dbct_ctx *DBCT);

	std::set<sdc_config_t>& get_config_set();
	int get_partitions(int table_id);

private:
	sdc_config();
	virtual ~sdc_config();

	void load();
	void load_table(const bpt::iptree& pt);

	bool loaded;
	std::set<sdc_config_t> config_set;
	std::map<int, int> partitions_map;

	friend class dbct_ctx;
};

}
}

#endif

