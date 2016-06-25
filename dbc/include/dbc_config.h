#if !defined(__DBC_CONFIG_H__)
#define __DBC_CONFIG_H__

#include "dbc_struct.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

struct data_index_t;
class dbc_config
{
public:
	static dbc_config& instance();

	bool open();
	void close();
	dbc_bbparms_t& get_bbparms();
	std::vector<data_index_t>& get_meta();
	std::vector<dbc_user_t>& get_users();
	std::map<int, dbc_data_t>& get_data_map();
	std::map<dbc_index_key_t, dbc_index_t>& get_index_map();
	std::map<dbc_index_key_t, dbc_func_t>& get_func_map();
	std::map<int, int>& get_primary_map();
	bool md5sum(std::string& result, bool is_client) const;

private:
	dbc_config();
	virtual ~dbc_config();

	void load_sys_table();
	void load_bbparms(const bpt::iptree& pt);
	void load_user(const bpt::iptree& pt);
	void load_data(const bpt::iptree& pt);
	void load_index(const bpt::iptree& pt);
	void load_func(const bpt::iptree& pt);
	void load_libs(const std::string& libs);
	void unload_libs();

	static void parse_perm(const string& perm_str, int& perm, const string& node_name);

	gpp_ctx *GPP;
	dbcp_ctx *DBCP;
	bool opened;
	boost::mutex mutex;
	dbc_bbparms_t bbparms;
	std::vector<data_index_t> meta;
	std::vector<dbc_user_t> users;
	std::map<int, dbc_data_t> data_map;
	std::map<dbc_index_key_t, dbc_index_t> index_map;
	std::map<dbc_index_key_t, dbc_func_t> func_map;
	std::map<int, int> primary_map;
	std::vector<void *> lib_handles;		// ¶¯Ì¬¿â¾ä±ú

	static dbc_config _instance;
};

}
}

#endif

