#if !defined(__SG_STE_H__)
#define __SG_STE_H__

#include "sg_struct.h"
#include "sg_qte.h"
#include "sg_manager.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

class sg_ste : public sg_manager
{
public:
	static sg_ste& instance(sgt_ctx *SGT);

	// ����STE�ڵ㣬�ڴ���ǰ��server��Ӧ��QTE�Ѿ�����
	sgid_t create(sgid_t qsgid, sg_svrparms_t& parms);
	sgid_t smcreate(sgid_t qsgid, sg_svrparms_t& parms);
	sgid_t mbcreate(sgid_t qsgid, sg_svrparms_t& parms);
	// ɾ��STE�ڵ㣬ͬʱɾ����Ӧ��SCTE�ڵ��Լ�����QTE�ڵ㣬
	// ��Ҫʱ����Ҫɾ��QTE�ڵ�
	int destroy(sgid_t *sgids, int cnt);
	int gdestroy(sgid_t *sgids, int cnt);
	int smdestroy(sgid_t *sgids, int cnt);
	int mbdestroy(sgid_t *sgids, int cnt);
	// ����STE�ڵ�
	int update(sg_ste_t *stes, int cnt, int flags);
	int gupdate(sg_ste_t *stes, int cnt, int flags);
	int smupdate(sg_ste_t *stes, int cnt, int flags);
	int mbupdate(sg_ste_t *stes, int cnt, int flags);
	// ����STE�ڵ�
	int retrieve(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids);
	int smretrieve(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids);
	int mbretrieve(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids);
	// ����QTE��STE����SCTE
	sgid_t offer(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt);
	sgid_t smoffer(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt);
	sgid_t mboffer(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt);
	// ɾ��STE
	int remove(sgid_t sgid);
	int smremove(sgid_t sgid);
	int mbremove(sgid_t sgid);

	// ��ַ��ƫ������ת��
	sg_ste_t * link2ptr(int link);
	sg_ste_t * sgid2ptr(sgid_t sgid);
	int ptr2link(const sg_ste_t *ste) const;
	sgid_t ptr2sgid(const sg_ste_t *ste) const;

	// ����ڵ��hashֵ
	int hash_value(int svrid) const;

private:
	sg_ste();
	virtual ~sg_ste();

	int conform(sg_qte_t& qte, sg_svrparms_t& svrparms, boost::shared_array<sg_svcparms_t>& nsvcp, int& cnt);

	friend class sgt_ctx;
};

}
}

#endif

