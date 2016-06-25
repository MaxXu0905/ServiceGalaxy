#if !defined(__SHM_VECTOR_H__)
#define __SHM_VECTOR_H__

#include "shm_malloc.h"
#include "shm_stl.h"
#include "machine.h"

namespace ai
{
namespace sg
{

template<typename _Val>
struct shm_vector_base
{
	int flags;
	size_t size;
	size_t capacity;
	_Val _M_data;
};

template<typename _Val>
class shm_vector
{
public:
	typedef _Val value_type;
	typedef _Val * pointer;
	typedef const _Val * const_pointer;
	typedef _Val& reference;
	typedef const _Val& const_reference;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef _Val * iterator;
	typedef const _Val * const_iterator;
	typedef _Val * reverse_iterator;
	typedef const _Val * const_reverse_iterator;

	shm_vector()
	{
		shm_mgr = NULL;
		_M_impl = NULL;
	}

	shm_vector(shm_malloc& shm_mgr_, const shm_link_t& shm_link = shm_link_t::SHM_LINK_NULL)
		: shm_mgr(&shm_mgr_)
	{
		if (shm_link == shm_link_t::SHM_LINK_NULL) {
			_M_impl = NULL;
			alloc(1);
		} else {
			_M_impl = reinterpret_cast<shm_vector_base<_Val> *>(shm_mgr->get_user_data(shm_link));
			if (_M_impl->flags != SHM_VECTOR_TYPE)
				throw runtime_error((_("shm_vector::shm_vector")).str(SGLOCALE));
		}
	}

	~shm_vector()
	{
	}

	shm_vector& operator=(const shm_vector& __x)
	{
		shm_mgr = __x.shm_mgr;
		_M_impl = __x._M_impl;
		return *this;
	}

	shm_vector& copy(const shm_vector& __x)
	{
		clear();
		alloc(__x.size());
		for (int i = 0; i < __x.size(); i++)
			push_back(__x[i]);
		return *this;
	}

	iterator begin()
	{
		return &_M_impl->_M_data;
	}

	const_iterator begin() const
	{
		return &_M_impl->_M_data;
	}

	iterator end()
	{
		return &_M_impl->_M_data + _M_impl->size;
	}
	const_iterator end() const
	{
		return &_M_impl->_M_data + _M_impl->size;
	}

	reverse_iterator rbegin()
	{
		return &_M_impl->_M_data -1 + _M_impl->size;
	}

	const_reverse_iterator rbegin() const
	{
		return &_M_impl->_M_data -1 + _M_impl->size;
	}

	reverse_iterator rend()
	{
		return &_M_impl->_M_data -1;
	}
	const_reverse_iterator rend() const
	{
		return &_M_impl->_M_data -1;
	}

	size_type size() const
	{
		return _M_impl->size;
	}

	void resize(size_type __new_size, value_type __x = value_type())
	{
		alloc(__new_size);
		for (int i = _M_impl->size; i < __new_size; i++)
			push_back(__x);
	}

	size_type capacity() const
	{
		return _M_impl->capacity;
	}

	bool empty() const
	{
		return (_M_impl->size == 0);
	}

	void reserve(size_type __n)
	{
		alloc(__n);
	}

	reference operator[](size_type __n)
	{
		return (&_M_impl->_M_data)[__n];
	}

	const_reference operator[](size_type __n) const
	{
		return (&_M_impl->_M_data)[__n];
	}

	reference at(size_type __n)
	{
		range_check(__n);
		return (*this)[__n];
	}

	const_reference at(size_type __n) const
	{
		range_check(__n);
		return (*this)[__n];
	}

	reference front()
	{
		return (*this)[0];
	}

	 const_reference front() const
	 {
	 	return (*this)[0];
	 }

	 reference back()
	 {
	 	return (*this)[size() - 1];
	 }

	const_reference back() const
	{
		return (*this)[size() - 1];
	}

	pointer data()
	{
		return &_M_impl->_M_data;
	}

	const_pointer data() const
	{
		return &_M_impl->_M_data;
	}

	void push_back(const value_type& __x)
	{
		if (_M_impl->size == _M_impl->capacity)
			alloc(_M_impl->size + 1);

		_M_impl->size++;
		reference __back = back();
		__back = __x;
	}

	void pop_back()
	{
		_M_impl->size--;
		pointer data = &_M_impl->_M_data + _M_impl->size;
		data->~_Val();
	}

	void clear()
	{
		for (int i = 0; i < _M_impl->size; i++) {
			reference val = (*this)[i];
			val.~_Val();
		}

		_M_impl->size = 0;
	}

	shm_link_t get_shm_link()
	{
		return shm_mgr->get_shm_link(_M_impl);
	}

	void destroy()
	{
		if (_M_impl == NULL)
			return;

		clear();
		shm_mgr->free(_M_impl);
		_M_impl = NULL;
	}

private:
	void range_check(size_type __n)
	{
		if (__n >= size())
			throw out_of_range((_("ERROR: out of range")).str(SGLOCALE));
	}

	void alloc(int __n = 1)
	{
		if (_M_impl != NULL && _M_impl->capacity >= __n)
			return;

		int alloc_size = SHM_MIN_SIZE;
		size_t data_size = __n * sizeof(_Val) + sizeof(shm_vector_base<_Val>) + sizeof(shm_data_t) - sizeof(_Val);
		while (alloc_size < data_size)
			alloc_size <<= 1;
		alloc_size -= sizeof(shm_data_t) - sizeof(void *);

		if (_M_impl == NULL) {
			_M_impl = reinterpret_cast<shm_vector_base<_Val> *>(shm_mgr->malloc(alloc_size));
			if (_M_impl == NULL)
				throw bad_alloc();

			_M_impl->flags = SHM_VECTOR_TYPE;
			_M_impl->size = 0;
			_M_impl->capacity = (alloc_size - sizeof(shm_vector_base<_Val>) + sizeof(_Val)) / sizeof(_Val);
		} else {
			void *addr = shm_mgr->realloc(_M_impl, alloc_size);
			if (addr == NULL)
				throw bad_alloc();

			_M_impl = reinterpret_cast<shm_vector_base<_Val> *>(addr);
			_M_impl->capacity = (alloc_size - sizeof(shm_vector_base<_Val>) + sizeof(_Val)) / sizeof(_Val);
		}
	}

	shm_malloc *shm_mgr;
	shm_vector_base<_Val> *_M_impl;
};

}
}

#endif
