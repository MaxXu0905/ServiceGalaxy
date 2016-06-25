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

	// 创建SCTE节点，在创建前，service对应的STE已经创建
	sgid_t create(sgid_t ssgid, sg_svcparms_t& parms, int flags);
	sgid_t smcreate(sgid_t ssgid, sg_svcparms_t& parms, int flags);
	sgid_t mbcreate(sgid_t ssgid, sg_svcparms_t& parms, int flags);
	// 删除SCTE节点
	int destroy(sgid_t *sgids, int cnt);
	int gdestroy(sgid_t *sgids, int cnt);
	int smdestroy(sgid_t *sgids, int cnt);
	int mbdestroy(sgid_t *sgids, int cnt);
	// 更新SCTE节点
	int update(sg_scte_t *sctes, int cnt, int flags);
	int gupdate(sg_scte_t *sctes, int cnt, int flags);
	int smupdate(sg_scte_t *sctes, int cnt, int flags);
	int mbupdate(sg_scte_t *sctes, int cnt, int flags);
	// 查找SCTE节点
	int retrieve(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids);
	int smretrieve(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids);
	int mbretrieve(int scope, const sg_scte_t *key, sg_scte_t *sctes, sgid_t *sgids);
	// 增加SCTE节点
	sgid_t add(sgid_t ssgid, sg_svcparms_t& parms);
	sgid_t smadd(sgid_t ssgid, sg_svcparms_t& parms);
	sgid_t mbadd(sgid_t ssgid, sg_svcparms_t& parms);
	// 发布服务
	sgid_t offer(const std::string& svc_name, const std::string& class_name);
	// 获取服务
	bool getsvc(message_pointer& msg, sg_scte_t& scte, int flags);

	// 地址与偏移量间转换
	sg_scte_t * link2ptr(int link);
	sg_scte_t * sgid2ptr(sgid_t sgid);
	int ptr2link(const sg_scte_t *scte) const;
	sgid_t ptr2sgid(const sg_scte_t *scte) const;

	// 获取服务的负载
	long get_load(const sg_scte_t& scte) const;

	// 计算节点的hash值
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

