#if !defined(__SG_SGTE_H__)
#define __SG_SGTE_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

class sg_sgte : public sg_manager
{
public:
	static sg_sgte& instance(sgt_ctx *SGT);

	// 创建SGTE节点
	sgid_t create(sg_sgparms_t& parms);
	sgid_t smcreate(sg_sgparms_t& parms);
	sgid_t mbcreate(sg_sgparms_t& parms);
	// 删除SGTE节点
	int destroy(sgid_t *sgids, int cnt);
	int smdestroy(sgid_t *sgids, int cnt);
	int mbdestroy(sgid_t *sgids, int cnt);
	int nulldestroy(sgid_t *sgids, int cnt);
	// 更新SGTE节点
	int update(sg_sgte_t *sgtes, int cnt, int flags);
	int smupdate(sg_sgte_t *sgtes, int cnt, int flags);
	int mbupdate(sg_sgte_t *sgtes, int cnt, int flags);
	// 查找SGTE节点
	int retrieve(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids);
	int smretrieve(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids);
	int mbretrieve(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids);
	// 创建所有SGTE节点
	bool getsgrps();

	// 地址与偏移量间转换
	sg_sgte_t * link2ptr(int link);
	sg_sgte_t * sgid2ptr(sgid_t sgid);
	int ptr2link(const sg_sgte_t *sgte) const;
	sgid_t ptr2sgid(const sg_sgte_t *sgte) const;

	// 计算节点的hash值
	int hash_value(const char *sgname) const;

private:
	sg_sgte();
	virtual ~sg_sgte();

	friend class sgt_ctx;
};

}
}

#endif

