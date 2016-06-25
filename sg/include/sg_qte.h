#if !defined(__SG_QTE_H__)
#define __SG_QTE_H__

#include "sg_struct.h"
#include "sg_message.h"
#include "sg_manager.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

class sg_qte : public sg_manager
{
public:
	static sg_qte& instance(sgt_ctx *SGT);

	// ����QTE�ڵ�
	sgid_t create(sg_queparms_t& parms);
	sgid_t smcreate(sg_queparms_t& parms);
	sgid_t mbcreate(sg_queparms_t& parms);
	// ɾ��QTE�ڵ㣬ͬʱɾ��QTE�ϵ�STEs
	int destroy(sgid_t *sgids, int cnt);
	int gdestroy(sgid_t *sgids, int cnt);
	int smdestroy(sgid_t *sgids, int cnt);
	int mbdestroy(sgid_t *sgids, int cnt);
	// ����QTE�ڵ�
	int update(sg_qte_t *qtes, int cnt, int flags);
	int gupdate(sg_qte_t *qtes, int cnt, int flags);
	int smupdate(sg_qte_t *qtes, int cnt, int flags);
	int mbupdate(sg_qte_t *qtes, int cnt, int flags);
	// ����QTE�ڵ�
	int retrieve(int scope, const sg_qte_t *key, sg_qte_t *qtes, sgid_t *sgids);
	int smretrieve(int scope, const sg_qte_t *key, sg_qte_t *qtes, sgid_t *sgids);
	int mbretrieve(int scope, const sg_qte_t *key, sg_qte_t *qtes, sgid_t *sgids);

	// �������
	bool enqueue(sg_scte_t *scte);
	bool rtenq(sg_scte_t *scte);
	bool rrenq(sg_scte_t *scte);
	bool nullenq(sg_scte_t *scte);
	bool dequeue(sg_scte_t *scte, bool undo);
	bool rtdeq(sg_scte_t *scte, bool undo);
	bool rrdeq(sg_scte_t *scte, bool undo);
	bool nulldeq(sg_scte_t *scte, bool undo);
	int choose(sg_qte_t *curr, sg_qte_t *best, long curr_load, long best_load);
	int realchoose(sg_qte_t *curr, sg_qte_t *best, long curr_load, long best_load);
	int nullchoose(sg_qte_t *curr, sg_qte_t *best, long curr_load, long best_load);
	void syncq();
	void syncqrr();
	void syncqrt();
	void syncqnull();

	// ��ַ��ƫ������ת��
	sg_qte_t * link2ptr(int link);
	sg_qte_t * sgid2ptr(sgid_t sgid);
	int ptr2link(const sg_qte_t *qte) const;
	sgid_t ptr2sgid(const sg_qte_t *qte) const;

	// ����ڵ��hashֵ
	int hash_value(const char *saddr) const;

private:
	sg_qte();
	virtual ~sg_qte();

	bool commenq(sg_scte_t *scte, bool rt);
	bool commdeq(sg_scte_t *scte, bool undo, bool rt);

	friend class sgt_ctx;
};

}
}

#endif

