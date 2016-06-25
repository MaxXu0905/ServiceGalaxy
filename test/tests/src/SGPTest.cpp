#include "gtest/gtest.h"
#include "FileUtils.h"
#include <unistd.h>
#include <stdlib.h>
#include "clt_test.h"
#include <time.h>
#include "sys.h"
#include "NetMessageServer.h"
using namespace ai::test::sg;
using namespace ai::test;
using namespace std;
using namespace testing;
//必须要加 为生成类名使用
#define _TEST_FILE_ sgtest

#define TEMPLATEPATH "conf/sg/"
#define TEMPLATECONFIG "conf/testconfig.conf"
static NetMessageServer &msgsrv = NetMessageServer::instance();
static SGClient4Test &sgclient = SGClient4Test::instance();
enum SGTestType{
	Single=1,
	Other=2,
	TowSide=3
};
SGTestType sgTestType=Single;
class SGEnvironment: public testing::Environment {
public:
	virtual void SetUp() {
		map<string,string> context;
		AttributeProvider attribute(context,TEMPLATECONFIG);
		NetMessageServer::instance().run(atoi(attribute.get("net_server_port").c_str()));
		switch(sgTestType){
			case Single:
				SGClient4Test::instance().sgpack(TEMPLATEPATH"server_sg_temp.xml",true,TEMPLATECONFIG);
				break;
			case Other:
				SGClient4Test::instance().sgpack(TEMPLATEPATH"server_sgm1_temp.xml",true,TEMPLATECONFIG);
				break;
			case TowSide:
				SGClient4Test::instance().sgpack(TEMPLATEPATH"server_sgm2_temp.xml",true,TEMPLATECONFIG);
				break;
		}
		SGClient4Test::instance().startsg();
		NetMessageServer::instance().cleanALL();
	}
	virtual void TearDown() {
		SGClient4Test::instance().stopsg();
		NetMessageServer::instance().cleanALL();
		NetMessageServer::instance().stop();

	}
};
class CharsParmTest: public ::testing::TestWithParam<const char*> {
};


TEST_P3(CharsParmTest,SG平台测试同步调用,不同参数测试) {
	string in = GetParam();
	string out;
	EXPECT_EQ2(0,sgclient.call("TESTSERVICE",in,out,0),调用同步服务)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查返回结果 );
}
INSTANTIATE_TEST_CASE_P3(CharsParmTest, SG平台测试同步调用,Values("meeny", "miny", "moe"));
TEST_P3(CharsParmTest,SG平台测试异步调用,不同参数测试) {
	string in = GetParam();
	string out;
	EXPECT_EQ2(0,sgclient.acallAndRplay("TESTSERVICE",in,out,0),调用异步服务)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查返回结果);
}
INSTANTIATE_TEST_CASE_P3(CharsParmTest, SG平台测试异步调用,Values("meeny", "miny", "moe"));

TEST_P3(CharsParmTest,SG平台测试异步cacel后获取结果,不同参数测试) {
	string in = GetParam();
	int handle;
	msgsrv.clean("servicecallback");
	EXPECT_NE2(0,handle=sgclient.acall("TESTSERVICECALLBACK",in,0),调用异步服务)<<"服务调用失败";
	EXPECT_EQ2(0, sgclient.cancel(handle),取消调用);
	string out1;
	EXPECT_NE2(0, sgclient.getrply(handle, out1,0),检查取消后是否可调用);
	msgsrv.recive("servicecallback",out1,5);
	EXPECT_STREQ2(in.c_str(), out1.c_str(),检查服务端是否接收到请求);

}
INSTANTIATE_TEST_CASE_P3(CharsParmTest, SG平台测试异步cacel后获取结果,Values("meeny", "miny", "moe"));

TEST_P3(CharsParmTest,SG平台测试无反回参数调用,不同参数测试) {
	string in = GetParam();
	msgsrv.clean("servicecallback");
	EXPECT_EQ2(0,sgclient.acallnoreply("TESTSERVICECALLBACK",in),无反回调用)<<"服务调用失败";
	string out;
	msgsrv.recive("servicecallback",out,5);
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务端是否接收到请求);
}
INSTANTIATE_TEST_CASE_P3(CharsParmTest, SG平台测试无反回参数调用,	Values("meeny", "miny", "moe","ttttttttttt"));


TEST3(SG平台测试API,服务超时与取消超时){
		int oldblktime=sgclient.get_blktime();
		EXPECT_TRUE2(sgclient.set_blktime(5),设置超时值);
		EXPECT_EQ2(5,sgclient.get_blktime(),检查超时是否设置正确);
		string out;
	    EXPECT_EQ2(13,sgclient.call("TESTSERVICEADVERTISE","sleep:15",out),测试超时);
	    sgclient.set_blktime(oldblktime);
		int handle;
		EXPECT_TRUE(sgclient.setprio(45));
		EXPECT_NE2(0,handle=sgclient.acall("TESTSERVICE","ttt",0),调用异步服务)<<"服务调用失败";
		EXPECT_EQ2(0, sgclient.cancel(handle),取消调用);
		EXPECT_EQ(95,sgclient.getprio());
}
TEST3(SG平台测试服务发布,发布与取消){
	string out;
	string in="aaaaaaaaaaaaaaaaaaaaaaaaaa";
	EXPECT_EQ2(6,sgclient.call("TESTSERVICE2",in,out),调用未发布服务);

	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","advertise:TESTSERVICE2:svc_test",out),发布服务);
	EXPECT_EQ2(0,sgclient.call("TESTSERVICE2",in,out),调用发布的服务);
	EXPECT_STREQ2(in.c_str(),out.c_str(),检查调用结果);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","unadvertise:TESTSERVICE2",out),取消发布服务);
	EXPECT_EQ2(6,sgclient.call("TESTSERVICE2",in,out),调用未发布服务);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","advertise:TESTSERVICE2:svc_test",out),再次发布服务);
	EXPECT_EQ2(0,sgclient.call("TESTSERVICE2",in,out),调用发布的服务);
	EXPECT_STREQ2(in.c_str(),out.c_str(),检查调用结果);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","unadvertise:TESTSERVICE2",out),再次取消发布服务);
	EXPECT_EQ2(6,sgclient.call("TESTSERVICE2",in,out),调用未发布服务);
}
TEST4(SG平台测试测试sg_svr被重载){
	string out;
	msgsrv.clean("serveroverload");
	string stop="stoped";
	string start="started";
	int groupId=1;
	switch(sgTestType){
			case Single:
				groupId=1;
				break;
			case Other:
				groupId=2;
				break;
			case TowSide:
				groupId=2;
				break;
	}
	EXPECT_EQ2(0,sgclient.stopServer(groupId,430),停止服务)<<"服务调用失败";
	msgsrv.recive("serveroverload",out,5);
	EXPECT_STREQ2(stop.c_str(), out.c_str(),检查服务端停止重载调用);
	msgsrv.clean("serveroverload");
	EXPECT_EQ2(0,sgclient.startServer(groupId,430),启动服务)<<"服务调用失败";
	msgsrv.recive("serveroverload",out,5);
	EXPECT_STREQ2(start.c_str(), out.c_str(),检查服务端起动重载调用);
}
TEST4(SG平台测试svc_fini()函数的各个返回值){
	string out;
	int urcode;
	EXPECT_EQ2(0,sgclient.callurcode("TESTSERVICEADVERTISE","svc_fini:SGSUCCESS",out,urcode),测试SGSUCCESS返回);
	EXPECT_EQ2(77,urcode,检查urcode);
	EXPECT_EQ2(11,sgclient.callurcode("TESTSERVICEADVERTISE","svc_fini:SGFAIL",out,urcode),测试SGFAIL返回);
	EXPECT_EQ2(88,urcode,检查urcode);
	switch(sgTestType){
		case Single:
			EXPECT_EQ2(11,sgclient.callurcode("TESTSERVICEADVERTISE","svc_fini:SGEXIT",out,urcode),测试SGEXIT返回);
			EXPECT_EQ2(99,urcode,检查urcode);
			EXPECT_EQ2(0,Sys::noutsys("./shell/isprocexist TestServerAllInOne "),检查服务是否退出);
			sgclient.fini();
			EXPECT_EQ2(0,Sys::noutsys("./shell/killSG "),强停平台);
			EXPECT_EQ2(0,sgclient.startsg(),再次启动平台)<<"服务启动失败";
			break;
		case Other:
		case TowSide:
			break;
	}

}
TEST4(SG平台测试forward函数){
	string out;
	string in="bbbbbbbbbbbbb";
	msgsrv.clean("servicecallback");
	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","forward:TESTSERVICECALLBACK:"+in,out),测试forward调用);
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查被forward返回结果 );
	out="";
	msgsrv.recive("servicecallback",out,5);
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查被forward是否被调 );
}
TEST4(测试启动顺序号){
	string out;
	EXPECT_EQ2(0,sgclient.stopServer(),停止sg所有服务);
	msgsrv.clean("serversequence");
	EXPECT_EQ2(0,sgclient.startServer(),启动所有服务);
	msgsrv.recive("serversequence",out,5);
	EXPECT_STREQ2("123", out.c_str(),检查启动顺序);
	msgsrv.clean("serversequence");
	EXPECT_EQ2(0,sgclient.stopServer(),停止sg所有服务);
	out="";
	msgsrv.recive("serversequence",out,5);
	EXPECT_STREQ2("321", out.c_str(),检查停止顺序);
	EXPECT_EQ2(0,sgclient.startServer(),再次启动所有服务);
	msgsrv.clean("serversequence");
}
TEST4(测试MSSQ是否工作正常){
	const char* strs[] ={"aaaaaaaaaaa","bbbbbbbbbbbbb","cccccccccccccccccccccc",
			"dddddddddddddddddddddd","eeeeeeeeeeeeeee","ffffffffffffffffffff","ggggggggggggggggggggg","hhhhhhhhhhhhhhhhhhhh","iiiiiiiiiiiiiiiiiiii","jjjjjjjjjjjjjj"};
	const int size=sizeof(strs)/sizeof(char*);
	int handle[size];
	for(int i=0;i<size;i++){
		EXPECT_NE2(0,handle[i]=sgclient.acall("TESTMSSQ",strs[i],0),调用异步服务)<<"服务调用失败";
	}
	//for(int i=0;i<size;i++){
	for(int i=size-1;i>=0;i--){
		string out="";
		EXPECT_EQ2(0, sgclient.getrply(handle[i], out,0),调用异步返回);
		EXPECT_STREQ2(strs[i], out.c_str(),检查返回结果);
	}

}
TEST4(SERVER选项测试) {
	if(sgTestType==Single){
		EXPECT_EQ2(0,sgclient.stopsg(),停止服务);
		EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sg_clopt_temp.xml",true,TEMPLATECONFIG),加载配置文件);
		msgsrv.clean("serverclopt");
		EXPECT_EQ2(0,sgclient.startsg(),启动服务);
		sgclient.fini();
		string appdir = getenv("APPROOT");
		string stdoutlog = appdir + "/TestServerCloptstdout.log";
		string stderrlog = appdir + "/TestServerCloptstderr.log";
		EXPECT_TRUE2(FileUtils::exists(stdoutlog.c_str()), 检查标准输出是否存在);
		EXPECT_TRUE2(FileUtils::contain(stdoutlog.c_str(),"servercloptaaaaaaaaaaa"),检查标准输出内容是否正确);
		FileUtils::del(stdoutlog.c_str());

		EXPECT_TRUE2(FileUtils::exists(stderrlog.c_str()),检查标准输出是否存在);
		EXPECT_TRUE2(FileUtils::contain(stderrlog.c_str(),"servercloptaaaaaaaaaaa"),检查标准输出内容是否正确);
		FileUtils::del(stderrlog.c_str());

		string out;
		msgsrv.recive("serverclopt",out,5);
		EXPECT_STREQ2("--bb=bbbb", out.c_str(),检查自定义参数);

		EXPECT_EQ2(3,Sys::noutsys("./shell/getnice TestServerClopt "),检查nice参数);

		vector<string> ins;
		vector<string> outs;
		vector<int> results;
		vector<int> urcodes;
		ins.push_back("aaaaaaaaaaaaaaaaa");
		ins.push_back("bbbbbbbbbbbbbbbbb");
		ins.push_back("ccccccccccccccccc");
		ins.push_back("ddddddddddddddddd");
		results=sgclient.callnthread("TESTCLOPTSERVICE",ins,outs,urcodes);
		for(int i=0;i<ins.size();i++) {
			EXPECT_EQ2(0, results[i],检查返回码);
			if(!results[i]) {
				EXPECT_STREQ2(ins[i].c_str(), outs[i].c_str(),检查返回结果);
			}
		}
		msgsrv.clean("serverclopt");
		EXPECT_EQ2(0,sgclient.stopsg(),停止服务);
		EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sg_temp.xml",true,TEMPLATECONFIG),加载配置文件);
		EXPECT_EQ2(0,sgclient.startsg(),启动服务);
	}
}
TEST4(ULOG测试) {
	if(sgTestType==Single){
		EXPECT_EQ2(0, sgclient.stopsg(), 停止服务);

		char logpath[256];
		getcwd(logpath, sizeof(logpath));
		string logpathstr(logpath);

		map<string, string> context;
		string ULOG = logpathstr + "/LOG";
		string machinelog = logpathstr + "/machineulog";
		string ulogpfx = logpathstr + "/serverlog";
		char * oldULOGPFX=getenv("SGLOGNAME");
		setenv("SGLOGNAME", ULOG.c_str(), 1);
		struct tm *local;
		time_t t;
		t = time(NULL);
		local = localtime(&t);
		char ULOGfile[256]={0};
		char machinelogfile[256]={0};
		char ulogpfxfile[256]={0};
		const char * time2s = "%s_%d-%02d-%02d.log";
		sprintf(ULOGfile, time2s, ULOG.c_str(), 1900+local->tm_year, local->tm_mon + 1,
				local->tm_mday);
		sprintf(machinelogfile, time2s, machinelog.c_str(), 1900+local->tm_year,
				local->tm_mon + 1, local->tm_mday);
		sprintf(ulogpfxfile, time2s, ulogpfx.c_str(), 1900+local->tm_year, local->tm_mon + 1,
				local->tm_mday);
		context.insert(pair<string, string>("serverlog", ("--ulogpfx="+ulogpfx).c_str()));
		context.insert(pair<string, string>("machineulog", machinelog.c_str()));
		EXPECT_EQ2(0, sgclient.sgpack(TEMPLATEPATH"server_sg_ulogpfx_temp.xml",context,TEMPLATECONFIG), 加载配置文件);
		FileUtils::del(ULOGfile);
		EXPECT_EQ2(0,sgclient.startsg(),启动服务);
		EXPECT_FALSE2(FileUtils::exists(ULOGfile), 检查环境变量配置日志是否存在);

		EXPECT_TRUE2(FileUtils::exists(machinelogfile), 检查主机配置日志是否存在);
		EXPECT_TRUE2(FileUtils::contain(machinelogfile,"ulogtestaaaaaaaaaaaaaaaaaaaaa"),检查主机配置日志是否正确);

		EXPECT_TRUE2(FileUtils::exists(ulogpfxfile), 检查Ulogpfx配置日志是否存在);
		EXPECT_TRUE2(FileUtils::contain(ulogpfxfile,"ulogtestaaaaaaaaaaaaaaaaaaaaa"),检查Ulogpfx配置日志是否正确);

		EXPECT_EQ2(0, sgclient.stopsg(), 停止服务);
		FileUtils::del(ULOGfile);
		FileUtils::del(machinelogfile);
		FileUtils::del(ulogpfxfile);
		context.erase("machineulog");

		EXPECT_EQ2(0, sgclient.sgpack(TEMPLATEPATH"server_sg_ulogpfx_temp.xml",context,TEMPLATECONFIG), 加载配置文件);
		EXPECT_EQ2(0,sgclient.startsg(),启动服务);
		EXPECT_TRUE2(FileUtils::exists(ULOGfile), 检查环境变量配置日志是否存在);
		EXPECT_TRUE2(FileUtils::contain(ULOGfile,"ulogtestaaaaaaaaaaaaaaaaaaaaa"),检查环境变配置日志是否正确);

		EXPECT_FALSE2(FileUtils::exists(machinelogfile), 检查主机配置日志是否存在);

		EXPECT_TRUE2(FileUtils::exists(ulogpfxfile), 检查Ulogpfx配置日志是否存在);
		EXPECT_TRUE2(FileUtils::contain(ulogpfxfile,"ulogtestaaaaaaaaaaaaaaaaaaaaa"),检查Ulogpfx配置日志是否正确);
		EXPECT_EQ2(0, sgclient.stopsg(), 停止服务);
		if(!oldULOGPFX){
			unsetenv("ULOGPFX");
		}else{
			setenv("ULOGPFX", oldULOGPFX, 1);
		}
		FileUtils::del(ULOGfile);
		FileUtils::del(machinelogfile);
		FileUtils::del(ulogpfxfile);
		EXPECT_EQ2(0, sgclient.sgpack(TEMPLATEPATH"server_sg_temp.xml",true,TEMPLATECONFIG), 加载配置文件);
		EXPECT_EQ2(0, sgclient.startsg(), 启动服务);
	}

}
int main(int argc, char **argv) {
	SGEnvironment sgEnviroment;
	//testing::AddGlobalTestEnvironment(&sgEnviroment);
	testing::InitGoogleTest(&argc, argv);
	char * type=getenv("SGTESTTYPE");
	if(type){
		int inttype=atoi(type);
		if(inttype==2){
			sgTestType=Other;
		}else if(inttype==3){
			sgTestType=TowSide;
		}
	}
	sgEnviroment.SetUp();
	int result= RUN_ALL_TESTS();
	sgEnviroment.TearDown();
	return result;
}
