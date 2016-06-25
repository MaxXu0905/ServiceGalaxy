#if !defined(__SHM_TREE_H__)
#define __SHM_TREE_H__

#include "shm_malloc.h"
#include "shm_stl.h"
#include "machine.h"

namespace ai
{
namespace sg
{

template<typename _Val>
struct shm_tree_node
{
	_Val data;
	int left;
	int right;
	int bf;

	void init(const _Val& __data)
	{
		data = __data;
		left = -1;
		right = -1;
		bf = 0;
	}
};

template<typename _Val>
struct shm_tree_head
{
	int flags;
	size_t size;
	size_t capacity;
	int _M_head;
	shm_tree_node<_Val> _M_data;
};

template<typename _Key, typename _Val, typename _KeyOfValue, typename _Compare>
class shm_tree_iterator
{
public:
	typedef _Val value_type;
	typedef value_type& reference;
	typedef value_type * pointer;
	typedef ptrdiff_t difference_type;
	typedef shm_tree_iterator<_Key, _Val, _KeyOfValue, _Compare> _Self;
	typedef shm_tree_node<value_type> * _Link_type;

	explicit shm_tree_iterator(shm_tree_head<value_type> *__x)
	{
		_M_impl = __x;
		p = -1;

		_M_node = &__x->_M_data + __x->_M_head;
		if (__x->_M_head == -1)
			return;

		while (_M_node->left != -1) {
			path[++p] = _M_node;
			_M_node = &_M_impl->_M_data + _M_node->left;
		}
	}

	explicit shm_tree_iterator(shm_tree_head<value_type> *__x, _Link_type __node)
	{
		_M_impl = __x;
		p = -1;

		_M_node = &__x->_M_data + __x->_M_head;
		if (__x->_M_head == -1)
			return;

		while (1) {
			if (_Compare()(_S_key(_M_node->data), _S_key(__node->data))) {
				_M_node = &_M_impl->_M_data + _M_node->right;
			} else if (_Compare()(_S_key(__node->data), _S_key(_M_node->data))) {
				path[++p] = _M_node;
				_M_node = &_M_impl->_M_data + _M_node->left;
			} else {
				break;
			}
		}
	}

	reference operator*() const
	{
		return _M_node->data;
	}

	pointer operator->() const
	{
		return &_M_node->data;
	}

	_Self& operator++()
	{
		next();
		return *this;
	}

	_Self operator++(int)
	{
		_Self __tmp = *this;
		next();
		return __tmp;
	}

	bool operator==(const _Self& __x) const
	{
		return _M_node == __x._M_node;
	}

	bool operator!=(const _Self& __x) const
	{
		return _M_node != __x._M_node;
	}

	_Self& operator=(const _Self& __x)
	{
		p = __x.p;
		for (int i = 0; i <= p; i++)
			path[i] = __x.path[i];
		_M_impl = __x._M_impl;
		_M_node = __x._M_node;
		return *this;
	}

	int p;
	_Link_type path[64];
	shm_tree_head<value_type> *_M_impl;
	_Link_type _M_node;

private:
	void next()
	{
		if (_M_node->right != -1) {
			_M_node = &_M_impl->_M_data + _M_node->right;
			while (_M_node->left != -1) {
				path[++p] = _M_node;
				_M_node = &_M_impl->_M_data + _M_node->left;
			}
		} else {
			if (p >= 0)
				_M_node = path[p--];
			else
				_M_node = &_M_impl->_M_data - 1;
		}
	}

	static const _Key& _S_key(const _Val& __x)
	{
		return _KeyOfValue()(__x);
	}
};

template<typename _Key, typename _Val, typename _KeyOfValue, typename _Compare>
class shm_tree_const_iterator
{
public:
	typedef _Val value_type;
	typedef const value_type& reference;
	typedef const value_type * pointer;
	typedef ptrdiff_t difference_type;
	typedef shm_tree_iterator<_Key, _Val, _KeyOfValue, _Compare> _Self;
	typedef shm_tree_node<value_type> * _Link_type;

	explicit shm_tree_const_iterator(shm_tree_head<value_type> *__x)
	{
		_M_impl = __x;
		p = -1;

		_M_node = &__x->_M_data + __x->_M_head;
		if (__x->_M_head == -1)
			return;

		while (_M_node->left != -1) {
			path[++p] = _M_node;
			_M_node = &_M_impl->_M_data + _M_node->left;
		}
	}

	explicit shm_tree_const_iterator(shm_tree_head<value_type> *__x, _Link_type __node)
	{
		_M_impl = __x;
		p = -1;

		_M_node = &__x->_M_data + __x->_M_head;
		if (__x->_M_head == -1)
			return;

		while (1) {
			if (_Compare()(_S_key(_M_node->data), _S_key(__node->data))) {
				_M_node = &_M_impl->_M_data + _M_node->right;
			} else if (_Compare()(_S_key(__node->data), _S_key(_M_node->data))) {
				path[++p] = _M_node;
				_M_node = &_M_impl->_M_data + _M_node->left;
			} else {
				break;
			}
		}
	}

	reference operator*() const
	{
		return _M_node->data;
	}

	pointer operator->() const
	{
		return &_M_node->data;
	}

	_Self& operator++()
	{
		next();
		return *this;
	}

	_Self operator++(int)
	{
		_Self __tmp = *this;
		next();
		return __tmp;
	}

	bool operator==(const _Self& __x) const
	{
		return _M_node == __x._M_node;
	}

	bool operator!=(const _Self& __x) const
	{
		return _M_node != __x._M_node;
	}

	_Self& operator=(const _Self& __x)
	{
		p = __x.p;
		for (int i = 0; i <= p; i++)
			path[i] = __x.path[i];
		_M_impl = __x._M_impl;
		_M_node = __x._M_node;
		return *this;
	}

	int p;
	_Link_type path[64];
	shm_tree_head<value_type> *_M_impl;
	_Link_type _M_node;

private:
	void next()
	{
		if (_M_node->right != -1) {
			_M_node = &_M_impl->_M_data + _M_node->right;
			while (_M_node->left != -1) {
				path[++p] = _M_node;
				_M_node = &_M_impl->_M_data + _M_node->left;
			}
		} else {
			if (p >= 0)
				_M_node = path[p--];
			else
				_M_node = &_M_impl->_M_data - 1;
		}
	}

	static const _Key& _S_key(const _Val& __x)
	{
		return _KeyOfValue()(__x);
	}
};

template<typename _Key, typename _Val, typename _KeyOfValue, typename _Compare>
class shm_tree_reverse_iterator
{
public:
	typedef _Val value_type;
	typedef value_type& reference;
	typedef value_type * pointer;
	typedef ptrdiff_t difference_type;
	typedef shm_tree_reverse_iterator<_Key, _Val, _KeyOfValue, _Compare> _Self;
	typedef shm_tree_node<value_type> * _Link_type;

	explicit shm_tree_reverse_iterator(shm_tree_head<value_type> *__x)
	{
		_M_impl = __x;
		p = -1;

		_M_node = &__x->_M_data + __x->_M_head;
		if (__x->_M_head == -1)
			return;

		while (_M_node->right != -1) {
			path[++p] = _M_node;
			_M_node = &_M_impl->_M_data + _M_node->right;
		}
	}

	explicit shm_tree_reverse_iterator(shm_tree_head<value_type> *__x, _Link_type __node)
	{
		_M_impl = __x;
		p = -1;

		_M_node = &__x->_M_data + __x->_M_head;
		if (__x->_M_head == -1)
			return;

		while (1) {
			if (_Compare()(_S_key(__node->data), _S_key(_M_node->data))) {
				_M_node = &_M_impl->_M_data + _M_node->left;
			} else if (_Compare()(_S_key(_M_node->data), _S_key(__node->data))) {
				path[++p] = _M_node;
				_M_node = &_M_impl->_M_data + _M_node->right;
			} else {
				break;
			}
		}
	}

	reference operator*() const
	{
		return _M_node->data;
	}

	pointer operator->() const
	{
		return &_M_node->data;
	}

	_Self& operator++()
	{
		next();
		return *this;
	}

	_Self operator++(int)
	{
		_Self __tmp = *this;
		next();
		return __tmp;
	}

	bool operator==(const _Self& __x) const
	{
		return _M_node == __x._M_node;
	}

	bool operator!=(const _Self& __x) const
	{
		return _M_node != __x._M_node;
	}

	_Self& operator=(const _Self& __x)
	{
		p = __x.p;
		for (int i = 0; i <= p; i++)
			path[i] = __x.path[i];
		_M_impl = __x._M_impl;
		_M_node = __x._M_node;
		return *this;
	}

	int p;
	_Link_type path[64];
	shm_tree_head<value_type> *_M_impl;
	_Link_type _M_node;

private:
	void next()
	{
		if (_M_node->left != -1) {
			_M_node = &_M_impl->_M_data + _M_node->left;
			while (_M_node->right != -1) {
				path[++p] = _M_node;
				_M_node = &_M_impl->_M_data + _M_node->right;
			}
		} else {
			if (p >= 0)
				_M_node = path[p--];
			else
				_M_node = &_M_impl->_M_data - 1;
		}
	}

	static const _Key& _S_key(const _Val& __x)
	{
		return _KeyOfValue()(__x);
	}
};

template<typename _Key, typename _Val, typename _KeyOfValue, typename _Compare>
class shm_tree_const_reverse_iterator
{
public:
	typedef _Val value_type;
	typedef const value_type& reference;
	typedef const value_type * pointer;
	typedef ptrdiff_t difference_type;
	typedef shm_tree_reverse_iterator<_Key, _Val, _KeyOfValue, _Compare> _Self;
	typedef shm_tree_node<value_type> * _Link_type;

	explicit shm_tree_const_reverse_iterator(shm_tree_head<value_type> *__x)
	{
		_M_impl = __x;
		p = -1;

		_M_node = &__x->_M_data + __x->_M_head;
		if (__x->_M_head == -1)
			return;

		while (_M_node->right != -1) {
			path[++p] = _M_node;
			_M_node = &_M_impl->_M_data + _M_node->right;
		}
	}

	explicit shm_tree_const_reverse_iterator(shm_tree_head<value_type> *__x, _Link_type __node)
	{
		_M_impl = __x;
		p = -1;

		_M_node = &__x->_M_data + __x->_M_head;
		if (__x->_M_head == -1)
			return;

		while (1) {
			if (_Compare()(_S_key(__node->data), _S_key(_M_node->data))) {
				_M_node = &_M_impl->_M_data + _M_node->left;
			} else if (_Compare()(_S_key(_M_node->data), _S_key(__node->data))) {
				path[++p] = _M_node;
				_M_node = &_M_impl->_M_data + _M_node->right;
			} else {
				break;
			}
		}
	}

	reference operator*() const
	{
		return _M_node->data;
	}

	pointer operator->() const
	{
		return &_M_node->data;
	}

	_Self& operator++()
	{
		next();
		return *this;
	}

	_Self operator++(int)
	{
		_Self __tmp = *this;
		next();
		return __tmp;
	}

	bool operator==(const _Self& __x) const
	{
		return _M_node == __x._M_node;
	}

	bool operator!=(const _Self& __x) const
	{
		return _M_node != __x._M_node;
	}

	_Self& operator=(const _Self& __x)
	{
		p = __x.p;
		for (int i = 0; i <= p; i++)
			path[i] = __x.path[i];
		_M_impl = __x._M_impl;
		_M_node = __x._M_node;
		return *this;
	}

	int p;
	_Link_type path[64];
	shm_tree_head<value_type> *_M_impl;
	_Link_type _M_node;

private:
	void next()
	{
		if (_M_node->left != -1) {
			_M_node = &_M_impl->_M_data + _M_node->left;
			while (_M_node->right != -1) {
				path[++p] = _M_node;
				_M_node = &_M_impl->_M_data + _M_node->right;
			}
		} else {
			if (p >= 0)
				_M_node = path[p--];
			else
				_M_node = &_M_impl->_M_data - 1;
		}
	}

	static const _Key& _S_key(const _Val& __x)
	{
		return _KeyOfValue()(__x);
	}
};

template<typename _Key, typename _Val, typename _KeyOfValue, typename _Compare>
class shm_tree
{
public:
	typedef size_t size_type;
	typedef _Key key_type;
	typedef _Val value_type;
	typedef shm_tree_node<value_type> Type;
	typedef shm_tree_node<value_type>& reference;
	typedef const shm_tree_node<value_type>& const_reference;
	typedef shm_tree_node<value_type>* pointer;
	typedef const shm_tree_node<value_type>* const_pointer;
	typedef shm_tree_iterator<_Key, _Val, _KeyOfValue, _Compare> iterator;
	typedef shm_tree_const_iterator<_Key, _Val, _KeyOfValue, _Compare> const_iterator;
	typedef shm_tree_reverse_iterator<_Key, _Val, _KeyOfValue, _Compare> reverse_iterator;
	typedef shm_tree_const_reverse_iterator<_Key, _Val, _KeyOfValue, _Compare> const_reverse_iterator;

	shm_tree()
	{
		shm_mgr = NULL;
		_M_impl = NULL;
	}

	shm_tree(shm_malloc& shm_mgr_, const shm_link_t& shm_link, int flags)
		: shm_mgr(&shm_mgr_)
	{
		if (shm_link == shm_link_t::SHM_LINK_NULL) {
			_M_impl = NULL;
			alloc(1);
			_M_impl->flags = flags;
		} else {			_M_impl = reinterpret_cast<shm_tree_head<value_type> *>(shm_mgr->get_user_data(shm_link));
			if (_M_impl->flags != flags)
				throw runtime_error((_("shm_tree::shm_tree")).str(SGLOCALE));
		}
	}

	~shm_tree()
	{
	}

	shm_tree& operator=(const shm_tree& __x)
	{
		shm_mgr = __x.shm_mgr;
		_M_impl = __x._M_impl;
		return *this;
	}

	shm_tree& copy(const shm_tree& __x)
	{
		clear();
		for (iterator iter = __x.begin(); iter != __x.end(); ++iter)
			insert(*iter);
		return *this;
	}

	iterator begin()
	{
		return iterator(_M_impl);
	}

	const_iterator begin() const
	{
		return const_iterator(_M_impl);
	}

	iterator end()
	{
		return iterator(_M_impl, &_M_impl->_M_data - 1);
	}
	const_iterator end() const
	{
		return const_iterator(_M_impl, &_M_impl->_M_data - 1);
	}

	reverse_iterator rbegin()
	{
		return reverse_iterator(_M_impl);
	}

	const_reverse_iterator rbegin() const
	{
		return const_reverse_iterator(_M_impl);
	}

	reverse_iterator rend()
	{
		return reverse_iterator(_M_impl, &_M_impl->_M_data - 1);
	}
	const_reverse_iterator rend() const
	{
		return const_reverse_iterator(_M_impl, &_M_impl->_M_data - 1);
	}

	size_type size() const
	{
		return _M_impl->size;
	}

	size_type capacity() const
	{
		return _M_impl->capacity;
	}

	bool empty() const
	{
		return (_M_impl->size == 0);
	}

	bool insert(const value_type& __data)
	{
		if (_M_impl->size == 0) { // ����ǿ��������½���Ϊ�����������ĸ�
			_M_impl->size++;
			_M_impl->_M_head = 0;
			_M_impl->_M_data.init(__data);
			return true;
		}

		// �����ȷ����ڴ棬�Ա�֤��ַ�������
		if (_M_impl->size == _M_impl->capacity)
			alloc(_M_impl->size + 1);

		p = 0;
		// prevΪ��һ�η��ʵĽ�㣬currentΪ��ǰ���ʽ��
		pointer prev = NULL;
		pointer current;
		int current_index = _M_impl->_M_head;
		while (current_index != -1) {
			current = get_link(current_index);
			path[p++] = current; // ��·���ϵĽ���������
			// �������ֵ�Ѵ��ڣ������ʧ��
			bool result = _Compare()(_S_key(__data), _S_key(current->data));
			if (!result && !_Compare()(_S_key(current->data), _S_key(__data))) {
				current->data = __data;
				return false;
			}
			prev = current;
			// ������ֵС�ڵ�ǰ��㣬������������������������������
			current_index = result ? prev->left : prev->right;
		}

		current_index = _M_impl->size++;
		current = get_link(current_index); // �����½��
		current->init(__data);
		if (_Compare()(_S_key(__data), _S_key(prev->data))) // �������ֵС��˫�׽���ֵ
			prev->left = current_index; //��Ϊ����
		else // �������ֵ����˫�׽���ֵ
			prev->right = current_index; // ��Ϊ�Һ���

		path[p] = current; //����Ԫ�ز�������path�����

		// �޸Ĳ�����������·���ϸ�����ƽ������
		int bf = 0;
		while (p > 0) { // bf��ʾƽ�����ӵĸı��������½���������������ƽ������+1
			// ���½���������������ƽ������-1
			bf = _Compare()(_S_key(__data), _S_key(path[p - 1]->data)) ? 1 : -1;
			path[--p]->bf += bf; //�ı䵱������ƽ������
			bf = path[p]->bf; //��ȡ��ǰ����ƽ������
			//�жϵ�ǰ���ƽ�����ӣ����Ϊ0��ʾ��������ƽ�⣬�����ٻ���
			//���ı����Ƚ��ƽ�����ӣ���ʱ��ӳɹ���ֱ�ӷ���
			if (bf == 0) {
				return true;
			} else if (bf == 2 || bf == -2) { //��Ҫ��ת�����
				RotateSubTree(bf);
				return true;
			}
		}
		return true;
	}

	void erase(iterator __position)
	{
		erase(_S_key(*__position));
	}

	void erase(const key_type& __k)
	{
		p = -1;
		// parent��ʾ˫�׽�㣬node��ʾ��ǰ���
		int current_index = _M_impl->_M_head;
		pointer current;
		// Ѱ��ָ��ֵ���ڵĽ��
		while (current_index != -1) {
			current = get_link(current_index);
			path[++p] = current;
			bool result = _Compare()(__k, _S_key(current->data));
			// ����ҵ��������RemoveNode����ɾ�����
			if (!result && !_Compare()(_S_key(current->data), __k)) {
				RemoveNode(current); // ����pָ��ɾ�����
				return; // ����true��ʾɾ���ɹ�
			}
			if (result) // ���ɾ��ֵС�ڵ�ǰ��㣬��������������Ѱ��
				current = current->left;
			else // ���ɾ��ֵ���ڵ�ǰ��㣬��������������Ѱ��
				current = current->right;
		}

		return; //����false��ʾɾ��ʧ��
	}

	iterator find(const key_type& __k)
	{
		iterator __i(_M_impl);
		int node_index = _M_impl->_M_head;
		while (node_index != -1) {
			pointer node = get_link(node_index);
			bool result = _Compare()(__k, _S_key(node->data));
			if (!result && !_Compare()(_S_key(node->data), __k)) {
				__i._M_node = node;
				return __i;
			}

			if (result) {
				__i.path[++__i.p] = node;
				node_index = node->left;
			} else {
				node_index = node->right;
			}
		}

		return end();
	}

	const_iterator find(const key_type& __k) const
	{
		const_iterator __i(_M_impl);
		int node_index = _M_impl->_M_head;
		while (node_index != -1) {
			pointer node = get_link(node_index);
			bool result = _Compare(__k, _S_key(node->data));
			if (!result && !_Compare()(_S_key(node->data), __k)) {
				__i._M_node = node;
				return __i;
			}

			if (result) {
				__i.path[++__i.p] = node;
				node_index = node->left;
			} else {
				node_index = node->right;
			}
		}

		return end();
	}

	iterator lower_bound(const key_type& __x)
	{
		return find(__x);
	}

	const_iterator lower_bound(const key_type& __x) const
	{
		return find(__x);
	}

	iterator upper_bound(const key_type& __x)
	{
		iterator __i = find(__x);
		for (; __i != end(); ++__i) {
			if (_Compare()(__x, _S_key(*__i)))
				break;
		}
		return __i;
	}

	const_iterator upper_bound(const key_type& __x) const
	{
		const_iterator __i = find(__x);
		for (; __i != end(); ++__i) {
			if (_Compare()(__x, _S_key(*__i)))
				break;
		}
		return __i;
	}

	std::pair<iterator, iterator> equal_range(const key_type& __x)
	{
		iterator __i = find(__x);
		iterator __j = __i;
		for (; __j != end(); ++__j) {
			if (_Compare()(__x, _S_key(*__j)))
				break;
		}
		return std::pair<iterator, iterator>(__i, __j);
	}

	std::pair<const_iterator, const_iterator> equal_range(const key_type& __x) const
	{
		const_iterator __i = find(__x);
		const_iterator __j = __i;
		for (; __j != end(); ++__j) {
			if (_Compare()(__x, _S_key(*__j)))
				break;
		}
		return std::pair<const_iterator, const_iterator>(__i, __j);
	}

	void clear()
	{

		for (iterator __i = begin(); __i != end(); ++__i)
			__i->~_Val();

		_M_impl->size = 0;
		_M_impl->_M_head = -1;
	}

	shm_link_t get_shm_link()
	{
		return shm_mgr->get_shm_link(_M_impl);
	}

	void destroy()
	{
		clear();
		shm_mgr->free(_M_impl);
		_M_impl = NULL;
	}

private:
	void alloc(int __n = 1)
	{
		if (_M_impl != NULL && _M_impl->capacity >= __n)
			return;

		int alloc_size = SHM_MIN_SIZE;
		size_t data_size = __n * sizeof(Type) + sizeof(shm_tree_head<value_type>) - sizeof(Type)
			+ sizeof(shm_data_t) - sizeof(void *);
		while (alloc_size < data_size)
			alloc_size <<= 1;
		alloc_size -= sizeof(shm_data_t) - sizeof(void *);

		if (_M_impl == NULL) {
			_M_impl = reinterpret_cast<shm_tree_head<value_type> *>(shm_mgr->malloc(alloc_size));
			if (_M_impl == NULL)
				throw bad_alloc();

			_M_impl->size = 0;
			_M_impl->capacity = (alloc_size - sizeof(shm_tree_head<value_type>) + sizeof(Type)) / sizeof(Type);
			_M_impl->_M_head = -1;
		} else {
			void *addr = shm_mgr->realloc(_M_impl, alloc_size);
			if (addr == NULL)
				throw bad_alloc();

			_M_impl = reinterpret_cast<shm_tree_head<value_type> *>(addr);
			_M_impl->capacity = (alloc_size - sizeof(shm_tree_head<value_type>) + sizeof(Type)) / sizeof(Type);
		}
	}

	shm_tree_node<value_type> * get_link(int index)
	{
		return &_M_impl->_M_data + index;
	}

	int to_index(const shm_tree_node<value_type> *current)
	{
		return (current - &_M_impl->_M_data);
	}

	//ɾ��ָ�����
	void RemoveNode(pointer node)
	{
		pointer tmp = NULL;

		// ����ɾ����������������ʱ
		if (node->left != -1 && node->right != -1) {
			tmp = get_link(node->left); // ��ȡ������
			path[++p] = tmp;
			while (tmp->right != -1) { // ��ȡnode���������ǰ����㣬�������tmp��
				// �ҵ��������е������½��
				tmp = get_link(tmp->right);
				path[++p] = tmp;
			}

			// ���������ǰ������ֵ���汻ɾ������ֵ
			node->data = tmp->data;
			if (path[p - 1] == node)
				path[p - 1]->left = tmp->left;
			else
				path[p - 1]->right = tmp->left;
		} else { // ��ֻ������������������ΪҶ�ӽ��ʱ
			// �����ҵ�Ωһ�ĺ��ӽ��
			int tmp_index;
			if (node->left != -1)
				tmp_index = node->left;
			else if (node->right != -1)
				tmp_index = node->right;
			else
				tmp_index = -1;

			if (p > 0) {
				if (path[p - 1]->left == node) // �����ɾ���������
					path[p - 1]->left = tmp_index;
				else // �����ɾ������Һ���
					path[p - 1]->right = tmp_index;
			} else { // ��ɾ�����Ǹ����ʱ
				_M_impl->_M_head = tmp_index;
			}
		}

		// ɾ����������ת������pָ��ʵ�ʱ�ɾ���Ľ��
		_Val& data = node->data;

		while (p > 0) 	{ // bf��ʾƽ�����ӵĸı�������ɾ�������������еĽ��ʱ��ƽ������-1
			// ��ɾ�������������ĺ���ʱ��ƽ������+1
			int bf = (data <= path[p - 1]->data) ? -1 : 1;
			path[--p].bf += bf; //�ı䵱������ƽ������
			bf = path[p].bf; //��ȡ��ǰ����ƽ������
			if (bf != 0) { //���bf==0�������߶Ƚ��ͣ��������ϻ���
				//���bfΪ1��-1��˵���߶�δ�䣬ֹͣ���ݣ����Ϊ2��-2���������ת
				//����ת��߶Ȳ��䣬��ֹͣ����
				if (bf == 1 || bf == -1 || !RotateSubTree(bf))
					break;
			}
		}
	}

	// ��ת��rootΪ�������������߶ȸı䣬�򷵻�true���߶�δ���򷵻�false
	bool RotateSubTree(int bf)
	{
		bool tallChange = true;
		pointer root = path[p], newRoot = NULL;
		if (bf == 2) { // ��ƽ������Ϊ2ʱ��Ҫ������ת����
			int leftBF = get_link(root->left)->bf;
			if (leftBF == -1) { // LR����ת
				newRoot = LR(root);
			} else if (leftBF == 1) {
				newRoot = LL(root); //LL����ת
			} else { // ����ת�����ӵ�bfΪ0ʱ��ֻ��ɾ��ʱ�Ż����
				newRoot = LL(root);
				tallChange = false;
			}
		}

		if (bf == -2) { // ��ƽ������Ϊ-2ʱ��Ҫ������ת����
			int rightBF = get_link(root->right)->bf; // ��ȡ��ת���Һ��ӵ�ƽ������
			if (rightBF == 1) {
				newRoot = RL(root); // RL����ת
			} else if (rightBF == -1) {
				newRoot = RR(root); // RR����ת
			} else { // ����ת�����ӵ�bfΪ0ʱ��ֻ��ɾ��ʱ�Ż����
				newRoot = RR(root);
				tallChange = false;
			}
		}

		//�����µ�������
		if (p > 0) {
			if (_Compare()(_S_key(root->data), _S_key(path[p - 1]->data)))
				path[p - 1]->left = to_index(newRoot);
			else
				path[p - 1]->right = to_index(newRoot);
		} else {
			_M_impl->_M_head = to_index(newRoot); //�����ת��ΪAVL���ĸ�����ָ����AVL�������
		}

		return tallChange;
	}

	// rootΪ��ת����rootPrevΪ��ת��˫�׽��
	// LL����ת��������ת�����������
	pointer LL(pointer root)
	{
		pointer rootNext = get_link(root->left);
		root->left = rootNext->right;
		rootNext->right = to_index(root);
		if (rootNext->bf== 1) {
			root->bf = 0;
			rootNext->bf = 0;
		} else { // rootNext->bf==0�������ɾ��ʱ��
			root->bf = 1;
			rootNext->bf = -1;
		}
		return rootNext; //rootNextΪ�������ĸ�
	}

	// LR����ת��������ת�����������
	pointer LR(pointer root)
	{
		pointer rootNext = get_link(root->left);
		pointer newRoot = get_link(rootNext->right);
		root->left = newRoot->right;
		rootNext->right = newRoot->left;
		newRoot->left = to_index(rootNext);
		newRoot->right = to_index(root);

		switch (newRoot->bf) { // �ı�ƽ������
		case 0:
			root->bf = 0;
			rootNext->bf = 0;
			break;
		case 1:
			root->bf = -1;
			rootNext->bf = 0;
			break;
		case -1:
			root->bf = 0;
			rootNext->bf = 1;
			break;
		}
		newRoot->bf = 0;
		return newRoot; //newRootΪ�������ĸ�
	}

	// RR����ת��������ת�����������
	pointer RR(pointer root)
	{
		pointer rootNext = get_link(root->right);
		root->right = rootNext->left;
		rootNext->left = to_index(root);

		if (rootNext->bf == -1) {
			root->bf = 0;
			rootNext->bf = 0;
		} else { // rootNext->bf==0�������ɾ��ʱ��
			root->bf = -1;
			rootNext->bf = 1;
		}

		return rootNext; // rootNextΪ�������ĸ�
	}

	// RL����ת��������ת�����������
	pointer RL(pointer root)
	{
		pointer rootNext = get_link(root->right);
		pointer newRoot = get_link(rootNext->left);
		root->right = newRoot->left;
		rootNext->left = newRoot->right;
		newRoot->right = to_index(rootNext);
		newRoot->left = to_index(root);

		switch (newRoot->bf) { //�ı�ƽ������
		case 0:
			root->bf = 0;
			rootNext->bf = 0;
			break;
		case 1:
			root->bf = 0;
			rootNext->bf = -1;
			break;
		case -1:
			root->bf = 1;
			rootNext->bf = 0;
			break;
		}

		newRoot->bf = 0;
		return newRoot; //newRootΪ�������ĸ�
	}

	static const _Key& _S_key(const _Val& __x)
	{
		return _KeyOfValue()(__x);
	}

	shm_malloc *shm_mgr;
	shm_tree_head<value_type> *_M_impl;

	pointer path[64]; //��¼����·���ϵĽ��
	int p; //��ʾ��ǰ���ʵ��Ľ����_path�ϵ�����
};

}
}

#endif

