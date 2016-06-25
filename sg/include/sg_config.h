#if !defined(__SG_CONFIG_H__)
#define __SG_CONFIG_H__

#include "cfg_iterator.h"
#include "sg_manager.h"
#include "sg_struct.h"

namespace ai
{
namespace sg
{

int const CFG_PAGE_SIZE = 8192;

struct sg_cfg_entry_t
{
	int per_pages;		// 一次分配需要多少页
	int per_entries;	// 一次分配能容纳多少节点
	int data_size;		// 数据字节数
	int entry_size;		// 节点字节数
	int pages;			// 总共需要多少页
	int entries;
	int entry_use;
	int entry_free;
};

struct sg_cfg_map_t
{
	int pages;
	int bbp_use;
	sg_cfg_entry_t pe_info;
	sg_cfg_entry_t node_info;
	sg_cfg_entry_t sgt_info;
	sg_cfg_entry_t svr_info;
	sg_cfg_entry_t svc_info;
};

class sg_config
{
public:
	typedef cfg_iterator<sg_mchent_t> mch_iterator;
	typedef cfg_iterator<sg_netent_t> net_iterator;
	typedef cfg_iterator<sg_sgent_t> sgt_iterator;
	typedef cfg_iterator<sg_svrent_t> svr_iterator;
	typedef cfg_iterator<sg_svcent_t> svc_iterator;

	static sg_config& instance(sgt_ctx *SGT, bool auto_open = true);

	bool open(const std::string& sgconfig_ = "");
	void close();
	bool reopen(const std::string& sgconfig_ = "");
	bool init(int pes, int nodes, int sgt, int svrs, int svcs, int perm);
	bool flush();

	sg_bbparms_t& get_bbparms();
	mch_iterator mch_begin();
	mch_iterator mch_end();
	net_iterator net_begin();
	net_iterator net_end();
	sgt_iterator sgt_begin();
	sgt_iterator sgt_end();
	svr_iterator svr_begin();
	svr_iterator svr_end();
	svc_iterator svc_begin();
	svc_iterator svc_end();

	sg_mchent_t * find_by_lmid(const string& lmid);
	bool find(sg_mchent_t& mchent);
	bool find(sg_netent_t& netent);
	bool find(sg_sgent_t& sgent);
	bool find(sg_svrent_t& svrent);
	bool find(sg_svcparms_t& svcparms, int grpid);

	bool insert(const sg_mchent_t& mchent);
	bool insert(const sg_netent_t& netent);
	bool insert(const sg_sgent_t& sgent);
	bool insert(const sg_svrent_t& svrent);
	bool insert(const sg_svcent_t& svcent);

	bool erase(const sg_mchent_t& mchent);
	bool erase(const sg_netent_t& netent);
	bool erase(const sg_sgent_t& sgent);
	bool erase(const sg_svrent_t& svrent);
	bool erase(const sg_svcent_t& svcent);

	template<typename T>
	bool update(const T& ent)
	{
		typedef cfg_iterator<T> ent_iterator;

		ent_iterator iter = find_internal(ent);
		if (iter == ent_iterator())
			return false;

		*iter = ent;
		return true;
	}

	bool propagate(const sg_mchent_t& mchent);

private:
	sg_config();
	virtual ~sg_config();

	char * get_entry(sg_cfg_entry_t& cfg_entry);
	void put_entry(sg_cfg_entry_t& cfg_entry, void *data);
	void get_page_info(size_t data_size, int entries, sg_cfg_entry_t& cfg_entry);
	void init_pages(sg_cfg_entry_t& cfg_entry, int page_offset);
	void alloc_pages(sg_cfg_entry_t& cfg_entry);

	mch_iterator find_internal(const sg_mchent_t& mchent);
	net_iterator find_internal(const sg_netent_t& netent);
	sgt_iterator find_internal(const sg_sgent_t& sgent);
	svr_iterator find_internal(const sg_svrent_t& svrent);
	svc_iterator find_internal(const sg_svcent_t& svcent);
	svc_iterator find_internal(const sg_svcparms_t& svcparms, int grpid);

	template<typename T>
	bool insert_internal(const T& ent, sg_cfg_entry_t& cfg_entry)
	{
		typedef cfg_iterator<T> ent_iterator;

		if (find_internal(ent) != ent_iterator())
			return false;

		T *datap = reinterpret_cast<T *>(get_entry(cfg_entry));
		memcpy(datap, &ent, sizeof(T));
		return true;
	}

	template<typename T>
	bool erase_internal(const T& ent, sg_cfg_entry_t& cfg_entry)
	{
		typedef cfg_iterator<T> ent_iterator;

		ent_iterator iter = find_internal(ent);
		if (iter == ent_iterator())
			return false;

		put_entry(cfg_entry, &(*iter));
		return true;
	}

	gpp_ctx *GPP;
	sgp_ctx *SGP;

	boost::shared_ptr<bi::file_mapping> mapping_mgr;
	boost::shared_ptr<bi::mapped_region> region_mgr;

	char *base_addr;
	sg_cfg_map_t *cfg_map;
	sg_bbparms_t *bbp;
	bool opened;
	std::string sgconfig;
	int ref_count;
	boost::mutex cfg_mutex;

	static void init_once();

	static boost::once_flag once_flag;
	static sg_config *_instance;

	friend class sgt_ctx;
};

}
}

#endif

