#if !defined(__COMMON_PARSER_H__)
#define __COMMON_PARSER_H__

#include "cmd_parser.h"

namespace ai
{
namespace sg
{

class h_parser : public cmd_parser
{
public:
	h_parser(int flags_, const std::vector<cmd_parser_t>& parsers_);
	~h_parser();

	parser_result_t parse(int argc, char **argv);

private:
	parser_result_t show_topic() const;

	const std::vector<cmd_parser_t>& parsers;
	std::string topic;
};

class q_parser : public cmd_parser
{
public:
	q_parser(int flags_);
	~q_parser();

	parser_result_t parse(int argc, char **argv);
};

class admin_server;
class system_parser : public cmd_parser
{
public:
	system_parser(int flags_, const std::string& command_, admin_server *admin_mgr_);
	~system_parser();

	parser_result_t parse(int argc, char **argv);

private:
	parser_result_t do_system(const std::string& command, int argc, char **argv);

	int mastermid;
	pid_t masterpid;
	std::string command;
	admin_server *admin_mgr;
};

}
}

#endif

