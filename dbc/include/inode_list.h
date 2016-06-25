#if !defined(__INODE_LIST_H__)
#define __INODE_LIST_H__

#include "dbc_struct.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

class inode_list : public dbc_manager
{
public:
	static inode_list& instance(dbct_ctx *DBCT);

	long insert(const dbc_snode_t& snode, long& root_offset, COMP_FUNC compare, dbc_upgradable_mutex_t& mutex);
	void search(const dbc_snode_t& snode, long root_offset, COMP_FUNC compare);
	void search(long root_offset);
	long erase(const dbc_snode_t& snode, long& root_offset);

private:
	inode_list();
	virtual ~inode_list();

	long new_node(const dbc_snode_t& snode);
	long search_auxiliary(const dbc_snode_t& snode, long *save, long root_offset, COMP_FUNC compare);
	int check_exist(long node_offset);

	friend class dbct_ctx;
};

}
}

#endif

