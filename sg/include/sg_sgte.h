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

	// ����SGTE�ڵ�
	sgid_t create(sg_sgparms_t& parms);
	sgid_t smcreate(sg_sgparms_t& parms);
	sgid_t mbcreate(sg_sgparms_t& parms);
	// ɾ��SGTE�ڵ�
	int destroy(sgid_t *sgids, int cnt);
	int smdestroy(sgid_t *sgids, int cnt);
	int mbdestroy(sgid_t *sgids, int cnt);
	int nulldestroy(sgid_t *sgids, int cnt);
	// ����SGTE�ڵ�
	int update(sg_sgte_t *sgtes, int cnt, int flags);
	int smupdate(sg_sgte_t *sgtes, int cnt, int flags);
	int mbupdate(sg_sgte_t *sgtes, int cnt, int flags);
	// ����SGTE�ڵ�
	int retrieve(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids);
	int smretrieve(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids);
	int mbretrieve(int scope, const sg_sgte_t *key, sg_sgte_t *sgtes, sgid_t *sgids);
	// ��������SGTE�ڵ�
	bool getsgrps();

	// ��ַ��ƫ������ת��
	sg_sgte_t * link2ptr(int link);
	sg_sgte_t * sgid2ptr(sgid_t sgid);
	int ptr2link(const sg_sgte_t *sgte) const;
	sgid_t ptr2sgid(const sg_sgte_t *sgte) const;

	// ����ڵ��hashֵ
	int hash_value(const char *sgname) const;

private:
	sg_sgte();
	virtual ~sg_sgte();

	friend class sgt_ctx;
};

}
}

#endif

