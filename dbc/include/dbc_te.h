#if !defined(__DBC_TE_H__)
#define __DBC_TE_H__

#include "dbc_struct.h"
#include "dbc_stat.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

int const TE_FIND_STAT = 0;
int const TE_FIND_ROWID_STAT = 1;
int const TE_INSERT_STAT = 2;
int const TE_UPDATE_STAT = 3;
int const TE_UPDATE_ROWID_STAT = 4;
int const TE_ERASE_STAT = 5;
int const TE_ERASE_ROWID_STAT = 6;
int const TE_TOTAL_STAT = 7;

extern const char *TE_STAT_NAMES[TE_TOTAL_STAT];

// �������ı䳤�ֽ���ΪMAX_VSLOTS * sizeof(long)
int const MAX_VSLOTS = 8192;

// ����
struct dbc_te_t
{
	int64_t user_id;// �û�ID
	int table_id;	// ��ID
	int version;	// ��汾����dbc_server����ʱ����
	int flags;		// �ڲ���ʶ
	int max_index;	// ��ǰ�������
	long cur_count;	// ��ǰ��¼��
	/*
	 * ���д��
	 * ����Ҫ�޸Ķ���ʱ(����ɾ��������������ɾ��������)����Ҫ��д��
	 * �Ա������ݵĲ�ѯ�����Ĳ�������Ҫ�Ӷ���
	 * ���ڴ�����������ʱ��Ҫ����
	 */
	bi::interprocess_upgradable_mutex rwlock;	// ��д��
	dbc_data_t te_meta;						// Ԫ���ݶ���
	long mem_used;							// ʹ�õ��ڴ���׵�ַ
	long row_used;							// ʹ�ýڵ�ƫ�������������������ʼλ��
	bi::interprocess_mutex mutex_free[MAX_VSLOTS];	// ���нڵ���
	bi::interprocess_mutex mutex_ref;				// ���ýڵ���
	long row_free[MAX_VSLOTS];				// ���нڵ�ƫ�������������������ʼλ��
	long row_ref;							// ���ýڵ�ƫ��������������ݿ�ʼλ��
	ihash_t index_hash[MAX_INDEXES];		// ������������ṹ
	dbc_stat_t stat_array[TE_TOTAL_STAT];	// ͳ������

	dbc_te_t();
	dbc_te_t(const dbc_te_t& rhs);
	dbc_te_t& operator=(const dbc_te_t& rhs);

	void set_flags(int flags_);
	void clear_flags(int flags_);
	bool in_flags(int flags_) const;
	bool in_persist() const;
	bool in_use() const;
	bool in_active() const;
};

class dbc_te : public dbc_manager
{
public:
	static dbc_te& instance(dbct_ctx *DBCT);

	bool init(dbc_te_t *te, const dbc_te_t& data, const vector<dbc_ie_t>& index_vector);
	// ��ʼ��������
	void init_rows(long offset, int row_size, int slot, int extra_size);
	// �л�������������
	void set_ctx(dbc_te_t *te_);
	// ��ȡ��ǰ��TE�ṹ
	dbc_te_t * get_te();
	// ���ݱ�������ȡ��TE�ṹ
	dbc_te_t * find_te(const string& table_name);
	// ����������ȡһ�����нڵ㣬��������������
	long create_row(int extra_size);
	// ����������ȡһ�����нڵ㣬���������󣬼�������������
	long create_row(const void *data);
	// �ͷ�һ���ڵ㵽��������
	bool erase_row(long rowid);
	// ���������������ҷ��������ļ�¼
	bool find_row(const void *data);
	// ����rowid����ȡ���������ļ�¼
	bool find_row(long rowid);
	// ���Ѵ��ڵĽڵ��������ü���
	void ref_row(long rowid);
	// ���Ѵ��ڵĽڵ�������ü���
	void unref_row(long rowid);
	// ���Ѿ����ڵĽڵ����
	bool lock_row(long rowid);
	bool lock_row(row_link_t *link);
	// ���Ѿ����ڵĽڵ����
	void unlock_row(long rowid);
	void unlock_row(row_link_t *link);
	// ����rowid��Ӧ���ݼ�¼����������
	bool create_row_indexes(long rowid);
	// ɾ��rowid��Ӧ���ݼ�¼����������
	void erase_row_indexes(long rowid);
	dbc_ie_t * get_index(int index_id);
	void mark_ref();
	void revoke_ref();
	void free_mem();
	bool check_rowid(long rowid);
	int get_vsize(const void *data);
	const char * marshal(const void *src);
	void marshal(void *dst, const void *src);
	void unmarshal(void *dst, const void *src);
	int primary_key();

private:
	dbc_te();
	virtual ~dbc_te();

	row_link_t * get_row_link(int extra_size, int flags);
	void put_row_link(row_link_t *link, long& head, bi::interprocess_mutex& mutex);
	dbc_ie_t * get_index_internal(int index_id);

	dbc_te_t *te;	// ָ�����ڴ��еı���ڵ�

	friend class dbct_ctx;
};

}
}

#endif

