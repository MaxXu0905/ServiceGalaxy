#if !defined(__SG_MESSAGE_H__)
#define __SG_MESSAGE_H__

#include <ctime>
#include <iostream>
#include <string>
#include <cstdlib>
#include <deque>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include "sg_struct.h"

using namespace std;

namespace ai
{
namespace sg
{

// metahdr flags
enum sg_metahdr_flags_t
{
	METAHDR_FLAGS_IN_SHM = 0x1,
	METAHDR_FLAGS_GW = 0x2,
	METAHDR_FLAGS_ALARM = 0x4, 		/* sgmngr sending message to process
									  * to indicate that the process
									  * timed out, e.g. alarm went off */
	METAHDR_FLAGS_DBBMFWD = 0x8,	/* sgclst bound message forwarded */
	METAHDR_FLAGS_DYED = 0x10,		/* message dyeing on */
	METAHDR_FLAGS_CMPRS = 0x20		/* message is compressed */
};

int const SGPROTOCOL = 1;
int const MAXBCSTMTYP = 0x40000000;
int const BBRQMTYP = 0x40000000;	/* rqust to sgmngr */

struct alt_flds_t
{
	int origmid;		/* machine identifier of message originator */
 	int origslot;		/* TMRTE slot of message originator */
	int origtmstmp;		/* TMRTE timestamp of message originator */
	int prevbbv;		/* Previous BB version (RPC_RQUST bufs only) */
	int prevsgv;		/* Previous SGPROFILE vers (RPC_RQUST) */
	int newver;			/* New version number (either BB or SG) */
	sgid_t svcsgid;		/* SGID of the current service name */
	int bbversion;		/* Bulletin board version at call time */
	int msgid;			/* tag for message */
};

struct sg_metahdr_t
{
	int mtype;		// 消息类型
	int protocol;	// 消息的协议号
	int qaddr;		// 接收者的物理消息地址
	int mid;		// 接收者的主机号
	int size;		// 整个消息字节数
	int flags;		// 消息标识，如是否通过共享内存传递

	bool is_rply() const { return (mtype & RPLYBASE); }

	static int bbrqstmtype() { return BBRQMTYP; }
	static int bbrplymtype(pid_t pid) { return (BBRQMTYP + (pid & 0x07FFFFFF)); }	/* rply from sgmngr */
};

// 消息头
struct sg_msghdr_t
{
	int flags;
	int error_code;
	int native_code;
	int ur_code;
	int error_detail;
	sg_proc_t sender;
	sg_proc_t rplyto;
	sg_proc_t rcvr;
	char svc_name[MAX_SVC_NAME + 1];
	int handle;	// sender's handle
	int sendmtype;	// msg type to send
	int senditer;	// send iteration
	int rplymtype;	// msg type which reply should have
	int rplyiter;	// reply iteration
	alt_flds_t alt_flds;

	bool rplywanted() const { return (rplymtype != NULLMTYPE); }
	bool is_meta() const { return (flags & SGMETAMSG); }
};

struct sg_msg_t
{
	sg_metahdr_t metahdr;
	sg_msghdr_t msghdr;
	long placeholder;
};

// 通过共享内存块传递的消息内容
struct sg_shm_data_t
{
	int size;						// size in shared memory
	char name[MAX_SHM_NAME + 1];	// shared memory name
};

struct sg_clinfo_t
{
	char usrname[MAX_IDENT + 1];	/* client user name */
	char cltname[MAX_IDENT + 1];	/* application client name */
};

size_t const MSGHDR_LENGTH = sizeof(sg_msg_t) - sizeof(long);
size_t const MSG_MIN = (MSGHDR_LENGTH > 1024) ? MSGHDR_LENGTH : 1024;

class sg_message;

typedef boost::shared_ptr<sg_message> message_pointer;
typedef std::deque<message_pointer> sg_message_queue;

class sg_message
{
public:
	static message_pointer create();
	static message_pointer create(sgt_ctx *SGT);
	~sg_message();

	message_pointer clone() const;
	void encode();
	void decode();

	sg_message& operator=(const std::string& __str);
	sg_message& operator=(const char *__s);
	sg_message& operator=(char __c);

	size_t size() const;
	size_t length() const;
	void set_size(size_t __size);
	void set_length(size_t __len);
	size_t max_size() const;
	size_t raw_capacity() const;
	size_t capacity() const;
	void resize(size_t __n, char __c = char());
	void reserve(size_t __res_arg = 0);
	void clear();
	bool empty() const;

	const char& operator[](size_t __pos) const;
	char& operator[](size_t __pos);
	const char& at(size_t __pos) const;
	char& at(size_t __pos);

	sg_message& operator+=(const std::string& __str);
	sg_message& operator+=(const char *__s);
	sg_message& operator+=(char __c);

	sg_message& append(const std::string& __str);
	sg_message& append(const std::string& __str, size_t __pos, size_t __n);
	sg_message& append(const char *__s, size_t __n);
	sg_message& append(const char *__s);
	sg_message& append(size_t __n, char __c);

	sg_message& assign(const std::string& __str);
	sg_message& assign(const std::string& __str, size_t __pos, size_t __n);
	sg_message& assign(const char *__s, size_t __n);
	sg_message& assign(const char *__s);
	sg_message& assign(size_t __n, char __c);

	void swap(sg_message& msg);
	char * raw_data() { return _buffer; }
	const char * raw_data() const { return _buffer; }
	char * data() { return _data; }
	const char * data() const { return _data; }
	sg_rpc_rqst_t * rpc_rqst() { return reinterpret_cast<sg_rpc_rqst_t *>(_data); }
	const sg_rpc_rqst_t * rpc_rqst() const { return reinterpret_cast<const sg_rpc_rqst_t *>(_data); }
	sg_rpc_rply_t * rpc_rply() { return reinterpret_cast<sg_rpc_rply_t *>(_data); }
	const sg_rpc_rply_t * rpc_rply() const { return reinterpret_cast<const sg_rpc_rply_t *>(_data); }

	operator sg_msg_t&() { return *reinterpret_cast<sg_msg_t *>(_buffer); }
	operator const sg_msg_t&() const { return *reinterpret_cast<const sg_msg_t *>(_buffer); }
	operator sg_msg_t *() { return reinterpret_cast<sg_msg_t *>(_buffer); }
	operator const sg_msg_t *() const { return reinterpret_cast<const sg_msg_t *>(_buffer); }

	operator sg_metahdr_t&() { return reinterpret_cast<sg_msg_t *>(_buffer)->metahdr; }
	operator const sg_metahdr_t&() const { return reinterpret_cast<const sg_msg_t *>(_buffer)->metahdr; }
	operator sg_metahdr_t *() { return &reinterpret_cast<sg_msg_t *>(_buffer)->metahdr; }
	operator const sg_metahdr_t *() const { return &reinterpret_cast<const sg_msg_t *>(_buffer)->metahdr; }

	operator sg_msghdr_t&() { return reinterpret_cast<sg_msg_t *>(_buffer)->msghdr; }
	operator const sg_msghdr_t&() const { return reinterpret_cast<const sg_msg_t *>(_buffer)->msghdr; }
	operator sg_msghdr_t *()  { return &reinterpret_cast<sg_msg_t *>(_buffer)->msghdr; }
	operator const sg_msghdr_t *() const { return &reinterpret_cast<const sg_msg_t *>(_buffer)->msghdr; }

	bool rplywanted() const { return sg_msghdr_t(*this).rplywanted(); }
	bool is_rply() const { return (sg_metahdr_t(*this).mtype & RPLYBASE); }
	bool is_meta() const { return (sg_msghdr_t(*this).flags & SGMETAMSG); }

private:
	sg_message();
	sg_message(const sg_message& rhs);

	size_t _buffer_size;
	char *_buffer;
	size_t _data_size;
	char *_data;
};

ostream& operator<<(ostream& os, const alt_flds_t& flds);
ostream& operator<<(ostream& os, const sg_metahdr_t& metahdr);
ostream& operator<<(ostream& os, const sg_msghdr_t& msghdr);
ostream& operator<<(ostream& os, const sg_message& msg);

class sg_message_deleter
{
public:
	void operator()(sg_message *msg);
};

}
}

#endif

