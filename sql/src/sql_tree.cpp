#include "sql_tree.h"

namespace ai
{
namespace scci
{

sql_tree::sql_tree()
{
}

sql_tree::sql_tree(composite_item_t *root)
{
	set_root(root);
}

sql_tree::sql_tree(arg_list_t *root)
{
	set_root(root);
}

sql_tree::sql_tree(relation_item_t *root)
{
	set_root(root);
}

sql_tree::~sql_tree()
{
}

void sql_tree::set_root(composite_item_t *root)
{
 	if (root)
		root->push(vector_root);
}

void sql_tree::set_root(arg_list_t *root)
{
	if (root) {
		for (arg_list_t::iterator iter = root->begin(); iter != root->end(); ++iter)
			(*iter)->push(vector_root);
	}
}

void sql_tree::set_root(relation_item_t *root)
{
	if (root)
		root->push(vector_root);
}

void sql_tree::clear()
{
	vector_root.clear();
}

sql_tree::iterator sql_tree::begin()
{
	return vector_root.begin();
}

sql_tree::iterator sql_tree::end()
{
	return vector_root.end();
}

sql_tree::const_iterator sql_tree::begin() const
{
	return vector_root.begin();
}

sql_tree::const_iterator sql_tree::end() const
{
	return vector_root.end();
}

sql_tree::reverse_iterator sql_tree::rbegin()
{
	return vector_root.rbegin();
}

sql_tree::reverse_iterator sql_tree::rend()
{
	return vector_root.rend();
}

sql_tree::const_reverse_iterator sql_tree::rbegin() const
{
	return vector_root.rbegin();
}

sql_tree::const_reverse_iterator sql_tree::rend() const
{
	return vector_root.rend();
}

}
}

