#if !defined(__BB_DPARSER_H__)
#define __BB_DPARSER_H__

#include "cmd_dparser.h"

namespace ai
{
namespace sg
{

class dbc_admin;
class bbc_dparser : public cmd_dparser
{
public:
	bbc_dparser(int flags_);
	~bbc_dparser();

	dparser_result_t parse(int argc, char **argv);
};

class bbp_dparser : public cmd_dparser
{
public:
	bbp_dparser(int flags_);
	~bbp_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	dparser_result_t show_bbp() const;
};

class dump_dparser : public cmd_dparser
{
public:
	dump_dparser(int flags_);
	~dump_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	std::string table_name;
};

class dumpdb_dparser : public cmd_dparser
{
public:
	dumpdb_dparser(int flags_);
	~dumpdb_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	int table_id;
	std::string table_name;
	int parallel;
	long commit_size;
};

class extend_dparser : public cmd_dparser
{
public:
	extend_dparser(int flags_);
	~extend_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	int add_count;
};

class kill_dparser : public cmd_dparser
{
public:
	kill_dparser(int flags_);
	~kill_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	int session_id;
	int signo;
};

class set_dparser : public cmd_dparser
{
public:
	set_dparser(int flags_);
	~set_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	std::string key;
	std::string value;
};

class forget_dparser : public cmd_dparser
{
public:
	forget_dparser(int flags_);
	~forget_dparser();

	dparser_result_t parse(int argc, char **argv);
};

class check_dparser : public cmd_dparser
{
public:
	check_dparser(int flags_);
	~check_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	void print(const string& head, const char *data, const dbc_data_t& te_meta);

	std::string table_name;
	std::string index_name;
	int index_id;
};

class enable_dparser : public cmd_dparser
{
public:
	enable_dparser(int flags_);
	~enable_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	std::string table_name;
	std::string index_name;
	int index_id;
};

class disable_dparser : public cmd_dparser
{
public:
	disable_dparser(int flags_);
	~disable_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	std::string table_name;
	std::string index_name;
	int index_id;
};

class reload_dparser : public cmd_dparser
{
public:
	reload_dparser(int flags_);
	~reload_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	static void s_load(bool safe_load, int64_t user_id, dbc_te_t *te, int& retval);

	std::vector<std::string> table_names;
};

}
}

#endif

