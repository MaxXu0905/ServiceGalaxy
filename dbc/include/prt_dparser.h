#if !defined(__PRT_DPARSER_H__)
#define __PRT_DPARSER_H__

#include "cmd_dparser.h"

namespace ai
{
namespace sg
{

class option_dparser : public cmd_dparser
{
public:
	option_dparser(const std::string& option_name_, int flags_, bool& value_);
	~option_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	std::string option_name;
	bool& value;
	std::string option;
};

class tep_dparser : public cmd_dparser
{
public:
	tep_dparser(int flags_);
	~tep_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	std::string table_name;
};

class show_dparser : public cmd_dparser
{
public:
	show_dparser(int flags_);
	~show_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	bool pattern_match(const string& pattern, const string& str);

	std::string topic;
	std::string pattern;
};

}
}

#endif

