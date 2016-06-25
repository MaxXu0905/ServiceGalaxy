#include <string>
#include <vector>
#include <map>
namespace ai
{
namespace test
{
namespace sg
{
using namespace ::std;

class SGClient4Test {
public:
	static SGClient4Test & instance();
	virtual ~SGClient4Test();
	SGClient4Test();
	virtual bool init()=0;
	virtual void fini()=0;
	virtual int acallAndRplay(const char *service, const string in,
			string& out, int flags) =0;
	virtual int acall(const char *service, const string in, int flags=0) =0;
	virtual int acallnoreply(const char *service, const string in) =0;
	virtual int cancel(int handle)=0;
	virtual int getrply(int handle, string& out, int flags)=0;
	virtual int call(const char *service, const string in, string& out,
			int flags=0) =0;
	virtual vector<int> callnthread(const char *service, const vector<string> &ins,
			vector<string>& outs,vector<int> &urcodes, int flags=0)=0;
	virtual int callurcode(const char *service, const string in, string& out,int &urcode,
				int flags=0) =0;
	virtual bool set_blktime(int blktime, int flags=0)=0;
	virtual int get_blktime(int flags=0)=0;
	virtual int getprio()=0;
	virtual bool setprio(int prio, int flags=0)=0;
	virtual int startServer(int32_t groupId=-1, int32_t serverId=-1) =0;
	virtual int stopServer(int32_t groupId=-1, int32_t serverId=-1) =0;
	virtual int startsg()=0;
	virtual int stopsg()=0;
	virtual int startadmin()=0;
	virtual int stopadmin()=0;
	virtual int sgpack(const string filename,bool istemp=false,const string configfile="")=0;
	virtual int sgpack(const string filename,map<string,string> &context,const string configfile="")=0;

};
}
}
}
