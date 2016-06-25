#if !defined(__SG_LOCK_H__)
#define __SG_LOCK_H__

#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/exceptions.hpp>
#include "sg_struct.h"

namespace bi = boost::interprocess;

namespace ai
{
namespace sg
{

class sharable_bblock
{
public:
	sharable_bblock(sgt_ctx *SGT_);
	~sharable_bblock();

private:
	sgt_ctx *SGT;
	bool require_unlock;
};

class scoped_bblock
{
public:
	scoped_bblock(sgt_ctx *SGT_);
	scoped_bblock(sgt_ctx *SGT_, bi::defer_lock_type);
	~scoped_bblock();

	void lock();
	void unlock();

private:
	sgt_ctx *SGT;
	bool require_unlock;
};

class scoped_syslock
{
public:
	scoped_syslock(sgt_ctx *SGT_);
	~scoped_syslock();

	void lock();
	void unlock();

private:
	sgt_ctx *SGT;
	bool require_unlock;
};

}
}

#endif

