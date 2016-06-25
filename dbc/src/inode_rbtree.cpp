#include "dbc_internal.h"

namespace ai
{
namespace sg
{

inode_rbtree& inode_rbtree::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<inode_rbtree *>(DBCT->_DBCT_mgr_array[DBC_INODE_TREE_MANAGER]);
}

long inode_rbtree::insert(const dbc_inode_t& inode, long& root_offset, COMP_FUNC compare, dbc_upgradable_mutex_t& mutex)
{
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_ie_t *ie = ie_mgr.get_ie();
	dbc_index_t& ie_meta = ie->ie_meta;

	long parent_offset = -1;
	long node_offset;
	dbc_inode_t *node;

	while (1) {
		node_offset = search_auxiliary(inode, &parent_offset, root_offset, compare);
		if (node_offset != -1) {
			if (!(ie_meta.index_type & INDEX_TYPE_NORMAL)) {
				/* 同一关键字的所有记录，其是否存在状态是一致的，
				 * 因此，只需找到第一条记录即可
				 */
				int result = check_exist(node_offset);
				if (result == -1) {
					DBCT->_DBCT_error_code = DBCEDUPENT;
					return -1;
				} else if (result == 0) {
					node = DBCT->long2inode(node_offset);
					row_link_t *link = DBCT->rowid2link(node->rowid);
					// 先增加引用计数，保证不会被回收重用
					link->inc_ref();
					// 释放桶锁，保证别的会话可以进入
					mutex.unlock();
					// 等待行级锁
					if (!te_mgr.lock_row(link)) {
						mutex.lock();
						return -1;
					}
					// 释放行级锁
					te_mgr.unlock_row(link);
					// 减少引用计数
					link->dec_ref();
					// 加上桶锁
					mutex.lock();
					// 重新查找
					continue;
				}
			}

			/* 继续遍历，挪到叶节点。主键或唯一索引在索引上可能
			 * 存在多条记录，这是因为，当数据更新后，但没有提交
			 * 时，需要保证通过索引能查到未提交的记录。下面的例
			 * 子就说明了可能会有多条相同记录:
			 * 1. 插入记录A
			 * 2. 更新记录A到B
			 * 3. 更新记录B到A
			 * 这样，主键或唯一索引中就会有两条记录A的索引，分别
			 * 指向老记录A和新记录A。
			 */
			char *inode_data = DBCT->rowid2data(inode.rowid);
			do {
				parent_offset = node_offset;
				node = DBCT->long2inode(node_offset);

				char *node_data = DBCT->rowid2data(node->rowid);
				long ret = (*compare)(inode_data, node_data);
				if (ret < 0)
					node_offset = node->left;
				else
					node_offset = node->right;
			} while (node_offset != -1);
		}

		break;
	}

	node_offset = new_node(inode);
	if (node_offset == -1) {
		DBCT->_DBCT_error_code = DBCEOS;
		return -1;
	}

	node = DBCT->long2inode(node_offset);
	node->parent = parent_offset;
	node->left = -1;
	node->right = -1;
	node->color = RED;

	if (parent_offset != -1) {
		dbc_inode_t *parent = DBCT->long2inode(parent_offset);
		char *parent_data = DBCT->rowid2data(parent->rowid);
		char *node_data = DBCT->rowid2data(node->rowid);
		long ret = (*compare)(parent_data, node_data);
		if (ret > 0)
			parent->left = node_offset;
		else
			parent->right = node_offset;
	} else {
		root_offset = node_offset;
	}

	return insert_rebalance(node_offset, root_offset);
}

void inode_rbtree::search(const dbc_inode_t& inode, long root_offset, COMP_FUNC compare)
{
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_ie_t *ie = ie_mgr.get_ie();
	dbc_index_t& ie_meta = ie->ie_meta;
	stack<long> root_stack;

	while (1) {
		long node_offset = search_auxiliary(inode, NULL, root_offset, compare);
		if (node_offset == -1) {
ENTRY:
			if (root_stack.empty())
				return;

			root_offset = root_stack.top();
			root_stack.pop();
			continue;
		}

		dbc_inode_t *root_inode = DBCT->long2inode(node_offset);
		row_link_t *link = DBCT->rowid2link(root_inode->rowid);
		if (DBCT->row_visible(link)) {
			if (DBCT->rows_enough())
				break;

			te_mgr.ref_row(root_inode->rowid);
			DBCT->_DBCT_rowids->push_back(root_inode->rowid);

			// 主键或唯一索引最多只可能找到一条
			if (!(ie_meta.index_type & INDEX_TYPE_NORMAL))
				break;
		}

		if (root_inode->left != -1) {
			if (root_inode->right != -1)
				root_stack.push(root_inode->right);

			root_offset = root_inode->left;
		} else if (root_inode->right != -1) {
			root_offset = root_inode->right;
		} else {
			goto ENTRY;
		}
	}
}

void inode_rbtree::search(long root_offset)
{
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	long node_offset = root_offset;
	stack<long> root_stack;

	char *global[3];
	global[1] = DBCT->_DBCT_data_start;
	global[2] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);

	while (node_offset != -1) {
		dbc_inode_t *node = DBCT->long2inode(node_offset);
		global[0] = DBCT->rowid2data(node->rowid);

		int ret = DBCT->execute_where_func(global);
		if (ret == 0) {
			row_link_t *link = DBCT->rowid2link(node->rowid);
			if (DBCT->row_visible(link)) {
				if (DBCT->rows_enough())
					break;
				te_mgr.ref_row(node->rowid);
				DBCT->_DBCT_rowids->push_back(node->rowid);
			}
		}

		if (node->left != -1) {
			if (node->right != -1)
				root_stack.push(node->right);

			node_offset = node->left;
		} else if (node->right != -1) {
			node_offset = node->right;
		} else {
			if (root_stack.empty())
				return;

			node_offset = root_stack.top();
			root_stack.pop();
		}
	}
}

long inode_rbtree::erase(const dbc_inode_t& inode, long& root_offset, COMP_FUNC compare)
{
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_inode_t *child;
	dbc_inode_t *parent;
	dbc_inode_t *old;
	dbc_inode_t *node;
	color_t color;
	long node_offset;
	long child_offset;
	long parent_offset;
	long old_offset;

	if ((node_offset = search_auxiliary_erase(inode, root_offset, compare)) == -1)
		return -1;

	node = DBCT->long2inode(node_offset);
	old_offset = node_offset;
	old = node;

	if (node->left != -1 && node->right != -1) {
		node = DBCT->long2inode(node->right);
		while (node->left != -1)
			node = DBCT->long2inode(node->left);

		node_offset = DBCT->inode2long(node);
		child_offset = node->right;
		parent_offset = node->parent;
		child = DBCT->long2inode(child_offset);
		parent = DBCT->long2inode(parent_offset);
		color = node->color;

		if (child_offset != -1)
			child->parent = parent_offset;

		if (parent_offset != -1) {
			if (parent->left == node_offset)
				parent->left = child_offset;
			else
				parent->right = child_offset;
		} else {
			root_offset = child_offset;
		}

		if (parent_offset == old_offset)
			parent = node;

		node->parent = old->parent;
		node->color = old->color;
		node->right = old->right;
		node->left = old->left;

		if (old->parent != -1) {
			dbc_inode_t *o_parent = DBCT->long2inode(old->parent);
			if (o_parent->left == old_offset)
				o_parent->left = node_offset;
			else
				o_parent->right = node_offset;
		} else {
			root_offset = node_offset;
		}

		dbc_inode_t *o_left = DBCT->long2inode(old->left);
		o_left->parent = node_offset;
		if (old->right != -1) {
			dbc_inode_t *o_right = DBCT->long2inode(old->right);
			o_right->parent = node_offset;
		}
	} else {
		if (node->left == -1)
			child_offset = node->right;
		else if (node->right == -1)
			child_offset = node->left;

		parent_offset = node->parent;
		child = DBCT->long2inode(child_offset);
		parent = DBCT->long2inode(parent_offset);
		color = node->color;

		if (child_offset != -1)
			child->parent = parent_offset;

		if (parent_offset != -1) {
			if (parent->left == node_offset)
				parent->left = child_offset;
			else
				parent->right = child_offset;
		} else {
			root_offset = child_offset;
		}
	}

	ie_mgr.put_inode(node);

	if (color == BLACK)
		root_offset = erase_rebalance(child_offset, parent_offset, root_offset);

	return root_offset;
}

inode_rbtree::inode_rbtree()
{
}

inode_rbtree::~inode_rbtree()
{
}

long inode_rbtree::new_node(const dbc_inode_t& inode)
{
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);

	dbc_inode_t *result = ie_mgr.get_inode();
	if (result == NULL)
		return -1;

	*result = inode;
	return DBCT->inode2long(result);
}

/*-----------------------------------------------------------
|   node           right
|   / \    ==>     / \
|   a  right     node  y
|       / \           / \
|       b  y         a   b
 -----------------------------------------------------------*/
void inode_rbtree::rotate_left(long node_offset, long& root_offset)
{
	dbc_inode_t *node = DBCT->long2inode(node_offset);
	long right_offset = node->right;
	dbc_inode_t *right = DBCT->long2inode(right_offset);

	node->right = right->left;
	if (right->left != -1) {
		dbc_inode_t *right_left = DBCT->long2inode(right->left);
		right_left->parent = node_offset;
	}
	right->left = node_offset;

	right->parent = node->parent;
	if (node->parent != -1) {
		dbc_inode_t *node_parent = DBCT->long2inode(node->parent);
		if (node_offset == node_parent->right)
			node_parent->right = right_offset;
		else
			node_parent->left = right_offset;
	} else {
		root_offset = right_offset;
	}

	node->parent = right_offset;
}

/*-----------------------------------------------------------
|       node           left
|       / \            / \
|    left  y   ==>    a   node
|   / \               / \
|  a   b             b   y
-----------------------------------------------------------*/
void inode_rbtree::rotate_right(long node_offset, long& root_offset)
{
	dbc_inode_t *node = DBCT->long2inode(node_offset);
	long left_offset = node->left;
	dbc_inode_t *left = DBCT->long2inode(left_offset);

	node->left = left->right;
	if (left->right != -1) {
		dbc_inode_t *left_right = DBCT->long2inode(left->right);
		left_right->parent = node_offset;
	}
	left->right = node_offset;

	left->parent = node->parent;
	if (node->parent != -1) {
		dbc_inode_t *node_parent = DBCT->long2inode(node->parent);
		if (node_offset == node_parent->right)
			node_parent->right = left_offset;
		else
			node_parent->left = left_offset;
	} else {
		root_offset = left_offset;
	}

	node->parent = left_offset;
}

long inode_rbtree::insert_rebalance(long node_offset, long& root_offset)
{
	dbc_inode_t *node = DBCT->long2inode(node_offset);
	dbc_inode_t *parent;
	dbc_inode_t *gparent;
	dbc_inode_t *uncle;

	while (1) {
		long parent_offset = node->parent;
		parent = DBCT->long2inode(parent_offset);
		if (node->parent == -1 || parent->color != RED)
			break;

		gparent = DBCT->long2inode(parent->parent);
		if (parent_offset == gparent->left) {
			uncle = DBCT->long2inode(gparent->right);
			if (gparent->right != -1 && uncle->color == RED) {
				uncle->color = BLACK;
				parent->color = BLACK;
				gparent->color = RED;
				node = gparent;
				node_offset = DBCT->inode2long(node);
			} else {
				if (parent->right == node_offset) {
					rotate_left(parent_offset, root_offset);
					std::swap(parent, node);
					node_offset = DBCT->inode2long(node);
				}

				parent->color = BLACK;
				gparent->color = RED;

				long gparent_offset = DBCT->inode2long(gparent);
				rotate_right(gparent_offset, root_offset);
			}
		} else {
			uncle = DBCT->long2inode(gparent->left);
			if (gparent->left != -1 && uncle->color == RED) {
				uncle->color = BLACK;
				parent->color = BLACK;
				gparent->color = RED;
				node = gparent;
				node_offset = DBCT->inode2long(node);
			} else {
				if (parent->left == node_offset) {
					rotate_right(parent_offset, root_offset);
					std::swap(parent, node);
					node_offset = DBCT->inode2long(node);
				}

				parent->color = BLACK;
				gparent->color = RED;

				long gparent_offset = DBCT->inode2long(gparent);
				rotate_left(gparent_offset, root_offset);
			}
		}
	}

	dbc_inode_t *root = DBCT->long2inode(root_offset);
	root->color = BLACK;

	return root_offset;
}

long inode_rbtree::erase_rebalance(long node_offset, long parent_offset, long& root_offset)
{
	dbc_inode_t *node = DBCT->long2inode(node_offset);
	dbc_inode_t *parent = DBCT->long2inode(parent_offset);
	dbc_inode_t *other;
	dbc_inode_t *o_left;
	dbc_inode_t *o_right;

	while ((node_offset == -1 || node->color == BLACK) && node_offset != root_offset) {
		if (parent->left == node_offset) {
			other = DBCT->long2inode(parent->right);
			if (other->color == RED) {
				other->color = BLACK;
				parent->color = RED;
				rotate_left(parent_offset, root_offset);
				other = DBCT->long2inode(parent->right);
			}

			o_left = DBCT->long2inode(other->left);
			o_right = DBCT->long2inode(other->right);
			if ((other->left == -1 || o_left->color == BLACK)
				&& (other->right == -1 || o_right->color == BLACK)) {
				other->color = RED;
				node = parent;
				node_offset = DBCT->inode2long(node);
				parent_offset = node->parent;
				parent = DBCT->long2inode(parent_offset);
			} else {
				if (other->right == -1 || o_right->color == BLACK) {
					if (other->left != -1)
						o_left->color = BLACK;

					other->color = RED;
					long other_offset = DBCT->inode2long(other);
					rotate_right(other_offset, root_offset);
					other = DBCT->long2inode(parent->right);
				}

				other->color = parent->color;
				parent->color = BLACK;
				if (other->right != -1) {
					o_right = DBCT->long2inode(other->right);
					o_right->color = BLACK;
				}

				parent_offset = DBCT->inode2long(parent);
				rotate_left(parent_offset, root_offset);
				node_offset = root_offset;
				node = DBCT->long2inode(node_offset);
				break;
			}
		} else {
			other = DBCT->long2inode(parent->left);
			if (other->color == RED) {
				other->color = BLACK;
				parent->color = RED;
				parent_offset = DBCT->inode2long(parent);
				rotate_right(parent_offset, root_offset);
				other = DBCT->long2inode(parent->left);
			}

			o_left = DBCT->long2inode(other->left);
			o_right = DBCT->long2inode(other->right);
			if ((other->left == -1 || o_left->color == BLACK)
				&& (other->right == -1 || o_right->color == BLACK)) {
				other->color = RED;
				node = parent;
				node_offset = DBCT->inode2long(node);
				parent_offset = node->parent;
				parent = DBCT->long2inode(parent_offset);
			} else {
				if (other->left == -1 || o_left->color == BLACK) {
					if (other->right != -1)
						o_right->color = BLACK;

					other->color = RED;
					long other_offset = DBCT->inode2long(other);
					rotate_left(other_offset, root_offset);
					other = DBCT->long2inode(parent->left);
				}


				other->color = parent->color;
				parent->color = BLACK;
				if (other->left != -1) {
					o_left = DBCT->long2inode(other->left);
					o_left->color = BLACK;
				}

				parent_offset = DBCT->inode2long(parent);
				rotate_left(parent_offset, root_offset);
				node_offset = root_offset;
				node = DBCT->long2inode(node_offset);
				break;
			}
		}
	}

	if (node_offset != -1)
		node->color = BLACK;

	return root_offset;
}

// 找到一条深度最小的记录
long inode_rbtree::search_auxiliary(const dbc_inode_t& inode, long *save, long root_offset, COMP_FUNC compare)
{
	long node_offset = root_offset;
	dbc_inode_t *node = DBCT->long2inode(node_offset);
	long parent_offset = -1;

	char *inode_data = DBCT->rowid2data(inode.rowid);
	while (node_offset != -1) {
		char *node_data = DBCT->rowid2data(node->rowid);
		long ret = (*compare)(inode_data, node_data);
		if (ret < 0) {
			parent_offset = node_offset;
			node_offset = node->left;
			node = DBCT->long2inode(node_offset);
		} else if (ret > 0) {
			parent_offset = node_offset;
			node_offset = node->right;
			node = DBCT->long2inode(node_offset);
		} else {
			break;
		}
	}

	if (save)
		*save = parent_offset;

	return node_offset;
}

// 根据rowid去匹配结果，其结果最多只有一条
long inode_rbtree::search_auxiliary_erase(const dbc_inode_t& inode, long root_offset, COMP_FUNC compare)
{
	stack<long> root_stack;
	long node_offset;
	dbc_inode_t *node;

	while (1) {
		node_offset = search_auxiliary(inode, NULL, root_offset, compare);
		if (node_offset == -1) {
ENTRY:
			if (root_stack.empty())
				return -1;

			root_offset = root_stack.top();
			root_stack.pop();
			continue;
		}

		node = DBCT->long2inode(node_offset);
		if (node->rowid == inode.rowid)
			return node_offset;

		if (node->left != -1) {
			if (node->right != -1)
				root_stack.push(node->right);

			root_offset = node->left;
		} else {
			goto ENTRY;
		}
	}

	return -1;
}

// 查看当前节点是否需要等待
// -1: 主键冲突，需要处理错误
// 0: 其他会话占用，需要等待
// 1: 正常，可以插入
int inode_rbtree::check_exist(long node_offset)
{
	dbc_inode_t *node = DBCT->long2inode(node_offset);
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.get_te();
	row_link_t *link = DBCT->rowid2link(node->rowid);

	switch (link->flags & (ROW_TRAN_INSERT | ROW_TRAN_DELETE)) {
	case ROW_TRAN_INSERT:
		if (DBCT->changed(te->table_id, node->rowid))
			return -1;
		else
			return 0;
		break;
	case ROW_TRAN_DELETE:
	case (ROW_TRAN_INSERT | ROW_TRAN_DELETE):
		if (DBCT->changed(te->table_id, node->rowid))
			return 1;
		else
			return 0;
		break;
	default:
		return -1;
	}
}

}
}

