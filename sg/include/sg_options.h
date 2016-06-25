#if !defined(__SG_OPTIONS_H__)
#define __SG_OPTIONS_H__

#include <ostream>

namespace ai
{
namespace sg
{

class sg_options
{
public:
	sg_options(std::ostream& os, int flags_);

	sg_options& operator()(int mask, const std::string& msg);

private:
	std::ostream& os;
	int flags;
	bool first;
};

struct sg_resource_options_t
{
public:
	sg_resource_options_t(int options_);

	friend std::ostream& operator<<(std::ostream& os, const sg_resource_options_t& self);

private:
	int options;
};

struct sg_resource_ldbal_t
{
public:
	sg_resource_ldbal_t(int ldbal_);

	friend std::ostream& operator<<(std::ostream& os, const sg_resource_ldbal_t& self);

private:
	int ldbal;
};

struct sg_qte_options_t
{
public:
	sg_qte_options_t(int options_);

	friend std::ostream& operator<<(std::ostream& os, const sg_qte_options_t& self);

private:
	int options;
};

struct sg_server_status_t
{
public:
	sg_server_status_t(int options_);

	friend std::ostream& operator<<(std::ostream& os, const sg_server_status_t& self);

private:
	int options;
};

struct sg_service_policy_t
{
public:
	sg_service_policy_t(int svc_policy_);

	friend std::ostream& operator<<(std::ostream& os, const sg_service_policy_t& self);

private:
	int svc_policy;
};


struct sg_client_status_t
{
public:
	sg_client_status_t(int options_);

	friend std::ostream& operator<<(std::ostream& os, const sg_client_status_t& self);

private:
	int options;
};

}
}

#endif

