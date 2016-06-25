#if !defined(__PRT_PARSER_H__)
#define __PRT_PARSER_H__

#include "cmd_parser.h"

namespace ai
{
namespace sg
{

class d_parser : public cmd_parser
{
public:
	d_parser(int flags_);
	~d_parser();

	parser_result_t parse(int argc, char **argv);
};

class option_parser : public cmd_parser
{
public:
	option_parser(const std::string& option_name_, int flags_, bool& value_);
	~option_parser();

	parser_result_t parse(int argc, char **argv);

private:
	std::string option_name;
	bool& value;
	std::string option;
};

class pclt_parser : public cmd_parser
{
public:
	pclt_parser(int flags_);
	~pclt_parser();

	parser_result_t parse(int argc, char **argv);

private:
	void show_terse(const vector<sg_rte_t>& clients) const;
	void show_verbose(const vector<sg_rte_t>& clients) const;
};

class pg_parser : public cmd_parser
{
public:
	pg_parser(int flags_);
	~pg_parser();

	parser_result_t parse(int argc, char **argv);

private:
	void show(const sg_sgte_t& sgte) const;
};

class pq_parser : public cmd_parser
{
public:
	pq_parser(int flags_);
	~pq_parser();

	parser_result_t parse(int argc, char **argv);
};

class psr_parser : public cmd_parser
{
public:
	psr_parser(int flags_);
	~psr_parser();

	parser_result_t parse(int argc, char **argv);

private:
	bool update(sg_ste_t& s1, const sg_ste_t& s2, int mid);
	void show_head(bool ph, int mid) const;
	void show_terse(const sg_ste_t& ste, const sg_qte_t& qte, int mid) const;
	void show_verbose(const sg_ste_t& ste, const sg_qte_t& qte) const;
};

class psc_parser : public cmd_parser
{
public:
	psc_parser(int flags_);
	~psc_parser();

	parser_result_t parse(int argc, char **argv);

private:
	bool update(sg_scte_t& s1, const sg_scte_t& s2, int mid);
	void show_head(bool ph, int mid) const;
	void show_terse(const sg_scte_t& scte, const sg_qte_t& qte, int mid) const;
	void show_verbose(const sg_scte_t& scte, const sg_qte_t& qte) const;
};

class srp_parser : public cmd_parser
{
public:
	srp_parser(int flags_);
	~srp_parser();

	parser_result_t parse(int argc, char **argv);
};

class scp_parser : public cmd_parser
{
public:
	scp_parser(int flags_);
	~scp_parser();

	parser_result_t parse(int argc, char **argv);
};

class stats_parser : public cmd_parser
{
public:
	stats_parser(int flags_);
	~stats_parser();

	parser_result_t parse(int argc, char **argv);
};


}
}

#endif

