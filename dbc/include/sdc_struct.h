#if !defined(__SDC_STRUCT_H__)
#define __SDC_STRUCT_H__

#include <inttypes.h>

namespace ai
{
namespace sg
{

enum sdc_opcode_t
{
	SDC_OPCODE_FIND = 0x1,
	SDC_OPCODE_ERASE = 0x2,
	SDC_OPCODE_INSERT = 0x4,
	SDC_OPCODE_UPDATE = 0x8,
	SDC_OPCODE_CONNECT = 0x10,
	SDC_OPCODE_GET_TABLE = 0x20,
	SDC_OPCODE_GET_INDEX = 0x40,
	SDC_OPCODE_GET_META = 0x80
};

enum sdc_flags_t
{
	SDC_FLAGS_AUTOCOMMIT = 0x1,
	SDC_FLAGS_NOTRAN = 0x2,
};

// 向SDC发起请求的消息包格式
struct sdc_rqst_t
{
	int opcode;
	int datalen;
	int struct_size;
	int max_rows;
	int flags;
	int64_t user_id;
	int table_id;
	int index_id;
	long placeholder;

	const char * data() const;
	char * data();
};

// 从SDC返回应答的消息包格式
struct sdc_rply_t
{
	int datalen;
	int rows;
	int error_code;
	int native_code;
	long placeholder;

	const char * data() const;
	char * data();
};

struct sdc_sql_rqst_t
{
	int64_t user_id;
	int max_rows;
	char placeholder[1];
};

}
}

#endif

