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
int const UE_IN_USE = 0x1;			// ����ʹ��
int const UE_MARK_REMOVED = 0x2;	// ���Ϊɾ��
int const UE_ID_BITS = 16;
int const UE_ID_MASK = (1 << UE_ID_BITS) - 1;

// SYS TE SLOTs
int const TE_MIN_RESERVED = 30000;
int const TE_DUAL = TE_MIN_RESERVED;
int const TE_SYS_INDEX_LOCK = TE_MIN_RESERVED + 1;
int const TE_MAX_RESERVED = TE_MIN_RESERVED + 2;

// TE flags
int const TE_IN_USE = 0x1;			// ����ʹ��
int const TE_IN_MEM = 0x2;			// �ڴ��
int const TE_READONLY = 0x4;		// ֻ��
int const TE_SYS_RESERVED = 0x8;	// ϵͳ����
int const TE_IN_TEMPORARY = 0x10;	// ��ʱ
int const TE_MARK_REMOVED = 0x20;	// ���Ϊɾ��
int const TE_NO_UPDATE = 0x40;		// ���������
int const TE_VARIABLE = 0x80;		// �䳤�ڵ�

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
// ������ر�ʶ
short const ROW_TRAN_INSERT = 0x1;
short const ROW_TRAN_DELETE = 0x2;
short const ROW_TRAN_UPDATE = 0x4;
short const ROW_TRAN_MASK = ROW_TRAN_INSERT | ROW_TRAN_DELETE | ROW_TRAN_UPDATE;
short const ROW_MARK_REMOVED = 0x8;
short const ROW_IN_USE = 0x10;
short const ROW_MASK = ROW_TRAN_MASK | ROW_MARK_REMOVED;
short const ROW_INVISIBLE = 0x20;
short const ROW_VARIABLE = 0x40;

// ��ѯʱ�ı�ʶ
int const FIND_FOR_UPDATE = 0x1;	// Ϊ�˸��²�ѯ
int const FIND_FOR_WAIT = 0x2;		// ��ѯʱ��Ҫ�ȴ�
int const FIND_MASK = FIND_FOR_UPDATE | FIND_FOR_WAIT;

// д���¼����
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

// DBC�������ݽṹ
struct dbc_bbparms_t
{
	int magic;		// magic
	int bbrelease;		// BB��Ʒ��
	long bbversion;	// BB�汾
	long initial_size;	// ��ʼ��С
	long total_size;	// �����ڴ��С
	int ipckey;			// �����ڴ�key
	uid_t uid;			// �û�����
	uid_t gid;			// �����
	int perm;			// ����ģʽ
	int scan_unit;		// ���ٴ�sanity_scan֮����н����Լ��
	int sanity_scan;	// һ�μ���ʱ����
	int stack_size;		// �߳�ջ��С
	int stat_level;		// ͳ�Ƽ���
	char sys_dir[MAX_DIR_LEN + 1];	// ϵͳ�����ļ�Ŀ¼
	char sync_dir[MAX_DIR_LEN + 1];	// ͬ���ļ�Ŀ¼
	char data_dir[MAX_DIR_LEN + 1];	// �����ļ�Ŀ¼
	long shm_size;			// һ�鹲���ڴ��С
	int shm_count;			// �����ڴ����
	int reserve_count;			// �����ڴ����
	int post_signo;			// ״̬���ʱ���͵��ź�
	int maxusers;				// ����û���
	int maxtes;				// �������ı���
	int maxses;				// ��������������
	int maxrtes;				// ��������ע��ڵ���
	int sebkts;				// SE hash table��С
	int segment_redos;			// һ�η���������¼��
	char libs[MAX_LIBS_LEN + 1];	// ��Ҫ���صĶ�̬��
	int proc_type;				// ��������
};

struct dbc_bbmeters_t
{
	bi::interprocess_mutex sqlite_mutex;	// SQLITE������
	bi::interprocess_condition sqlite_cond;	// SQLITE��������
	int sqlite_states[MAX_SQLITES];		// SQLITE�ڵ�״̬
	int64_t tran_id;					// ����ID
	bi::interprocess_mutex load_mutex;	// ������ص���
	bi::interprocess_condition load_cond;	// ������ص�����
	bool load_ready;					// �Ƿ�������
	int curtes;		// ��ǰʹ�õı���
	int currtes;	// ��ǰʹ�õ�ע��ڵ���
	int curmaxte;	// ��ǰʹ�õ������±�
	int additional_count;	// ��Ҫ��չ�Ŀ���
	dbc_stat_t stat_array[TOTAL_STAT];

	dbc_bbmeters_t();
	~dbc_bbmeters_t();
};

struct dbc_bbmap_t
{
	int shmids[MAX_SHMS];	// �����ڴ�shmid����
	long mem_free;			// �����ڴ�λ��
};

struct dbc_bboard_t
{
	dbc_bbparms_t bbparms;	/* static stuff */
	bi::interprocess_recursive_mutex syslock;	// ϵͳ��
	bi::interprocess_recursive_mutex memlock;	// �ڴ������
	bi::interprocess_mutex nomem_mutex;		// �ڴ治����
	bi::interprocess_condition nomem_cond;	// �ڴ治������
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
// �ýṹ���뱣֤��sizeof(long)����
struct row_link_t
{
	short flags;		// ��¼��ʶ
	short ref_count;	// ���ü���
	int slot;			// �ڵ��ڿ����б������
	int extra_size;		// �䳤�ֶε����ֽ���
	int accword;		// ������
	short lock_level;	// �������
	short wait_level;	// �ȴ����
	int rte_id;			// �����Ľڵ�ID
	long prev;			// ָ��ǰһ����¼
	long next;			// ָ���һ����¼

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

int const PARTITION_TYPE_STRING = 0x1;	// �ַ����ͷ���
int const PARTITION_TYPE_NUMBER = 0x2;	// �����ͷ���

int const REFRESH_TYPE_DB = 0x1;			// Refresh using DB table.
int const REFRESH_TYPE_FILE = 0x2;			// Refresh using file.
int const REFRESH_TYPE_BFILE = 0x4;			// Refresh using binary file.
int const REFRESH_TYPE_INCREMENT = 0x8;	// increment refresh
int const REFRESH_TYPE_IN_MEM = 0x10;		// in memory table
int const REFRESH_TYPE_NO_LOAD = 0x20;	// Do not load by now
int const REFRESH_TYPE_AGG = 0x40;			// aggregrate record on conflict
int const REFRESH_TYPE_DISCARD = 0x80;		// discard record on conflict

int const SEARCH_TYPE_EQUAL = 0x1; // ���
int const SEARCH_TYPE_STRCHR = 0x2; // �����ַ����ַ�����
int const SEARCH_TYPE_STRSTR = 0x4; // �����ַ������ַ�����
int const SEARCH_TYPE_WILDCARD = 0x8; // ����ƥ��
int const SEARCH_TYPE_LESS = 0x10; // С��
int const SEARCH_TYPE_MORE = 0x20; // ����
int const SEARCH_TYPE_IGNORE_CASE = 0x40; // ���Դ�Сд
int const SEARCH_TYPE_MATCH_N = 0x80; // �ƥ��

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

int const METHOD_TYPE_SEQ = 0x1;	// ˳�����
int const METHOD_TYPE_BINARY = 0x2;	// ���ֲ���
int const METHOD_TYPE_HASH = 0x4;	// ��ϣ����
int const METHOD_TYPE_STRING = 0x8;	// �ַ����͹�ϣ����
int const METHOD_TYPE_NUMBER = 0x10;	// �����͹�ϣ����

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
	char field_name[FIELD_NAME_SIZE + 1];	// ���ɴ�����ֶ�����
	char fetch_name[FETCH_NAME_SIZE + 1];	// SELECT�ֶ�����
	char update_name[UPDATE_NAME_SIZE + 1];	// INSERT/UPDATE/DELETE�ֶ�����
	char column_name[COLUMN_NAME_SIZE + 1];	// ���е�����
	sqltype_enum field_type;				// �ֶ�����
	int field_size; 							// �ֶγ���
	int field_offset;						// �ֶ��ڽṹ�е�ƫ����
	int internal_offset;						// �ֶ��ڲ�ƫ����
	aggtype_enum agg_type;				// �ۺ�����
	bool is_primary;						// �Ƿ�Ϊ����
	bool is_partition;						// ���ֶ�Ϊpartition�ֶ�
	char partition_format[PARTITION_FORMAT_SIZE + 1];	// PARTITION��ʽ���ֶ�
	int load_size;							// �ֶμ���ʱ�ĳ���
};

struct dbc_data_t
{
	char table_name[TABLE_NAME_SIZE + 1];	// ����
	char dbtable_name[TABLE_NAME_SIZE + 1];	// ���ݿ��еı���
	char conditions[CONDITIONS_SIZE + 1];		// ��������
	char dbname[MAX_DBNAME + 1];			// ���ݿ�����
	char openinfo[MAX_OPENINFO + 1];		// ���ݿ�������Ϣ
	char escape;							// �䳤��ʽ�µ�ת���ַ�
	char delimiter;							// �䳤��ʽ�µ������ļ��ָ���
	long segment_rows;						// һ����չ��¼��
	int refresh_type;						// ˢ������
	int partition_type;						// ��������
	int partitions;							// ������
	int struct_size;						// �ⲿ�ṹ�ֽ���
	int internal_size;						// �ڲ��ṹ�ֽ���
	int extra_size;							// �ڲ��ṹ�����ֽ���
	int field_size;							// �ֶ���
	dbc_data_field_t fields[MAX_FIELDS];		// �ֶνṹ
};

struct dbc_index_field_t
{
	char field_name[FIELD_NAME_SIZE + 1];	// �ֶ�����
	bool is_hash;							// ���ֶ�Ϊhash�ֶ�
	char hash_format[HASH_FORMAT_SIZE + 1]; // HASH��ʽ���ֶ�
	int search_type;						// ��������
	int special_type;						// ��������
	int range_group;						// ��Χ����
	int range_offset;						// ��Χƫ����
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
	long buckets;	// hash�ṹ��һ�����������С
	long segment_rows;	// һ����չ������
	int field_size;
	dbc_index_field_t fields[MAX_FIELDS];
};

// ����������¼
struct dbc_inode_t
{
	long left;	// ǰһ�ڵ�
	long right;	// ��һ�ڵ�
	long parent;
	color_t color;
	long rowid;	// ָ��ļ�¼��ַ

	dbc_inode_t& operator=(const dbc_inode_t& rhs);
};

// ˳��������¼
struct dbc_snode_t
{
	long prev;	// ǰһ�ڵ�
	long next;	// ��һ�ڵ�
	long rowid;	// ָ��ļ�¼��ַ

	dbc_snode_t& operator=(const dbc_snode_t& rhs);
};

typedef long (*COMP_FUNC)(const void *key, const void *data);

struct dbc_func_field_t
{
	string field_name;
	bool nullable; // ���������Ƿ������ݿ�ָ��
	string field_ref; // ����ֶ��Ƿ�������ֶι���
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

