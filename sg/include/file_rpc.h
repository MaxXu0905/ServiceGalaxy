#if !defined(__FILE_RPC_H__)
#define __FILE_RPC_H__

#include "sg_internal.h"

namespace ai
{
namespace sg
{

class file_rpc : public sg_manager
{
public:
	static file_rpc& instance(sgt_ctx *SGT);

	bool dir(vector<std::string>& files, const std::string& sftp_prefix, const std::string& dir_name, const std::string& pattern);
	bool unlink(const std::string& sftp_prefix, const std::string& filename);
	bool mkdir(const string& sftp_prefix, const string& dir_name);
	bool rename(const std::string& sftp_prefix, const std::string& src_name, const std::string& dst_name);
	bool get(const std::string& sftp_prefix, const std::string& src_name, const std::string& dst_name);
	bool put(const std::string& sftp_prefix, const std::string& src_name, const std::string& dst_name);

private:
	file_rpc();
	virtual ~file_rpc();

	message_pointer send_msg;
	message_pointer recv_msg;

	friend class sgt_ctx;
};

}
}

#endif

