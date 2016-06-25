#if !defined(__COMMON_DPARSER_H__)
#define __COMMON_DPARSER_H__

#include "cmd_dparser.h"

namespace ai
{
namespace sg
{

class h_dparser : public cmd_dparser
{
public:
	h_dparser(int flags_, const std::vector<cmd_dparser_t>& parsers_);
	~h_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	dparser_result_t show_topic() const;

	const std::vector<cmd_dparser_t>& parsers;
	std::string topic;
};

class q_dparser : public cmd_dparser
{
public:
	q_dparser(int flags_);
	~q_dparser();

	dparser_result_t parse(int argc, char **argv);
};

class dbc_admin;
class system_dparser : public cmd_dparser
{
public:
	system_dparser(int flags_, const std::string& command_, dbc_admin *admin_mgr_);
	~system_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	dparser_result_t do_system(const std::string& command, int argc, char **argv);
	dparser_result_t do_boot();
	dparser_result_t do_shut();

	std::string command;
	dbc_admin *admin_mgr;
};

}
}

#endif

