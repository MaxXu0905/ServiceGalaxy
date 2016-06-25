#include "uhash.h"

namespace ai
{
namespace sg
{

int const INT32MASK = ~(1 << (sizeof(int) * 8 - 1));

// lookup is read-only
int const uhash::lookup[16] = 
{
	0x00000000,
	0xCC017601,
	0xD8014C01,
	0x14003A00,
	0xF0013801,
	0x3C004E00,
	0x28007400,
	0xE4010201,
	0xA001D001,
	0x6C00A600,
	0x78009C00,
	0xB401EA01,
	0x5000E800,
	0x9C019E01,
	0x8801A401,
	0x4400D200
};

/*
 * hashl added to handle the problems of hashing byte-order dependent
 * entities such as longs identically on heterogeneous platforms. _e_gp_hash
 * provides byte order dependent hashing for string values. Any short, int
 * or long hashing should be done through _e_gp_hashl() to preserve order
 * independence. The current algorithm is simply a modulus algorithm
 * deemed to be the best combination of simplicity and efficiency for
 * the needs of /T.
 *
 * key is the key value to be hashed and bkts is the number of bkts to use.
 */
long uhash::hashl(long key, long bkts)
{
	return (key % bkts);
}

long uhash::hashname(const char *key, long bkts)
{
	const char *cp;
	short ch;
	int lkey;
	cp = key;

	lkey = 0;
	while(*cp) {
		ch = *cp++ & 0377;
		lkey = (int)(((lkey>>4)&0xFFFFFFFL) ^ (lookup[lkey&017] ^ lookup[ch&017]));
		ch >>= 4;
		lkey = (int)(((lkey>>4)&0xFFFFFFFL) ^ (lookup[lkey&017] ^ lookup[ch&017]));
		ch >>= 4;
	}

	lkey &= INT32MASK;

	// use modulo to determine bucket
	return (lkey % bkts);
}

long uhash::hashlist(const char **key_arr, long bkts)
{
	const char *cp;
	short ch;
	int lkey;

	lkey = 0;
	for(cp = key_arr[0];*key_arr; cp = *++key_arr) {
		while(*cp) {
			ch = *cp++ & 0377;
			lkey = (int)(((lkey>>4)&0xFFFFFFFL) ^ (lookup[lkey&017] ^ lookup[ch&017]));
			ch >>= 4;
			lkey = (int)(((lkey>>4)&0xFFFFFFFL) ^ (lookup[lkey&017] ^ lookup[ch&017]));
			ch >>= 4;
		}
	}

	lkey &= INT32MASK;

	// use modulo to determine bucket
	return (lkey % bkts);
}
 
long uhash::hash(const char *key, unsigned short kfl, long bkts)
{
	const char *cp;
	short ch;
	int lkey;

	cp = key;
	lkey = (int)0;
	while(kfl--) {
		ch = *cp++ & 0377;
		lkey = (int)(((lkey>>4)&0xFFFFFFFL) ^ (lookup[lkey&017] ^ lookup[ch&017]));
		ch >>= 4;
		lkey = (int)(((lkey>>4)&0xFFFFFFFL) ^ (lookup[lkey&017] ^ lookup[ch&017]));
		ch >>= 4;
	}

	lkey &= INT32MASK;

	// use modulo to determine bucket
	return (lkey % bkts);
}

}
}

