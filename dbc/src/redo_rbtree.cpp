#include "dbc_internal.h"

namespace ai
{
namespace sg
{

redo_rbtree& redo_rbtree::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<redo_rbtree *>(DBCT->_DBCT_mgr_array[DBC_REDO_TREE_MANAGER]);
}

long redo_rbtree::insert(const redo_row_t& redo_row)
{
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();
	long& root_offset = rte->redo_root;

	long parent_offset = -1;
	long node_offset;
	redo_row_t *node;

	if (search_auxiliary(redo_row, &parent_offset) != -1) {
		DBCT->_DBCT_error_code = DBCEDUPENT;
		return -1;
	}

	node_offset = new_node(redo_row);
	if (node_offset == -1) // ERROR already set in previous function.
		return -1;

	node = DBCT->long2redorow(node_offset);
	node->parent = parent_offset;
	node->left = -1;
	node->right = -1;
	node->color = RED;

	if (parent_offset != -1) {
		redo_row_t *parent = DBCT->long2redorow(parent_offset);
		int ret = parent->compare(*node);
		if (ret > 0)
			parent->left = node_offset;
		else
			parent->right = node_offset;
	} else {
		root_offset = node_offset;
	}

	return insert_rebalance(node_offset);
}

// 不比较sequence，因此使用compare_s
long redo_rbtree::search(const redo_row_t& redo_row)
{
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();

	long node_offset = rte->redo_root;
	redo_row_t *node = DBCT->long2redorow(node_offset);
	long parent_offset = -1;
	int ret;

	while (node_offset != -1) {
		ret = node->compare_s(redo_row);
		if (ret > 0) {
			parent_offset = node_offset;
			node_offset = node->left;
			node = DBCT->long2redorow(node_offset);
		} else if (ret < 0) {
			parent_offset = node_offset;
			node_offset = node->right;
			node = DBCT->long2redorow(node_offset);
		} else {
			break;
		}
	}

	return node_offset;
}

long redo_rbtree::erase(const redo_row_t& redo_row)
{
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();
	long& root_offset = rte->redo_root;

	redo_row_t *child;
	redo_row_t *parent;
	redo_row_t *old;
	redo_row_t *node;
	color_t color;
	long node_offset;
	long child_offset;
	long parent_offset;
	long old_offset;

	if ((node_offset = search_auxiliary(redo_row, NULL)) == -1)
		return -1;

	node = DBCT->long2redorow(node_offset);
	old_offset = node_offset;
	old = node;

	if (node->left != -1 && node->right != -1) {
		node = DBCT->long2redorow(node->right);
		while (node->left != -1)
			node = DBCT->long2redorow(node->left);

		node_offset = DBCT->redorow2long(node);
		child_offset = node->right;
		parent_offset = node->parent;
		child = DBCT->long2redorow(child_offset);
		parent = DBCT->long2redorow(parent_offset);
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
			redo_row_t *o_parent = DBCT->long2redorow(old->parent);
			if (o_parent->left == old_offset)
				o_parent->left = node_offset;
			else
				o_parent->right = node_offset;
		} else {
			root_offset = node_offset;
		}

		redo_row_t *o_left = DBCT->long2redorow(old->left);
		o_left->parent = node_offset;
		if (old->right != -1) {
			redo_row_t *o_right = DBCT->long2redorow(old->right);
			o_right->parent = node_offset;
		}
	} else {
		if (node->left == -1)
			child_offset = node->right;
		else if (node->right == -1)
			child_offset = node->left;

		parent_offset = node->parent;
		child = DBCT->long2redorow(child_offset);
		parent = DBCT->long2redorow(parent_offset);
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

	dbc_redo& redo_mgr = dbc_redo::instance(DBCT);
	redo_bucket_t *redo_bucket = redo_mgr.row2bucket(old);
	redo_bucket->freed++;
	old->flags |= REDO_MARK_REMOVED;

	if (color == BLACK)
		root_offset = erase_rebalance(child_offset, parent_offset);

	return root_offset;
}

redo_rbtree::redo_rbtree()
{
}

redo_rbtree::~redo_rbtree()
{
}

long redo_rbtree::new_node(const redo_row_t& redo_row)
{
	dbc_redo& redo_mgr = dbc_redo::instance(DBCT);
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();
	redo_row_t *result = NULL;

	if (rte->redo_free == -1) {
		redo_bucket_t *redo_bucket = redo_mgr.get_bucket();
		if (redo_bucket == NULL) // ERROR alreay set in previous function.
			return -1;

		redo_bucket->prev = -1;
		redo_bucket->next = rte->redo_free;
		rte->redo_free = DBCT->redobucket2long(redo_bucket);

		redo_bucket->used++;
		result = &redo_bucket->redo_rows[0];
	} else {
		redo_bucket_t *redo_bucket = DBCT->long2redobucket(rte->redo_free);
		if (redo_bucket->freed > 0) {
			// 至少能找到一个节点
			for (int i =0; i < redo_bucket->used; i++) {
				result = &redo_bucket->redo_rows[i];
				if (result->flags & REDO_MARK_REMOVED) {
					result->flags &= ~REDO_MARK_REMOVED;
					redo_bucket->freed--;
					break;
				}
			}
		} else {
			result = &redo_bucket->redo_rows[redo_bucket->used];
			result->flags = REDO_IN_USE;
			if (++redo_bucket->used == REDO_ROWS_PER_BUCKET) {
				// 从空闲列表删除
				rte->redo_free = redo_bucket->next;
				if (rte->redo_free != -1) {
					redo_bucket_t *redo_free = DBCT->long2redobucket(rte->redo_free);
					redo_free->prev = -1;
				}

				// 插入到满列表
				long bucket_offset = DBCT->redobucket2long(redo_bucket);
				redo_bucket->prev = -1;
				redo_bucket->next = rte->redo_full;
				if (rte->redo_full != -1) {
					redo_bucket_t *redo_full = DBCT->long2redobucket(rte->redo_full);
					redo_full->prev = bucket_offset;
				}
				rte->redo_full = bucket_offset;

			}
		}
	}

	*result = redo_row;
	return DBCT->redorow2long(result);
}

/*-----------------------------------------------------------
|   node           right
|   / \    ==>     / \
|   a  right     node  y
|       / \           / \
|       b  y         a   b
 -----------------------------------------------------------*/
void redo_rbtree::rotate_left(long node_offset)
{
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();
	long& root_offset = rte->redo_root;

	redo_row_t *node = DBCT->long2redorow(node_offset);
	long right_offset = node->right;
	redo_row_t *right = DBCT->long2redorow(right_offset);

	node->right = right->left;
	if (right->left != -1) {
		redo_row_t *right_left = DBCT->long2redorow(right->left);
		right_left->parent = node_offset;
	}
	right->left = node_offset;

	right->parent = node->parent;
	if (node->parent != -1) {
		redo_row_t *node_parent = DBCT->long2redorow(node->parent);
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
void redo_rbtree::rotate_right(long node_offset)
{
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();
	long& root_offset = rte->redo_root;

	redo_row_t *node = DBCT->long2redorow(node_offset);
	long left_offset = node->left;
	redo_row_t *left = DBCT->long2redorow(left_offset);

	node->left = left->right;
	if (left->right != -1) {
		redo_row_t *left_right = DBCT->long2redorow(left->right);
		left_right->parent = node_offset;
	}
	left->right = node_offset;

	left->parent = node->parent;
	if (node->parent != -1) {
		redo_row_t *node_parent = DBCT->long2redorow(node->parent);
		if (node_offset == node_parent->right)
			node_parent->right = left_offset;
		else
			node_parent->left = left_offset;
	} else {
		root_offset = left_offset;
	}

	node->parent = left_offset;
}

long redo_rbtree::insert_rebalance(long node_offset)
{
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();
	long& root_offset = rte->redo_root;

	redo_row_t *node = DBCT->long2redorow(node_offset);
	redo_row_t *parent;
	redo_row_t *gparent;
	redo_row_t *uncle;

	while (1) {
		long parent_offset = node->parent;
		parent = DBCT->long2redorow(parent_offset);
		if (node->parent == -1 || parent->color != RED)
			break;

		gparent = DBCT->long2redorow(parent->parent);
		if (parent_offset == gparent->left) {
			uncle = DBCT->long2redorow(gparent->right);
			if (gparent->right != -1 && uncle->color == RED) {
				uncle->color = BLACK;
				parent->color = BLACK;
				gparent->color = RED;
				node = gparent;
				node_offset = DBCT->redorow2long(node);
			} else {
				if (parent->right == node_offset) {
					rotate_left(parent_offset);
					std::swap(parent, node);
					node_offset = DBCT->redorow2long(node);
				}

				parent->color = BLACK;
				gparent->color = RED;

				long gparent_offset = DBCT->redorow2long(gparent);
				rotate_right(gparent_offset);
			}
		} else {
			uncle = DBCT->long2redorow(gparent->left);
			if (gparent->left != -1 && uncle->color == RED) {
				uncle->color = BLACK;
				parent->color = BLACK;
				gparent->color = RED;
				node = gparent;
				node_offset = DBCT->redorow2long(node);
			} else {
				if (parent->left == node_offset) {
					rotate_right(parent_offset);
					std::swap(parent, node);
					node_offset = DBCT->redorow2long(node);
				}

				parent->color = BLACK;
				gparent->color = RED;

				long gparent_offset = DBCT->redorow2long(gparent);
				rotate_left(gparent_offset);
			}
		}
	}

	redo_row_t *root = DBCT->long2redorow(root_offset);
	root->color = BLACK;

	return root_offset;
}

long redo_rbtree::erase_rebalance(long node_offset, long parent_offset)
{
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();
	long& root_offset = rte->redo_root;

	redo_row_t *node = DBCT->long2redorow(node_offset);
	redo_row_t *parent = DBCT->long2redorow(parent_offset);
	redo_row_t *other;
	redo_row_t *o_left;
	redo_row_t *o_right;

	while ((node_offset == -1 || node->color == BLACK) && node_offset != root_offset) {
		if (parent->left == node_offset) {
			other = DBCT->long2redorow(parent->right);
			if (other->color == RED) {
				other->color = BLACK;
				parent->color = RED;
				rotate_left(parent_offset);
				other = DBCT->long2redorow(parent->right);
			}

			o_left = DBCT->long2redorow(other->left);
			o_right = DBCT->long2redorow(other->right);
			if ((other->left == -1 || o_left->color == BLACK)
				&& (other->right == -1 || o_right->color == BLACK)) {
				other->color = RED;
				node = parent;
				node_offset = DBCT->redorow2long(node);
				parent_offset = node->parent;
				parent = DBCT->long2redorow(parent_offset);
			} else {
				if (other->right == -1 || o_right->color == BLACK) {
					if (other->left != -1)
						o_left->color = BLACK;

					other->color = RED;
					long other_offset = DBCT->redorow2long(other);
					rotate_right(other_offset);
					other = DBCT->long2redorow(parent->right);
				}

				other->color = parent->color;
				parent->color = BLACK;
				if (other->right != -1) {
					o_right = DBCT->long2redorow(other->right);
					o_right->color = BLACK;
				}

				parent_offset = DBCT->redorow2long(parent);
				rotate_left(parent_offset);
				node_offset = root_offset;
				node = DBCT->long2redorow(node_offset);
				break;
			}
		} else {
			other = DBCT->long2redorow(parent->left);
			if (other->color == RED) {
				other->color = BLACK;
				parent->color = RED;
				parent_offset = DBCT->redorow2long(parent);
				rotate_right(parent_offset);
				other = DBCT->long2redorow(parent->left);
			}

			o_left = DBCT->long2redorow(other->left);
			o_right = DBCT->long2redorow(other->right);
			if ((other->left == -1 || o_left->color == BLACK)
				&& (other->right == -1 || o_right->color == BLACK)) {
				other->color = RED;
				node = parent;
				node_offset = DBCT->redorow2long(node);
				parent_offset = node->parent;
				parent = DBCT->long2redorow(parent_offset);
			} else {
				if (other->left == -1 || o_left->color == BLACK) {
					if (other->right != -1)
						o_right->color = BLACK;

					other->color = RED;
					long other_offset = DBCT->redorow2long(other);
					rotate_left(other_offset);
					other = DBCT->long2redorow(parent->left);
				}


				other->color = parent->color;
				parent->color = BLACK;
				if (other->left != -1) {
					o_left = DBCT->long2redorow(other->left);
					o_left->color = BLACK;
				}

				parent_offset = DBCT->redorow2long(parent);
				rotate_left(parent_offset);
				node_offset = root_offset;
				node = DBCT->long2redorow(node_offset);
				break;
			}
		}
	}

	if (node_offset != -1)
		node->color = BLACK;

	return root_offset;
}

long redo_rbtree::search_auxiliary(const redo_row_t& redo_row, long *save)
{
	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	dbc_rte_t *rte = rte_mgr.get_rte();

	long node_offset = rte->redo_root;
	redo_row_t *node = DBCT->long2redorow(node_offset);
	long parent_offset = -1;
	int ret;

	while (node_offset != -1) {
		ret = node->compare(redo_row);
		if (ret > 0) {
			parent_offset = node_offset;
			node_offset = node->left;
			node = DBCT->long2redorow(node_offset);
		} else if (ret < 0) {
			parent_offset = node_offset;
			node_offset = node->right;
			node = DBCT->long2redorow(node_offset);
		} else {
			break;
		}
	}

	if (save)
		*save = parent_offset;

	return node_offset;
}

}
}

