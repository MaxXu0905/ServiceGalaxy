#ifndef __COMPARER_H__
#define __COMPARER_H__

namespace ai
{
namespace sg
{

class icomparer
{
public:
	icomparer() {}
	virtual ~icomparer() {}
	virtual int compare_binary(int nTableIndex,const void *x,const void * y) = 0;
	virtual int compare_linear(int nTableIndex,const void *x,const void * y) = 0;
	virtual int compare_hash(int nTableIndex,const void *x,const void * y) = 0;
};

}
}

#endif

