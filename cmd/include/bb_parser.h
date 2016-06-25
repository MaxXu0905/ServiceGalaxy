#if !defined(__BB_PARSER_H__)
#define __BB_PARSER_H__

#include "cmd_parser.h"

namespace ai
{
namespace sg
{

class bbc_parser : public cmd_parser
{
public:
	bbc_parser(int flags_);
	~bbc_parser();

	parser_result_t parse(int argc, char **argv);

private:
	std::string sect_name;
};

class bbi_parser : public cmd_parser
{
public:
	bbi_parser(int flags_);
	~bbi_parser();

	parser_result_t parse(int argc, char **argv);

private:
	parser_result_t show_bbi() const;
};

class bbm_parser : public cmd_parser
{
public:
	bbm_parser(int flags_);
	~bbm_parser();

	parser_result_t parse(int argc, char **argv);

private:
	parser_result_t show_bbm() const;
	void domaps(const char *uname, const char *lname, int maxents, int numbkts, int entsize,
		long hash, long base, int tfree, int *hashtab) const;
	int Pfmap(long base, int offset, int elen, int vflag) const;
	int Pumap(long offset, long hashoff, int nbkts, int elen, int& vflag) const;

	bool qsect;
	bool ssect;
	bool csect;
};

class bbp_parser : public cmd_parser
{
public:
	bbp_parser(int flags_);
	~bbp_parser();

	parser_result_t parse(int argc, char **argv);

private:
	parser_result_t show_bbp() const;
};

class bbr_parser : public cmd_parser
{
public:
	bbr_parser(int flags_);
	~bbr_parser();

	parser_result_t parse(int argc, char **argv);
};

class bbs_parser : public cmd_parser
{
public:
	bbs_parser(int flags_);
	~bbs_parser();

	parser_result_t parse(int argc, char **argv);

private:
	parser_result_t show_bbs();
};

class du_parser : public cmd_parser
{
public:
	du_parser(int flags_);
	~du_parser();

	parser_result_t parse(int argc, char **argv);

private:
	message_pointer dump(const sg_proc_t& bbm);

	std::string c_value;
};

class dumem_parser : public cmd_parser
{
public:
	dumem_parser(int flags_);
	~dumem_parser();

	parser_result_t parse(int argc, char **argv);

private:
	std::string c_value;
};

class loadmem_parser : public cmd_parser
{
public:
	loadmem_parser(int flags_);
	~loadmem_parser();

	parser_result_t parse(int argc, char **argv);

private:
	std::string c_value;
	int ipckey;
};

}
}

#endif

