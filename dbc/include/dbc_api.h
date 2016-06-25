#if !defined(__DBC_API_H__)
#define __DBC_API_H__

#include "sql_control.h"
#include "compiler.h"
#include "dbc_internal.h"

namespace ai
{
namespace sg
{

#if !defined(__DBCT_UPDATE_FUNC_DEFINED__)
#define __DBCT_UPDATE_FUNC_DEFINED__
typedef void (*DBCC_UPDATE_FUNC)(void *result, const void *old_data, const void *new_data);
#endif

class cursor_compare
{
public:
	cursor_compare(char *data_start_, compiler::func_type order_func_);

	bool operator()(long left, long right) const;

private:
	char *data_start;
	compiler::func_type order_func;
};

class dbc_cursor
{
public:
	~dbc_cursor();

	bool next();
	bool prev();
	bool status();
	bool seek(int cursor_);
	const char * data() const;
	long rowid() const;
	long size() const;
	void order(compiler::func_type order_func);

protected:
	dbc_cursor(dbct_ctx *DBCT_, vector<long> *rowids_, dbc_te_t *te_);

private:
	dbct_ctx *DBCT;
	vector<long> *rowids;
	dbc_te_t *te;
	int cursor;
	char *unmarshal_buf;

	friend class dbc_api;
};

class dbc_api : public dbc_manager
{
public:
	static dbc_api& instance();
	static dbc_api& instance(dbct_ctx *DBCT);

	// ��¼��DBC
	bool login();
	// ��DBCע��
	void logout();
	// ����DBC
	bool connect(const string& user_name, const string& password);
	// �Ƿ��Ѿ�����
	bool connected() const;
	// ��DBC�Ͽ�����
	void disconnect();
	// �����û�ID
	bool set_user(int64_t user_id);
	// ��ȡ�û�ID
	int64_t get_user() const;
	// ����ע��ڵ�������
	void set_ctx(dbc_rte_t *rte);

	// ���ݱ�����ȡ��ID
	int get_table(const string& table_name);
	// ���ݱ�������������ȡ��ID
	int get_index(const string& table_name, const string& index_name);
	// ������󷵻�����
	void set_rownum(long rownum);
	// ��ȡ��ǰ���õ���󷵻�����
	long get_rownum() const;
	// ȫ����Ҽ�¼
	long find(int table_id, const void *data, void *result, long max_rows = 1);
	// ���������Ҽ�¼
	long find(int table_id, int index_id, const void *data, void *result, long max_rows = 1);
	// ȫ����α�
	auto_ptr<dbc_cursor> find(int table_id, const void *data);
	// ���������α�
	auto_ptr<dbc_cursor> find(int table_id, int index_id, const void *data);
	// ��rowid���Ҽ�¼
	long find_by_rowid(int table_id, long rowid, void *result, long max_rows = 1);
	// ��rowid���α�
	auto_ptr<dbc_cursor> find_by_rowid(int table_id, long rowid);
	// ȫ��ɾ����¼
	long erase(int table_id, const void *data);
	// ������ɾ����¼
	long erase(int table_id, int index_id, const void *data);
	// ��rowidɾ����¼
	bool erase_by_rowid(int table_id, long rowid);
	// �����¼
	bool insert(int table_id, const void *data);
	// ȫ����¼�¼
	long update(int table_id, const void *old_data, const void *new_data);
	// ���������¼�¼
	long update(int table_id, int index_id, const void *old_data, const void *new_data);
	// ��rowid���¼�¼
	long update_by_rowid(int table_id, long rowid, const void *new_data);
	// ��ȡrowid
	long get_rowid(int seq_no);
	// ��ȡ���еĵ�ǰֵ
	long get_currval(const string& seq_name);
	// ��ȡ���е���һֵ
	long get_nextval(const string& seq_name);
	// Ԥ�ύ
	bool precommit();
	// ���׶��ύ
	bool postcommit();
	// �ύ
	bool commit();
	// �ع�
	bool rollback();
	// ��ǰ��ļ�¼��
	size_t size(int table_id);
	// ��ǰ����ⲿ�ṹ�ֽ���
	int struct_size(int table_id);
	// �����Զ��ύ
	bool set_autocommit(bool autocommit);
	// ��ȡ�Զ��ύ��ʶ
	bool get_autocommit();
	// ���ó�ʱ
	bool set_timeout(long timeout);
	// ��ȡ��ʱ
	long get_timeout();

	// ���º������ڲ�ʹ�ã��ṩSQL��ʽ�Ĳ���
	// ���ø��º���
	void set_update_func(DBCC_UPDATE_FUNC update_func);
	// ������º���
	void clear_update_func();
	// ������������
	void set_where_func(compiler::func_type where_func);
	// �����������
	void clear_where_func();

	// ��ձ�
	bool truncate(const string& table_name);
	// ������
	bool create(dbc_te_t& dbc_te_);
	// ɾ����
	bool drop(const string& table_name);
	// ��������
	bool create_index(const dbc_ie_t& ie_node);
	// ��������
	bool create_sequence(const dbc_se_t& se_node);
	// �������
	bool alter_sequence(dbc_se_t *se);
	// ɾ������
	bool drop_sequence(const string& seq_name);

	// ��ȡ�������
	int get_error_code() const;
	// ��ȡ���ش������
	int get_native_code() const;
	// ��ȡ������Ϣ
	const char * strerror() const;
	// ��ȡ���ش�����Ϣ
	const char * strnative() const;
	// ��ȡ�û��Զ���������
	int get_ur_code() const;
	// �����û��Զ���������
	void set_ur_code(int ur_code);

	// ��ȡpartition_id
	static int get_partition_id(int partition_type, int partitions, const char *key);

private:
	dbc_api();
	virtual ~dbc_api();

	bool erase_by_rowid_internal(int table_id, long rowid);
	bool insert(int table_id, const void *data, long old_rowid);
	void erase_undo(int table_id, long rowid, long redo_sequence);
	void insert_undo(int table_id, long rowid, long redo_sequence);
	void commit_save(const redo_row_t *root, set<redo_prow_t>& row_set);
	bool commit_internal(redo_row_t *root);
	bool rollback_internal(redo_row_t *root);
	bool commit(bool do_save);
	void recover(int table_id, long redo_sequence);
	void clone_data(const char *data);

	static void logout_atexit();
	static int match_myself(void *client, char **global, const char **in, char **out);

	friend class dbc_server;
	friend class dbct_ctx;
};

}
}

#endif

