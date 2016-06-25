#if !defined(__CFG_ITERATOR_H__)
#define __CFG_ITERATOR_H__

namespace ai
{
namespace sg
{

template<typename T>
class cfg_iterator
{
public:
	struct T_list
	{
		sg_syslink_t syslink;
		T value;
	};

	cfg_iterator(void *base_, int head)
		: base(reinterpret_cast<char *>(base_)),
		  current(head)
	{
	}

	cfg_iterator()
		: base(NULL),
		  current(-1)
	{
	}

	~cfg_iterator()
	{
	}

	cfg_iterator& operator++()
	{
		T_list *entry = reinterpret_cast<T_list *>(base + current);
		current = entry->syslink.flink;
		return *this;
	}

	cfg_iterator operator++(int)
	{
		cfg_iterator _this = *this;
		T_list *entry = reinterpret_cast<T_list *>(base + current);
		current = entry->syslink.flink;
		return _this;
	}

	cfg_iterator& operator--()
	{
		T_list *entry = reinterpret_cast<T_list *>(base + current);
		current = entry->syslink.blink;
		return *this;
	}

	cfg_iterator operator--(int)
	{
		cfg_iterator _this = *this;
		T_list *entry = reinterpret_cast<T_list *>(base + current);
		current = entry->syslink.blink;
		return _this;
	}

	bool operator==(const cfg_iterator& rhs) const
	{
		return (current == rhs.current);
	}

	bool operator!=(const cfg_iterator& rhs) const
	{
		return (current != rhs.current);
	}

	T& operator*()
	{
		T_list *entry = reinterpret_cast<T_list *>(base + current);
		return entry->value;
	}

	T * operator->()
	{
		T_list *entry = reinterpret_cast<T_list *>(base + current);
		return &entry->value;
	}

private:
	char *base;
	int current;
};

}
}

#endif


