#include "dbc_internal.h"

namespace ai
{
namespace sg
{

inode_list& inode_list::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<inode_list *>(DBCT->_DBCT_mgr_array[DBC_INODE_LIST_MANAGER]);
}

long inode_list::insert(const dbc_snode_t& snode, long& root_offset, COMP_FUNC compare, dbc_upgradable_mutex_t& mutex)
{
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_ie_t *ie = ie_mgr.get_ie();
	dbc_index_t& ie_meta = ie->ie_meta;

	long prev_offset = -1;
	long node_offset;
	dbc_snode_t *node;

	while (1) {
		node_offset = search_auxiliary(snode, &prev_offset, root_offset, compare);
		if (node_offset != -1 && !(ie_meta.index_type & INDEX_TYPE_NORMAL)) {
			/* 同一关键字的所有记录，其是否存在状态是一致的，
			 * 因此，只需找到第一条记录即可
			 */
			int result = check_exist(node_offset);
			if (result == -1) {
				DBCT->_DBCT_error_code = DBCEDUPENT;
				return -1;
			} else if (result == 0) {
				node = DBCT->long2snode(node_offset);
				row_link_t *link = DBCT->rowid2link(node->rowid);
				// 先增加引用计数，保证不会被回收重用，
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

		break;
	}

	node_offset = new_node(snode);
	if (node_offset == -1) {
		DBCT->_DBCT_error_code = DBCEOS;
		return -1;
	}

	node = DBCT->long2snode(node_offset);

	if (prev_offset != -1) {
		dbc_snode_t *prev = DBCT->long2snode(prev_offset);

		if (prev->next != -1) {
			dbc_snode_t *next = DBCT->long2snode(prev->next);
			next->prev = node_offset;
		}
		node->next = prev->next;
		prev->next = node_offset;
		node->prev = prev_offset;
	} else {
		if (root_offset != -1) {
			dbc_snode_t *root = DBCT->long2snode(root_offset);
			root->prev = node_offset;
		}
		node->prev = -1;
		node->next = root_offset;
		root_offset = node_offset;
	}

	return node_offset;
}

void inode_list::search(const dbc_snode_t& snode, long root_offset, COMP_FUNC compare)
{
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);
	dbc_ie_t *ie = ie_mgr.get_ie();
	dbc_index_t& ie_meta = ie->ie_meta;
	long node_offset = root_offset;
	char *inode_data = DBCT->rowid2data(snode.rowid);

	while (node_offset != -1) {
		dbc_snode_t *node = DBCT->long2snode(node_offset);
		char *node_data = DBCT->rowid2data(node->rowid);
		long ret = (*compare)(inode_data, node_data);
		if (ret == 0) {
			row_link_t *link = DBCT->rowid2link(node->rowid);
			if (DBCT->row_visible(link)) {
				if (DBCT->rows_enough())
					break;

				te_mgr.ref_row(node->rowid);
				DBCT->_DBCT_rowids->push_back(node->rowid);

				// 主键或唯一索引最多只可能找到一条
				if (!(ie_meta.index_type & INDEX_TYPE_NORMAL))
					break;
			}
		}

		node_offset = node->next;
	}
}

void inode_list::search(long root_offset)
{
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	long node_offset = root_offset;

	char *global[3];
	global[1] = DBCT->_DBCT_data_start;
	global[2] = reinterpret_cast<char *>(&DBCT->_DBCT_timestamp);

	while (node_offset != -1) {
		dbc_snode_t *node = DBCT->long2snode(node_offset);
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

		node_offset = node->next;
	}
}

long inode_list::erase(const dbc_snode_t& snode, long& root_offset)
{
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);

	long node_offset = root_offset;
	while (node_offset != -1) {
		dbc_snode_t *node = DBCT->long2snode(node_offset);
		if (node->rowid == snode.rowid) {
			if (node->prev != -1) {
				dbc_snode_t *prev = DBCT->long2snode(node->prev);
				prev->next = node->next;
			} else {
				root_offset = node->next;
			}
			if (node->next != -1) {
				dbc_snode_t *next = DBCT->long2snode(node->next);
				next->prev = node->prev;
			}

			ie_mgr.put_snode(node);
			return root_offset;
		}

		node_offset = node->next;
	}

	return -1;
}

inode_list::inode_list()
{
}

inode_list::~inode_list()
{
}

long inode_list::new_node(const dbc_snode_t& snode)
{
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);

	dbc_snode_t *result = ie_mgr.get_snode();
	if (result == NULL)
		return -1;

	*result = snode;
	return DBCT->snode2long(result);
}

// 按照降序方式排序，保证最长匹配和任意匹配规则
long inode_list::search_auxiliary(const dbc_snode_t& snode, long *save, long root_offset, COMP_FUNC compare)
{
	long node_offset = root_offset;
	dbc_snode_t *node = DBCT->long2snode(node_offset);
	long prev_offset = -1;

	char *inode_data = DBCT->rowid2data(snode.rowid);
	while (node_offset != -1) {
		char *node_data = DBCT->rowid2data(node->rowid);
		long ret = (*compare)(inode_data, node_data);
		if (ret > 0) {
			prev_offset = node_offset;
			node_offset = node->next;
			node = DBCT->long2snode(node_offset);
		} else if (ret < 0) {
			node_offset = -1;
			break;
		} else {
			break;
		}
	}

	*save = prev_offset;
	return node_offset;
}

// 查看当前节点是否需要等待
// -1: 主键冲突，需要处理错误
// 0: 其他会话占用，需要等待
// 1: 正常，可以插入
int inode_list::check_exist(long node_offset)
{
	dbc_snode_t *node = DBCT->long2snode(node_offset);
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

