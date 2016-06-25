#if !defined(__SQL_DPARSER_H__)
#define __SQL_DPARSER_H__

#include "cmd_dparser.h"

namespace ai
{
namespace sg
{

class desc_dparser : public cmd_dparser
{
public:
	desc_dparser(int flags_);
	~desc_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	std::string table_name;
};

class dbc_admin;
class sql_dparser : public cmd_dparser
{
public:
	sql_dparser(int flags_, const std::string& command_, dbc_admin *admin_mgr_);
	~sql_dparser();

	dparser_result_t parse(int argc, char **argv);

private:
	dparser_result_t do_screate(int argc, char **argv);
	dparser_result_t do_create(const string& sql);
	dparser_result_t do_drop(int argc, char **argv);
	dparser_result_t do_alter(int argc, char **argv);
	dparser_result_t do_commit();
	dparser_result_t do_rollback();
	dparser_result_t do_select(const string& sql);
	dparser_result_t do_insert(const string& sql);
	dparser_result_t do_sdc_select(const string& sql);
	dparser_result_t do_update(const string& sql);
	dparser_result_t do_delete(const string& sql);
	dparser_result_t do_truncate(int argc, char **argv);

	std::string command;
	dbc_admin *admin_mgr;
};

}
}

#endif

