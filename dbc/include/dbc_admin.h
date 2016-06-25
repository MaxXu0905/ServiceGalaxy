#if !defined(__DBC_ADMIN_H__)
#define __DBC_ADMIN_H__

#include "dbc_internal.h"
#include "cmd_dparser.h"

namespace ai
{
namespace sg
{

using namespace ai::scci;

class dbc_admin : public dbc_manager
{
public:
	dbc_admin();
	~dbc_admin();

	int run(int argc, char *argv[]);
	void maininit();
	std::string& get_cmd();
	Dbc_Database * get_db();
	std::set<int>& get_mids();
	const std::string& get_user_name() const;
	const std::string& get_password() const;

private:
	dparser_result_t process();
	bool parse(sg_init_t& init_info, const string& sginfo);

	std::string cmd;
	int& global_flags;
	bool& echo;
	bool& verbose;
	bool& page;
	std::string& user_name;
	std::string& password;
	dbc_api& api_mgr;
	Dbc_Database *dbc_db;
	std::set<int> mids;
	std::vector<cmd_dparser_t> parsers;
	int exit_code;
};

}
}

#endif

