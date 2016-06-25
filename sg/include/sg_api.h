#if !defined(__SG_API_H__)
#define __SG_API_H__

#include "sg_message.h"
#include "sg_struct.h"
#include "sg_manager.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

int const STATUS_OP_OR = 0x1;
int const STATUS_OP_AND = 0x2;
int const STATUS_OP_SET = 0x4;

int const SGBLK_NEXT = 1;
int const SGBLK_ALL = 2;

class sg_api : public sg_manager
{
public:
	static sg_api& instance();
	static sg_api& instance(sgt_ctx *SGT);

	bool init(const sg_init_t *init_info = NULL);
	bool fini();

	bool set_ctx(int context);
	bool get_ctx(int& context);
	bool set_blktime(int blktime, int flags);
	int get_blktime(int flags);

	bool call(const char *service, message_pointer& send_msg, message_pointer& recv_msg, int flags);
  	int acall(const char *service, message_pointer& send_msg, int flags);
	bool getrply(int& handle, message_pointer& recv_msg, int flags);
	bool advertise(const std::string& svc_name, const std::string& class_name);
	bool unadvertise(const std::string& svc_name);
	bool cancel(int cd);
	int getprio();
	bool setprio(int prio, int flags);

	void set_procname(const std::string& progname);
	void write_log(const char *filename, int linenum, const char *msg, int len = 0);
	void write_log(const char *filename, int linenum, const std::string& msg);
	void write_log(const std::string& msg);
	void write_log(const char *msg);

	int get_error_code() const;
	int get_native_code() const;
	int get_ur_code() const;
	int get_error_detail() const;
	const char * strerror() const;
	const char * strnative() const;

	// 根据key和version，获取配置信息
	bool get_config(std::string& config, const std::string& key, int version = -1);

private:
	sg_api();
	virtual ~sg_api();

	bool sginit(int proc_type, int slot, const sg_init_t *init_info);
	bool sgfini(int proc_type);
	bool call_internal(const char *service, message_pointer& send_msg, message_pointer& recv_msg, int flags);
	int acall_internal(const char *service, message_pointer& send_msg, int flags);
	bool getrply_internal(int& handle, message_pointer& recv_msg, int flags);
	int make_handle();
	void drop_handle(int mtype);
	int unadvertise_internal(sgid_t sgid, int flags);
	void initerr();
	void settimeleft();
	void clncallblktime();

	FRIEND_CLASS_DECLARE
};

}
}

#endif

