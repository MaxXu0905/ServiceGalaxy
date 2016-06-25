#if !defined(__SG_SCTE_H__)
#define __SG_SCTE_H__

#include "sg_struct.h"
#include "sg_manager.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

struct sg_cache_t
{
	int svc_policy;
	long load_limit;
	int local_index;
	int remote_index;
	vector<sg_scte_t *> local_sctes;
	vector<sg_scte_t *> remote_sctes;
};

class sg_scte : public sg_manager
{
public:
	static sg_scte& instance(sgt_ctx *SGT);

	// ����SCTE�ڵ㣬�ڴ���ǰ��service��Ӧ��STE�Ѿ�����
	sgid_t create(sgid_t ssgid, sg_svcparms_t& parms, int flags);
	sgid_t smcreate(sgid_t ssgid, sg_svcparms_t& parms, int flags);
	sgid_t mbcreate(sgid_t ssgid, sg_svcparms_t& parms, int flags);
	// ɾ��SCTE�ڵ�
	int destroy(sgid_t *sgids, int cnt);
	int gdestroy(sgid_t *sgids, int cnt);
	int smdestroy(sgid_t *sgids, int cnt);
	int mbdestroy(sgid_t *sgids, int cnt);
	// ����SCTE�ڵ�
	int update(sg_scte_t *sctes, int cnt, int flags);
	int gupdate(sg_scte_t *sctes, int cnt, int flags);
	int smupdate(sg_scte_t *sctes, int cnt, int flags);
	int mbupdate(sg_scte_t *sctes, int cnt, int flags);
	// ����SCTE�ڵ�
	int retrieve(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids);
	int smretrieve(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids);
	int mbretrieve(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids);
	// ����SCTE�ڵ�
	sgid_t add(sgid_t ssgid, sg_svcparms_t& parms);
	sgid_t smadd(sgid_t ssgid, sg_svcparms_t& parms);
	sgid_t mbadd(sgid_t ssgid, sg_svcparms_t& parms);
	// ��������
	sgid_t offer(const std::string& svc_name, const std::string& class_name);
	// ��ȡ����
	bool getsvc(message_pointer& msg, sg_scte_t& scte, int flags);

	// ��ַ��ƫ������ת��
	sg_scte_t * link2ptr(int link);
	sg_scte_t * sgid2ptr(sgid_t sgid);
	int ptr2link(const sg_scte_t *scte) const;
	sgid_t ptr2sgid(const sg_scte_t *scte) const;

	// ��ȡ����ĸ���
	long get_load(const sg_scte_t& scte) const;

	// ����ڵ��hashֵ
	int hash_value(const char *svc_name) const;

private:
	sg_scte();
	virtual ~sg_scte();

	typedef boost::unordered_map<string, sg_cache_t>::iterator cache_iterator;

	int bbversion;
	boost::unordered_map<string, sg_cache_t> cache_map;

	friend class sgt_ctx;
};

}
}

#endif

