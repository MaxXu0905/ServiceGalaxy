#if !defined(__UHASH_H__)
#define __UHASH_H__

namespace ai
{
namespace sg
{

class uhash
{
public:
	static long hashl(long key, long bkts);
	static long hashname(const char *key, long bkts);
	static long hashlist(const char **key_arr, long bkts);
	static long hash(const char *key, unsigned short kfl, long bkts);

private:
	static int const lookup[16];
};

}
}

#endif

