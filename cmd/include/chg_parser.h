#if !defined(__CHG_PARSER_H__)
#define __CHG_PARSER_H__

#include "cmd_parser.h"

namespace ai
{
namespace sg
{

class adv_parser : public cmd_parser
{
public:
	adv_parser(int flags_);
	~adv_parser();

	parser_result_t parse(int argc, char **argv);

private:
	std::string class_name;
};

class una_parser : public cmd_parser
{
public:
	una_parser(int flags_);
	~una_parser();

	parser_result_t parse(int argc, char **argv);
};

class lp_parser : public cmd_parser
{
public:
	lp_parser(int flags_, int type_);
	~lp_parser();

	parser_result_t parse(int argc, char **argv);

private:
	parser_result_t chg_lp(int type, const string& name, long lrange, long hrange);

	int type;
	long c_value;
};

class migg_parser : public cmd_parser
{
public:
	migg_parser(int flags_);
	~migg_parser();

	parser_result_t parse(int argc, char **argv);
};

class migm_parser : public cmd_parser
{
public:
	migm_parser(int flags_);
	~migm_parser();

	parser_result_t parse(int argc, char **argv);
};

class status_parser : public cmd_parser
{
public:
	status_parser(int flags_, int type_);
	~status_parser();

	parser_result_t parse(int argc, char **argv);

private:
	parser_result_t chg_status();

	int type;
	std::string cmd_desc;
};


}
}

#endif

