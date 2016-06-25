#if !defined(__ADMIN_SERVER_H__)
#define __ADMIN_SERVER_H__

#include "sg_internal.h"
#include "cmd_parser.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

class admin_server : public sg_manager
{
public:
	admin_server();
	~admin_server();

	int run();
	void maininit();

private:
	parser_result_t process();

	std::string cmd;
	int& global_flags;
	bool& echo;
	bool& verbose;
	bool& page;
	std::vector<cmd_parser_t> parsers;

	const char *oldbbtype;
	int exit_code;
};

}
}

#endif

