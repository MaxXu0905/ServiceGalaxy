#if !defined(__INODE_RBTREE_H__)
#define __INODE_RBTREE_H__

#include "dbc_struct.h"

namespace ai
{
namespace sg
{

class inode_rbtree : public dbc_manager
{
public:
	static inode_rbtree& instance(dbct_ctx *DBCT);

	long insert(const dbc_inode_t& inode, long& root_offset, COMP_FUNC compare, dbc_upgradable_mutex_t& mutex);
	void search(const dbc_inode_t& inode, long root_offset, COMP_FUNC compare);
	void search(long root_offset);
	long erase(const dbc_inode_t& inode, long& root_offset, COMP_FUNC compare);

private:
	inode_rbtree();
	virtual ~inode_rbtree();

	long new_node(const dbc_inode_t& inode);
	void rotate_left(long node_offset, long& root_offset);
	void rotate_right(long node_offset, long& root_offset);
	long insert_rebalance(long node_offset, long& root_offset);
	long erase_rebalance(long node_offset, long parent_offset, long& root_offset);
	long search_auxiliary(const dbc_inode_t& inode, long *save, long root_offset, COMP_FUNC compare);
	long search_auxiliary_erase(const dbc_inode_t& inode, long root_offset, COMP_FUNC compare);
	int check_exist(long node_offset);

	friend class dbct_ctx;
};

}
}

#endif

