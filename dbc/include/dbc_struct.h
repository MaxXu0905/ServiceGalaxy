#if !defined(__DBC_STRUCT_H__)
#define __DBC_STRUCT_H__

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include "dbc_lock.h"
#include "sql_base.h"
#include "sg_internal.h"
#include "dbc_stat.h"

namespace bi = boost::interprocess;
using namespace std;
using namespace ai::scci;

namespace ai
{
namespace sg
{

int const COPYRIGHT_SIZE = 127;
int const DBC_MAGIC = 0x873651EA;
int const DBC_RELEASE = 10;
long const DFLT_SHM_SIZE = 1024L * 1024 * 1024;
int const DFLT_SHM_COUNT = 2;
int const DFLT_RESERVE_COUNT = 10;
int const DFLT_MAXUSERS = 100;
int const DFLT_MAXTES = 100;
int const DFLT_MAXSES = 100;
int const DFLT_MAXRTES = 1000;
int const DFLT_SEBKTS = 100;
int const DFLT_SEGMENT_REDOS = 100000;
int const DFLT_BUCKETS = 100000;
int const DFLT_SEGMENT_ROWS = 100000;
size_t const DFLT_PAGES = 65536;
size_t const DFLT_FILE_SIZE = 512L * 1024 * 1024;
int const MAX_DIR_LEN = 1023;
int const MAX_INDEXES = 100;
int const MAX_SEQUENCES = 100;

int const MAX_SHMS = 1000;

int const STRUCT_ALIGN = sizeof(long);
int const MAX_HASH_SIZE = 1023;

int const MAX_DBC_USER_NAME = 127;
int const MAX_DBC_PASSWORD = 127;
int const MAX_DBNAME = 127;
int const MAX_OPENINFO = 1023;
int const MAX_LIBS_LEN = 1023;

// Operation permissions
int const DBC_PERM_INSERT = 0x1;
int const DBC_PERM_UPDATE = 0x2;
int const DBC_PERM_DELETE = 0x4;
int const DBC_PERM_SELECT = 0x8;
int const DBC_PERM_CREATE = 0x10;
int const DBC_PERM_DROP = 0x20;
int const DBC_PERM_ALTER = 0x40;
int const DBC_PERM_TRUNCATE = 0x80;
int const DBC_PERM_RELOAD = 0x100;
int const DBC_PERM_SYSTEM = 0x1FF;
int const DBC_PERM_READONLY = DBC_PERM_SELECT | DBC_PERM_SYSTEM;

int const MAX_DBC_ENTITIES = (1 << 16);

// MEM BLOCK flags
int const DBC_MEM_MAGIC = 0x837475ED;
int const MEM_IN_USE = 0x1;

// UE flags
int const UE_IN_USE = 0x1;			// 正在使用
int const UE_MARK_REMOVED = 0x2;	// 标记为删除
int const UE_ID_BITS = 16;
int const UE_ID_MASK = (1 << UE_ID_BITS) - 1;

// SYS TE SLOTs
int const TE_MIN_RESERVED = 30000;
int const TE_DUAL = TE_MIN_RESERVED;
int const TE_SYS_INDEX_LOCK = TE_MIN_RESERVED + 1;
int const TE_MAX_RESERVED = TE_MIN_RESERVED + 2;

// TE flags
int const TE_IN_USE = 0x1;			// 正在使用
int const TE_IN_MEM = 0x2;			// 内存表
int const TE_READONLY = 0x4;		// 只读
int const TE_SYS_RESERVED = 0x8;	// 系统保留
int const TE_IN_TEMPORARY = 0x10;	// 临时
int const TE_MARK_REMOVED = 0x20;	// 标记为删除
int const TE_NO_UPDATE = 0x40;		// 不允许更新
int const TE_VARIABLE = 0x80;		// 变长节点

// SE flags
int const SE_IN_USE = 0x1;

// Administrator RTE SLOTs
int const RTE_SLOT_SERVER = 0;
int const RTE_SLOT_SYNC = 1;
int const RTE_SLOT_ADMIN = 2;
int const RTE_SLOT_MAX_RESERVED = 3;

// RTE flags
int const RTE_IN_USE = 0x1;
int const RTE_MARK_REMOVED = 0x02;
int const RTE_REATTACH = 0x04;
int const RTE_IN_TRANS = 0x100;
int const RTE_IN_SUSPEND = 0x200;
int const RTE_ABORT_ONLY = 0x400;
int const RTE_IN_PRECOMMIT = 0x800;
int const RTE_TRAN_MASK = 0xF00;

// ROW flags
// 事务相关标识
short const ROW_TRAN_INSERT = 0x1;
short const ROW_TRAN_DELETE = 0x2;
short const ROW_TRAN_UPDATE = 0x4;
short const ROW_TRAN_MASK = ROW_TRAN_INSERT | ROW_TRAN_DELETE | ROW_TRAN_UPDATE;
short const ROW_MARK_REMOVED = 0x8;
short const ROW_IN_USE = 0x10;
short const ROW_MASK = ROW_TRAN_MASK | ROW_MARK_REMOVED;
short const ROW_INVISIBLE = 0x20;
short const ROW_VARIABLE = 0x40;

// 查询时的标识
int const FIND_FOR_UPDATE = 0x1;	// 为了更新查询
int const FIND_FOR_WAIT = 0x2;		// 查询时需要等待
int const FIND_MASK = FIND_FOR_UPDATE | FIND_FOR_WAIT;

// 写入记录类型
int const UPDATE_TYPE_INSERT = 0x1;
int const UPDATE_TYPE_UPDATE = 0x2;
int const UPDATE_TYPE_DELETE = 0x4;

// Global index passed as global array.
int const GI_ROW_DATA = 0;
int const GI_DATA_START = 1;
int const GI_TIMESTAMP = 2;
int const GI_ROW_DATA2 = 3;
int const MAX_GI = 4;

typedef long thash_t;
typedef long ihash_t;
typedef long shash_t;

int const COMMIT_STAT = 0;
int const ROLLBACK_STAT = 1;
int const PRECOMMIT_STAT = 2;
int const POSTCOMMIT_STAT = 3;
int const TOTAL_STAT = 4;

class dbcp_ctx;
class dbct_ctx;
class dbc_bbparms_t;
class dbc_data_t;
class dbc_data_field_t;
class data_index_t;
class dbc_rte_t;
class dbc_te_t;
class dbc_ie_t;
class dbc_se_t;
class redo_row_t;
class redo_prow_t;

extern const char *STAT_NAMES[TOTAL_STAT];

struct dbc_mem_block_t
{
	int magic;
	int flags;
	size_t size;	// data buf size, excluding mem block header
	long prev;
	long next;
};

int const SQLITE_IN_USE = 0x1;
int const SQLITE_IN_PRECOMMIT = 0x2;
int const MAX_SQLITES = 16;

int const DBC_STANDALONE = 0x1;
int const DBC_MASTER = 0x2;
int const DBC_SLAVE = 0x4;

// DBC参数数据结构
struct dbc_bbparms_t
{
	int magic;		// magic
	int bbrelease;		// BB产品号
	long bbversion;	// BB版本
	long initial_size;	// 初始大小
	long total_size;	// 整个内存大小
	int ipckey;			// 共享内存key
	uid_t uid;			// 用户归属
	uid_t gid;			// 组归属
	int perm;			// 访问模式
	int scan_unit;		// 多少次sanity_scan之后进行健康性检查
	int sanity_scan;	// 一次检查的时间间隔
	int stack_size;		// 线程栈大小
	int stat_level;		// 统计级别
	char sys_dir[MAX_DIR_LEN + 1];	// 系统定义文件目录
	char sync_dir[MAX_DIR_LEN + 1];	// 同步文件目录
	char data_dir[MAX_DIR_LEN + 1];	// 数据文件目录
	long shm_size;			// 一块共享内存大小
	int shm_count;			// 共享内存块数
	int reserve_count;			// 虚拟内存块数
	int post_signo;			// 状态变更时发送的信号
	int maxusers;				// 最大用户数
	int maxtes;				// 最大允许的表数
	int maxses;				// 最大允许的序列数
	int maxrtes;				// 最大允许的注册节点数
	int sebkts;				// SE hash table大小
	int segment_redos;			// 一次分配重做记录数
	char libs[MAX_LIBS_LEN + 1];	// 需要加载的动态库
	int proc_type;				// 启动类型
};

struct dbc_bbmeters_t
{
	bi::interprocess_mutex sqlite_mutex;	// SQLITE访问锁
	bi::interprocess_condition sqlite_cond;	// SQLITE访问条件
	int sqlite_states[MAX_SQLITES];		// SQLITE节点状态
	int64_t tran_id;					// 事务ID
	bi::interprocess_mutex load_mutex;	// 加载相关的锁
	bi::interprocess_condition load_cond;	// 加载相关的条件
	bool load_ready;					// 是否加载完毕
	int curtes;		// 当前使用的表数
	int currtes;	// 当前使用的注册节点数
	int curmaxte;	// 当前使用的最大表下标
	int additional_count;	// 需要扩展的块数
	dbc_stat_t stat_array[TOTAL_STAT];

	dbc_bbmeters_t();
	~dbc_bbmeters_t();
};

struct dbc_bbmap_t
{
	int shmids[MAX_SHMS];	// 共享内存shmid数组
	long mem_free;			// 空闲内存位置
};

struct dbc_bboard_t
{
	dbc_bbparms_t bbparms;	/* static stuff */
	bi::interprocess_recursive_mutex syslock;	// 系统锁
	bi::interprocess_recursive_mutex memlock;	// 内存分配锁
	bi::interprocess_mutex nomem_mutex;		// 内存不足锁
	bi::interprocess_condition nomem_cond;	// 内存不足条件
	dbc_bbmeters_t bbmeters;	/* occupancy stats */
	dbc_bbmap_t bbmap;		/* navigation map */

	void init(const dbc_bbparms_t& config_bbparms);
	long alloc(size_t size);
	void free(long data_offset);
	dbc_mem_block_t * long2block(long offset);
	long block2long(const dbc_mem_block_t *mem_block);

private:
	void free_internal(long data_offset);
};

enum color_t
{
	RED = 0,
	BLACK = 1
};

// Additional row struct for internal use.
// 该结构必须保证按sizeof(long)对齐
struct row_link_t
{
	short flags;		// 记录标识
	short ref_count;	// 引用计数
	int slot;			// 节点在空闲列表的索引
	int extra_size;		// 变长字段的总字节数
	int accword;		// 自旋锁
	short lock_level;	// 加锁层次
	short wait_level;	// 等待层次
	int rte_id;			// 加锁的节点ID
	long prev;			// 指向前一个记录
	long next;			// 指向后一个记录

 	short inc_ref();
	short dec_ref();

	bool lock(dbct_ctx *DBCT);
	void unlock(dbct_ctx *DBCT);

	char * data() { return reinterpret_cast<char *>(this + 1); }
};

int const SYS_ROW_SIZE = sizeof(row_link_t);

struct row_bucket_t
{
	dbc_upgradable_mutex_t mutex;
	long root;

	row_bucket_t();
	~row_bucket_t();
};

int const PARTITION_TYPE_STRING = 0x1;	// 字符串型分区
int const PARTITION_TYPE_NUMBER = 0x2;	// 数字型分区

int const REFRESH_TYPE_DB = 0x1;			// Refresh using DB table.
int const REFRESH_TYPE_FILE = 0x2;			// Refresh using file.
int const REFRESH_TYPE_BFILE = 0x4;			// Refresh using binary file.
int const REFRESH_TYPE_INCREMENT = 0x8;	// increment refresh
int const REFRESH_TYPE_IN_MEM = 0x10;		// in memory table
int const REFRESH_TYPE_NO_LOAD = 0x20;	// Do not load by now
int const REFRESH_TYPE_AGG = 0x40;			// aggregrate record on conflict
int const REFRESH_TYPE_DISCARD = 0x80;		// discard record on conflict

int const SEARCH_TYPE_EQUAL = 0x1; // 相等
int const SEARCH_TYPE_STRCHR = 0x2; // 查找字符在字符串中
int const SEARCH_TYPE_STRSTR = 0x4; // 查找字符串在字符串中
int const SEARCH_TYPE_WILDCARD = 0x8; // 任意匹配
int const SEARCH_TYPE_LESS = 0x10; // 小于
int const SEARCH_TYPE_MORE = 0x20; // 大于
int const SEARCH_TYPE_IGNORE_CASE = 0x40; // 忽略大小写
int const SEARCH_TYPE_MATCH_N = 0x80; // 最长匹配

int const SPECIAL_TYPE_1 = 1;
int const SPECIAL_TYPE_2 = 2;
int const SPECIAL_TYPE_3 = 3;
int const SPECIAL_TYPE_4 = 4;
int const SPECIAL_TYPE_5 = 5;
int const SPECIAL_TYPE_999 = 999;

int const INDEX_TYPE_PRIMARY = 0x1;
int const INDEX_TYPE_UNIQUE = 0x2;
int const INDEX_TYPE_NORMAL = 0x4;
int const INDEX_TYPE_RANGE = 0x8;

int const METHOD_TYPE_SEQ = 0x1;	// 顺序查找
int const METHOD_TYPE_BINARY = 0x2;	// 二分查找
int const METHOD_TYPE_HASH = 0x4;	// 哈希查找
int const METHOD_TYPE_STRING = 0x8;	// 字符串型哈希查找
int const METHOD_TYPE_NUMBER = 0x10;	// 数字型哈希查找

int const TABLE_NAME_SIZE = 127;
int const INDEX_NAME_SIZE = 127;
int const SEQUENCE_NAME_SIZE = 127;
int const FIELD_NAME_SIZE = 31;
int const FETCH_NAME_SIZE = 255;
int const UPDATE_NAME_SIZE = 255;
int const COLUMN_NAME_SIZE = 31;
int const MAX_FIELDS = 100;
int const HASH_FORMAT_SIZE = 255;
int const PARTITION_FORMAT_SIZE = 255;
int const CONDITIONS_SIZE = 1023;
int const SERACH_TYPE_SIZE = 255;

/*
 * tperrno values - error codes
 * The man pages explain the context in which the following error codes
 * can return.
 */
int const DBCMINVAL = 0;			/* minimum error message */
int const DBCEABORT = 1;
int const DBCEBADDESC = 2;
int const DBCEBLOCK = 3;
int const DBCEINVAL = 4;
int const DBCELIMIT = 5;
int const DBCENOENT = 6;
int const DBCEOS = 7;
int const DBCEPERM = 8;
int const DBCEPROTO = 9;
int const DBCESYSTEM = 10;
int const DBCETIME = 11;
int const DBCETRAN = 12;
int const DBCGOTSIG = 13;
int const DBCERMERR = 14;
int const DBCERELEASE = 15;
int const DBCEMATCH = 16;
int const DBCEDIAGNOSTIC = 17;
int const DBCESVRSTAT = 18;		/* bad server status */
int const DBCEBBINSANE = 19;	/* BB is insane */
int const DBCENOBRIDGE = 20;	/* no BRIDGE is available */
int const DBCEDUPENT = 21;		/* duplicate table entry */
int const DBCECHILD = 22;		/* child start problem */
int const DBCENOMSG = 23;		/* no message */
int const DBCENOSERVER = 24;	/* no SERVER */
int const DBCENOSWITCH = 25;
int const DBCEIMAGEINSAME = 26;
int const DBCESEQNOTDEFINE = 27;	/* sequence is not defined in session yet. */
int const DBCENOSEQ = 28;		/* sequence doesn't exist. */
int const DBCESEQOVERFLOW = 29;	/* sequence overflow. */
int const DBCEREADONLY = 30;	/* Table is readonly. */
int const DBCMAXVAL = 31;		/* maximum error message */

enum aggtype_enum
{
	AGGTYPE_NONE = 0,
	AGGTYPE_SUM = 1,
	AGGTYPE_MIN = 2,
	AGGTYPE_MAX = 3
};

struct dbc_data_field_t
{
	char field_name[FIELD_NAME_SIZE + 1];	// 生成代码的字段名称
	char fetch_name[FETCH_NAME_SIZE + 1];	// SELECT字段名称
	char update_name[UPDATE_NAME_SIZE + 1];	// INSERT/UPDATE/DELETE字段名称
	char column_name[COLUMN_NAME_SIZE + 1];	// 表中的列名
	sqltype_enum field_type;				// 字段类型
	int field_size; 							// 字段长度
	int field_offset;						// 字段在结构中的偏移量
	int internal_offset;						// 字段内部偏移量
	aggtype_enum agg_type;				// 聚合类型
	bool is_primary;						// 是否为主键
	bool is_partition;						// 该字段为partition字段
	char partition_format[PARTITION_FORMAT_SIZE + 1];	// PARTITION格式化字段
	int load_size;							// 字段加载时的长度
};

struct dbc_data_t
{
	char table_name[TABLE_NAME_SIZE + 1];	// 表名
	char dbtable_name[TABLE_NAME_SIZE + 1];	// 数据库中的表名
	char conditions[CONDITIONS_SIZE + 1];		// 加载条件
	char dbname[MAX_DBNAME + 1];			// 数据库名称
	char openinfo[MAX_OPENINFO + 1];		// 数据库连接信息
	char escape;							// 变长方式下的转义字符
	char delimiter;							// 变长方式下的输入文件分隔符
	long segment_rows;						// 一次扩展记录数
	int refresh_type;						// 刷新类型
	int partition_type;						// 分区类型
	int partitions;							// 分区数
	int struct_size;						// 外部结构字节数
	int internal_size;						// 内部结构字节数
	int extra_size;							// 内部结构额外字节数
	int field_size;							// 字段数
	dbc_data_field_t fields[MAX_FIELDS];		// 字段结构
};

struct dbc_index_field_t
{
	char field_name[FIELD_NAME_SIZE + 1];	// 字段名称
	bool is_hash;							// 该字段为hash字段
	char hash_format[HASH_FORMAT_SIZE + 1]; // HASH格式化字段
	int search_type;						// 查找类型
	int special_type;						// 特殊类型
	int range_group;						// 范围分组
	int range_offset;						// 范围偏移量
};

struct dbc_index_key_t
{
	int table_id;
	int index_id;

	bool operator<(const dbc_index_key_t& rhs) const;
};

struct dbc_index_t
{
	char index_name[INDEX_NAME_SIZE + 1];
	int index_type;
	int method_type;
	long buckets;	// hash结构的一级索引数组大小
	long segment_rows;	// 一次扩展索引数
	int field_size;
	dbc_index_field_t fields[MAX_FIELDS];
};

// 二分索引记录
struct dbc_inode_t
{
	long left;	// 前一节点
	long right;	// 后一节点
	long parent;
	color_t color;
	long rowid;	// 指向的记录地址

	dbc_inode_t& operator=(const dbc_inode_t& rhs);
};

// 顺序索引记录
struct dbc_snode_t
{
	long prev;	// 前一节点
	long next;	// 后一节点
	long rowid;	// 指向的记录地址

	dbc_snode_t& operator=(const dbc_snode_t& rhs);
};

typedef long (*COMP_FUNC)(const void *key, const void *data);

struct dbc_func_field_t
{
	string field_name;
	bool nullable; // 输入或输出是否允许传递空指针
	string field_ref; // 输出字段是否跟输入字段公用
};

struct dbc_func_t
{
	int field_size;
	dbc_func_field_t fields[MAX_FIELDS];
};

struct dbc_index_switch_t
{
	dbc_index_switch_t()
	{
		memset(this, 0, sizeof(this));
	}

	~dbc_index_switch_t()
	{
	}

	void (*get_hash)(char *key, const void *data);
	long (*compare)(const void *key, const void *data);
	long (*compare_exact)(const void *key, const void *data);
};

struct dbc_switch_t
{
	int max_index;
	int struct_size;
	int internal_size;
	int extra_size;
	long (*equal)(const void *key, const void *data);
	void (*get_partition)(char *key, const void *data);
	void (*print)(const void *data);
	void (*read_key)(FILE *fp, void *data);
	void (*write_key)(FILE *fp, const void *data);
	dbc_index_switch_t *index_switches;
};

class space_predicate
{
public:
	bool operator()(char ch) {
		return ::isspace(ch);
	}
};

class toupper_predicate
{
public:
	char operator()(char ch) {
		return static_cast<char>(::toupper(ch));
	}
};

class tolower_predicate
{
public:
	char operator()(char ch) {
		return static_cast<char>(::tolower(ch));
	}
};

}
}

#endif

