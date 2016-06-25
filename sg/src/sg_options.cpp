#include "sg_internal.h"

namespace ai
{
namespace sg
{

sg_options::sg_options(std::ostream& os_, int flags_)
	: os(os_),
	  flags(flags_)
{
	first = true;
}

sg_options& sg_options::operator()(int mask, const std::string& msg)
{
	if (flags & mask) {
		if (first)
			first = false;
		else
			os << ",";
		os << msg;
	}

	return *this;
}

sg_resource_options_t::sg_resource_options_t(int options_)
	: options(options_)
{
}

std::ostream& operator<<(std::ostream& os, const sg_resource_options_t& self)
{
	sg_options(os, self.options)
		(SGLAN, "LAN")
		(SGMIGALLOWED, "MIGALLOWED")
		(SGHA, "HA")
		(SGACCSTATS, "ACCSTATS")
		(SGCLEXITL, "CLEXITL")
		(SGAA, "AA")
    ;

    return os;
}

sg_resource_ldbal_t::sg_resource_ldbal_t(int ldbal_)
	: ldbal(ldbal_)
{
}

std::ostream& operator<<(std::ostream& os, const sg_resource_ldbal_t& self)
{
	sg_options(os, self.ldbal)
		(LDBAL_RR, "RR")
		(LDBAL_RT, "RT")
    ;

    return os;
}

sg_qte_options_t::sg_qte_options_t(int options_)
	: options(options_)
{
}

std::ostream& operator<<(std::ostream& os, const sg_qte_options_t& self)
{
	sg_options(os, self.options)
		(RESTARTABLE, "RESTARTABLE")
    ;

    return os;
}

sg_server_status_t::sg_server_status_t(int options_)
	: options(options_)
{
}

std::ostream& operator<<(std::ostream& os, const sg_server_status_t& self)
{
	sg_options(os, self.options)
		(IDLE, "IDLE")
		(BUSY, "BUSY")
		(RESTARTING, "RESTARTING")
		(SUSPENDED, "SUSPENDED")
		(CLEANING, "CLEANING")
		(SHUTDOWN, "SHUTDOWN")
		(BRSUSPEND, "BRSUSPEND")
		(MIGRATING, "MIGRATING")
		(PSUSPEND, "PSUSPEND")
		(SPAWNING, "SPAWNING")
		(SVCTIMEDOUT, "SVCTIMEDOUT")
		(EXITING, "EXITING")
		(SVRKILLED, "SVRKILLED")
    ;

    return os;
}

sg_service_policy_t::sg_service_policy_t(int svc_policy_)
	: svc_policy(svc_policy_)
{
}

std::ostream& operator<<(std::ostream& os, const sg_service_policy_t& self)
{
	sg_options(os, self.svc_policy)
		(POLICY_LF, "LF")
		(POLICY_RR, "RR")
		(POLICY_IF, "IF")
    ;

    return os;
}

sg_client_status_t::sg_client_status_t(int options_)
	: options(options_)
{
}

std::ostream& operator<<(std::ostream& os, const sg_client_status_t& self)
{
	sg_options(os, self.options)
		(P_BUSY, "BUSY")
		(P_TRAN, "TRAN")
    ;

    return os;
}

}
}

