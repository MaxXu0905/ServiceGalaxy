#if !defined(__SQL_TREE_H__)
#define __SQL_TREE_H__

#include <vector>
#include "sql_base.h"

namespace ai
{
namespace scci
{

using namespace std;

class sql_tree
{
public:
	typedef vector<sql_item_t>::iterator iterator;
	typedef vector<sql_item_t>::const_iterator const_iterator;
	typedef vector<sql_item_t>::reverse_iterator reverse_iterator;
	typedef vector<sql_item_t>::const_reverse_iterator const_reverse_iterator;

	sql_tree();
	sql_tree(composite_item_t *root);
	sql_tree(arg_list_t *root);
	sql_tree(relation_item_t *root);
	~sql_tree();

	void set_root(composite_item_t *root);
	void set_root(arg_list_t *root);
	void set_root(relation_item_t *root);
	void clear();

	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;
	reverse_iterator rbegin();
	reverse_iterator rend();
	const_reverse_iterator rbegin() const;
	const_reverse_iterator rend() const;

private:
	vector<sql_item_t> vector_root;
};

}
}

#endif

