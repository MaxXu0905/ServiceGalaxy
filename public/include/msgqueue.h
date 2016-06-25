#ifndef __MSGQUEUE_H__
#define __MSGQUEUE_H__

#include <sys/msg.h>
#include "machine.h"
#include "dstream.h"
#include "user_exception.h"

using namespace std;

namespace ai
{
namespace sg
{

class msgqueue
{
private:
	int   _nmsgid;
	int   _nrunmode;
	key_t _nIpcKey;
	
	struct message
	{
		long mtype;
		long	mprocid;
		char mtext[1]; // W -- wait  S -- start  R -- register
	};
    struct commandmsg
    {
        long mtype ; 
        char mtext[1024] ;
    };
public:
	msgqueue(int nIpcKey);
	~msgqueue();

	int create_msgqueue();
	int get_msgqueue();
	
	int sendmsg(int nmsgtype,char cmd);
	int recvmsg(int nmsgtype,long& pid,char& cmd);
    int recvmsg(long nmsgtype, char* msgbuf, size_t buf_size, int msg_flag = 0) ;
};

}
}

#endif

