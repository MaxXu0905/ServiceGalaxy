#if !defined(__BR_PARSER_H__)
#define __BR_PARSER_H__

#include "cmd_parser.h"

namespace ai
{
namespace sg
{

class m_parser : public cmd_parser
{
public:
	m_parser(int flags_);
	~m_parser();

	parser_result_t parse(int argc, char **argv);
};

class pcl_parser : public cmd_parser
{
public:
	pcl_parser(int flags_);
	~pcl_parser();

	parser_result_t parse(int argc, char **argv);

private:
	bool pclean(int& cleaned, int& outof);
};

class pnw_parser : public cmd_parser
{
public:
	pnw_parser(int flags_);
	~pnw_parser();

	parser_result_t parse(int argc, char **argv);
};

class rco_parser : public cmd_parser
{
public:
	rco_parser(int flags_);
	~rco_parser();

	parser_result_t parse(int argc, char **argv);
};

class stopl_parser : public cmd_parser
{
public:
	stopl_parser(int flags_);
	~stopl_parser();

	parser_result_t parse(int argc, char **argv);
};

}
}

#endif

