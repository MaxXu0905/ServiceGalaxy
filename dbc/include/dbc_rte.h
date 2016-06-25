#if !defined(__DBC_RTE_H__)
#define __DBC_RTE_H__

#include "dbc_struct.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

// ע��ڵ�
struct dbc_rte_t
{
	short next;			// ��һ�ڵ�
	int accword;		// ��������flags
	int flags;			// ʹ�������ʶ
	int rte_id;			// ע��ڵ�ID
	bool autocommit;	// �Ƿ��Զ��ύ
 	bi::interprocess_mutex mutex;	// ����Դ��ռ��ʱ���ȴ����ź���
	bi::interprocess_condition cond;	// ����Դ��ռ��ʱ���ȴ�������
	pid_t pid;			// �ýڵ�����Ľ���pid
	long timeout;
	long redo_full;		// û�п��нڵ���б�
	long redo_free;		// �п��нڵ���б�
	long redo_root;		// ��������
	int64_t tran_id;	// ����ID
 	time_t rtimestamp;	// ����ʼʱ���

	void reset_tran();
	void set_flags(int flags_);
	void clear_flags(int flags_);
	bool in_flags(int flags_) const;
};

class dbc_rte : public dbc_manager
{
public:
	static dbc_rte& instance(dbct_ctx *DBCT);

	void init();
	void reinit();
	void set_ctx(dbc_rte_t *rte_);
	dbc_rte_t * get_rte();
	dbc_rte_t * base();
	bool get_slot();
	void put_slot();
	bool set_tran();
	void reset_tran();
	void set_flags(int flags);
	void clear_flags(int flags);
	bool in_flags(int flags) const;
	void set_autocommit(bool autocommit);
	bool get_autocommit() const;
	void set_timeout(long timeout);
	long get_timeout() const;

private:
	dbc_rte();
	virtual ~dbc_rte();

	dbc_rte_t *rte;

	friend class dbct_ctx;
};

}
}

#endif

