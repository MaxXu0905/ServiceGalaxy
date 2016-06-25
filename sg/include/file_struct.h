#if !defined(__FILE_STRUCT_H__)
#define __FILE_STRUCT_H__

#include <inttypes.h>

namespace ai
{
namespace sg
{

const char FMNGR_GSVC[] = "FMNGR_GSVC";
const char FMNGR_PSVC[] = "FMNGR_PSVC";
const char FMNGR_OSVC[] = "FMNGR_OSVC";

int const FILE_OPCODE_DIR = 1;
int const FILE_OPCODE_UNLINK = 2;
int const FILE_OPCODE_MKDIR = 3;
int const FILE_OPCODE_RENAME = 4;
int const FILE_OPCODE_GET = 5;
int const FILE_OPCODE_PUT = 6;
int const FILE_OPCODE_MIN = FILE_OPCODE_DIR;
int const FILE_OPCODE_MAX = FILE_OPCODE_PUT;

// 向FMNGR发起请求的消息包格式
struct file_rqst_t
{
	int opcode;
	int datalen;
	long placeholder;

	const char * data() const;
	char * data();
};

// 从FMNGR返回应答的消息包格式
struct file_rply_t
{
	int datalen;
	int error_code;
	int native_code;
	long placeholder;

	const char * data() const;
	char * data();
};

}
}

#endif

