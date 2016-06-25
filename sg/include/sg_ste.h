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

	// 创建STE节点，在创建前，server对应的QTE已经创建
	sgid_t create(sgid_t qsgid, sg_svrparms_t& parms);
	sgid_t smcreate(sgid_t qsgid, sg_svrparms_t& parms);
	sgid_t mbcreate(sgid_t qsgid, sg_svrparms_t& parms);
	// 删除STE节点，同时删除对应的SCTE节点以及调整QTE节点，
	// 必要时，需要删除QTE节点
	int destroy(sgid_t *sgids, int cnt);
	int gdestroy(sgid_t *sgids, int cnt);
	int smdestroy(sgid_t *sgids, int cnt);
	int mbdestroy(sgid_t *sgids, int cnt);
	// 更新STE节点
	int update(sg_ste_t *stes, int cnt, int flags);
	int gupdate(sg_ste_t *stes, int cnt, int flags);
	int smupdate(sg_ste_t *stes, int cnt, int flags);
	int mbupdate(sg_ste_t *stes, int cnt, int flags);
	// 查找STE节点
	int retrieve(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids);
	int smretrieve(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids);
	int mbretrieve(int scope, const sg_ste_t *key, sg_ste_t *stes, sgid_t *sgids);
	// 创建QTE、STE、和SCTE
	sgid_t offer(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt);
	sgid_t smoffer(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt);
	sgid_t mboffer(sg_svrparms_t& svrparms, sg_svcparms_t *svcparms, int cnt);
	// 删除STE
	int remove(sgid_t sgid);
	int smremove(sgid_t sgid);
	int mbremove(sgid_t sgid);

	// 地址与偏移量间转换
	sg_ste_t * link2ptr(int link);
	sg_ste_t * sgid2ptr(sgid_t sgid);
	int ptr2link(const sg_ste_t *ste) const;
	sgid_t ptr2sgid(const sg_ste_t *ste) const;

	// 计算节点的hash值
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

