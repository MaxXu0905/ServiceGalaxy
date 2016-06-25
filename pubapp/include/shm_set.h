#if !defined(__SHM_SET_H__)
#define __SHM_SET_H__

#include "shm_tree.h"
#include "machine.h"

namespace ai
{
namespace sg
{

template<typename _Key>
class _Identity
{
public:
	const _Key& operator()(const _Key& __data)
	{
		return __data;
	}
};

template<typename _Key, typename _Compare = std::less<_Key> >
class shm_set
{
public:
	typedef _Key key_type;
	typedef _Key value_type;
	typedef _Compare key_compare;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	
	typedef typename shm_tree<_Key, value_type, _Identity<_Key>, _Compare>::iterator iterator;
	typedef typename shm_tree<_Key, value_type, _Identity<_Key>, _Compare>::const_iterator const_iterator;
	typedef typename shm_tree<_Key, value_type, _Identity<_Key>, _Compare>::reverse_iterator reverse_iterator;
	typedef typename shm_tree<_Key, value_type, _Identity<_Key>, _Compare>::const_reverse_iterator const_reverse_iterator;

	shm_set()
		: _M_t()
	{
	}

	shm_set(shm_malloc& shm_mgr, const shm_link_t& shm_link = shm_link_t::SHM_LINK_NULL)
		: _M_t(shm_mgr, shm_link, SHM_SET_TYPE)
	{
	}
	
	~shm_set()
	{
	}

	shm_set& operator=(const shm_set& __x)
	{
		_M_t = __x._M_t;
		return *this;
	}

	shm_set& copy(const shm_set& __x)
	{
		return _M_t.copy(__x._M_t);
	}

	iterator begin()
	{
		return _M_t.begin();
	}
	
	const_iterator begin() const
	{
		return _M_t.begin();
	}
	
	iterator end()
	{
		return _M_t.end();
	}
	const_iterator end() const
	{
		return _M_t.end();
	}

	reverse_iterator rbegin()
	{
		return _M_t.rbegin();
	}
	
	const_reverse_iterator rbegin() const
	{
		return _M_t.rbegin();
	}
	
	reverse_iterator rend()
	{
		return _M_t.rend();
	}
	const_reverse_iterator rend() const
	{
		return _M_t.rend();
	}

	size_type size() const
	{
		return _M_t.size();
	}
	
	size_type capacity() const
	{
		return _M_t.capacity();
	}
	
	bool empty() const
	{
		return _M_t.empty();
	}

	iterator insert(const value_type& __x)
	{
		_M_t.insert(__x);
		return _M_t.find(__x);
	}

	void erase(iterator __position)
	{
		_M_t.erase(__position);
	}

	size_type erase(const key_type& __k)
	{
		return _M_t.erase(__k);
	}

	iterator find(const key_type& __k)
	{
		return _M_t.find(__k);
	}

	const_iterator find(const key_type& __k) const
	{
		return _M_t.find(__k);
	}

	size_type count(const key_type& __k) const
	{
		return (_M_t.find(__k) == _M_t.end() ? 0 : 1);
	}

	iterator lower_bound(const key_type& __k)
	{
		return _M_t.lower_bound(__k);
	}

	const_iterator lower_bound(const key_type& __k) const
	{
		return _M_t.lower_bound(__k);
	}

	iterator upper_bound(const key_type& __k)
	{
		return _M_t.upper_bound(__k);
	}

	const_iterator upper_bound(const key_type& __k) const
	{
		return _M_t.upper_bound(__k);
	}

	std::pair<iterator, iterator> equal_range(const key_type& __k)
	{
		return _M_t.equal_range(__k);
	}

	std::pair<const_iterator, const_iterator> equal_range(const key_type& __k) const
	{
		return _M_t.equal_range(__k);
	}
		
	void clear()
	{
		_M_t.clear();
	}

	shm_link_t get_shm_link()
	{
		return _M_t.get_shm_link();
	}

	void destroy()
	{
		_M_t.destroy();
	}

private:
	shm_tree<_Key, value_type, _Identity<_Key>, _Compare> _M_t;
};

}
}

#endif

