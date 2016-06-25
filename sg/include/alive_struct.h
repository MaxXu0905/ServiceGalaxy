#if !defined(__ALIVE_STRUCT_H__)
#define __ALIVE_STRUCT_H__

namespace ai
{
namespace sg
{

const char ALIVE_SVC[] = "ALIVE_SVC";
int const MAX_USRNAME = 127;
int const MAX_HOSTNAME = 127;

// ��kmngr�����������Ϣ����ʽ
struct alive_rqst_t
{
	char usrname[MAX_USRNAME + 1];
	char hostname[MAX_HOSTNAME + 1];
	time_t expire;
	pid_t pid;
};

// ��kmngr����Ӧ�����Ϣ����ʽ
struct alive_rply_t
{
	bool rtn;
};

}
}

#endif

