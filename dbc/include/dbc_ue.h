#if !defined(__DBC_UE_H__)
#define __DBC_UE_H__

#include "dbc_struct.h"
#include "dbc_stat.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

class dbc_user_t;

// �û�����
struct dbc_ue_t
{
	char user_name[MAX_DBC_USER_NAME + 1];
	char password[MAX_DBC_PASSWORD + 1];
	char dbname[MAX_DBNAME + 1];		// ���ݿ�����
	char openinfo[MAX_OPENINFO + 1];	// ���ݿ����Ӳ���
	int64_t user_id;// �û�ID
	int perm;		// �û�Ȩ��
	int flags;		// �ڲ���ʶ
	time_t ctime;	// ����ʱ��
	time_t mtime;	// ����ʱ��
	long thash[MAX_DBC_ENTITIES];
	long shash[MAX_DBC_ENTITIES];

	void set_flags(int flags_);
	void clear_flags(int flags_);
	bool in_flags(int flags_) const;
	bool in_use() const;
	int get_user_idx() const;
};

class dbc_ue : public dbc_manager
{
public:
	static dbc_ue& instance(dbct_ctx *DBCT);

	// ��ȡ��ǰ��UE�ṹ
	dbc_ue_t * get_ue();
	// �л��û�������
	void set_ctx(int user_idx);
	void set_ctx(dbc_ue_t *ue_);
	// ��¼�û�
	bool connect(const std::string& user_name, const std::string& password);
	// ע���û�
	void disconnect();
	// ��ȡ����״̬
	bool connected() const;
	// ��ȡ��ǰ��TE�ṹ
	dbc_te_t * get_table(int table_id);
	// ��ȡ��ǰ��SE�ṹ
	dbc_se_t * get_seq(const std::string& seq_name);
	// �����û����ɹ��򷵻��û�ID��ʧ�ܷ���-1
	int64_t create_user(const dbc_user_t& user);

private:
	dbc_ue();
	virtual ~dbc_ue();

	dbc_ue_t *ue;	// ָ�����ڴ��е��û�����ڵ�

	friend class dbct_ctx;
};

}
}

#endif

