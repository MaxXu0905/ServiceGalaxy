#if !defined(__SHM_MAP_H__)
#define __SHM_MAP_H__

#include "shm_tree.h"
#include "machine.h"

namespace ai
{
namespace sg
{

template<typename _Key, typename _Val>
class _Select1st
{
public:
	const _Key& operator()(const _Val& __data)
	{
		return __data.first;
	}
};

template<typename _Key, typename _Val, typename _Compare = std::less<_Key> >
class shm_map
{
public:
	typedef _Key key_type;
	typedef _Val mapped_type;
	typedef typename std::pair<_Key, _Val> value_type;
	typedef _Compare key_compare;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;

	typedef typename shm_tree<_Key, value_type, _Select1st<_Key, value_type>, _Compare>::iterator iterator;
	typedef typename shm_tree<_Key, value_type, _Select1st<_Key, value_type>, _Compare>::const_iterator const_iterator;
	typedef typename shm_tree<_Key, value_type, _Select1st<_Key, value_type>, _Compare>::reverse_iterator reverse_iterator;
	typedef typename shm_tree<_Key, value_type, _Select1st<_Key, value_type>, _Compare>::const_reverse_iterator const_reverse_iterator;

	shm_map()
		: _M_t()
	{
	}

	shm_map(shm_malloc& shm_mgr, const shm_link_t& shm_link = shm_link_t::SHM_LINK_NULL)
		: _M_t(shm_mgr, shm_link, SHM_MAP_TYPE)
	{
	}

	~shm_map()
	{
	}

	shm_map& operator=(const shm_map& __x)
	{
		_M_t = __x._M_t;
		return *this;
	}

	shm_map& copy(const shm_map& __x)
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

	mapped_type& operator[](const key_type& __k)
	{
		iterator __i = lower_bound(__k);
		if (__i == end() || _Compare()(__k, __i->first))
			__i = insert(value_type(__k, mapped_type()));
		return __i->second;
	}

	mapped_type& at(const key_type& __k)
	{
		iterator __i = lower_bound(__k);
		if (__i == end() || _Compare()(__k, __i->first))
			throw std::out_of_range((_("ERROR: out of range")).str(SGLOCALE));
		return __i->second;
	}

	const mapped_type& at(const key_type& __k) const
	{
		const_iterator __i = lower_bound(__k);
		if (__i == end() || _Compare()(__k, __i->first))
			throw std::out_of_range((_("ERROR: out of range")).str(SGLOCALE));
		return __i->second;
	}

	iterator insert(const value_type& __x)
	{
		_M_t.insert(__x);
		return _M_t.find(__x.first);
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
	shm_tree<_Key, value_type, _Select1st<_Key, value_type>, _Compare> _M_t;
};

}
}

#endif

