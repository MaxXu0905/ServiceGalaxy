#if !defined(__REDO_RBTREE_H__)
#define __REDO_RBTREE_H__

#include "dbc_redo.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

class redo_rbtree : public dbc_manager
{
public:
	static redo_rbtree& instance(dbct_ctx *DBCT);

	long insert(const redo_row_t& redo_row);
	long search(const redo_row_t& redo_row);
	long erase(const redo_row_t& redo_row);

private:
	redo_rbtree();
	virtual ~redo_rbtree();

	long new_node(const redo_row_t& redo_row);
	void rotate_left(long node_offset);
	void rotate_right(long node_offset);
	long insert_rebalance(long node_offset);
	long erase_rebalance(long node_offset, long parent_offset);
	long search_auxiliary(const redo_row_t& redo_row, long *save);

	friend class dbct_ctx;
};

}
}

#endif

