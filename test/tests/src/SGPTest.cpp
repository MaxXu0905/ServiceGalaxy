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
//����Ҫ�� Ϊ��������ʹ��
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


TEST_P3(CharsParmTest,SGƽ̨����ͬ������,��ͬ��������) {
	string in = GetParam();
	string out;
	EXPECT_EQ2(0,sgclient.call("TESTSERVICE",in,out,0),����ͬ������)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),��鷵�ؽ�� );
}
INSTANTIATE_TEST_CASE_P3(CharsParmTest, SGƽ̨����ͬ������,Values("meeny", "miny", "moe"));
TEST_P3(CharsParmTest,SGƽ̨�����첽����,��ͬ��������) {
	string in = GetParam();
	string out;
	EXPECT_EQ2(0,sgclient.acallAndRplay("TESTSERVICE",in,out,0),�����첽����)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),��鷵�ؽ��);
}
INSTANTIATE_TEST_CASE_P3(CharsParmTest, SGƽ̨�����첽����,Values("meeny", "miny", "moe"));

TEST_P3(CharsParmTest,SGƽ̨�����첽cacel���ȡ���,��ͬ��������) {
	string in = GetParam();
	int handle;
	msgsrv.clean("servicecallback");
	EXPECT_NE2(0,handle=sgclient.acall("TESTSERVICECALLBACK",in,0),�����첽����)<<"�������ʧ��";
	EXPECT_EQ2(0, sgclient.cancel(handle),ȡ������);
	string out1;
	EXPECT_NE2(0, sgclient.getrply(handle, out1,0),���ȡ�����Ƿ�ɵ���);
	msgsrv.recive("servicecallback",out1,5);
	EXPECT_STREQ2(in.c_str(), out1.c_str(),��������Ƿ���յ�����);

}
INSTANTIATE_TEST_CASE_P3(CharsParmTest, SGƽ̨�����첽cacel���ȡ���,Values("meeny", "miny", "moe"));

TEST_P3(CharsParmTest,SGƽ̨�����޷��ز�������,��ͬ��������) {
	string in = GetParam();
	msgsrv.clean("servicecallback");
	EXPECT_EQ2(0,sgclient.acallnoreply("TESTSERVICECALLBACK",in),�޷��ص���)<<"�������ʧ��";
	string out;
	msgsrv.recive("servicecallback",out,5);
	EXPECT_STREQ2(in.c_str(), out.c_str(),��������Ƿ���յ�����);
}
INSTANTIATE_TEST_CASE_P3(CharsParmTest, SGƽ̨�����޷��ز�������,	Values("meeny", "miny", "moe","ttttttttttt"));


TEST3(SGƽ̨����API,����ʱ��ȡ����ʱ){
		int oldblktime=sgclient.get_blktime();
		EXPECT_TRUE2(sgclient.set_blktime(5),���ó�ʱֵ);
		EXPECT_EQ2(5,sgclient.get_blktime(),��鳬ʱ�Ƿ�������ȷ);
		string out;
	    EXPECT_EQ2(13,sgclient.call("TESTSERVICEADVERTISE","sleep:15",out),���Գ�ʱ);
	    sgclient.set_blktime(oldblktime);
		int handle;
		EXPECT_TRUE(sgclient.setprio(45));
		EXPECT_NE2(0,handle=sgclient.acall("TESTSERVICE","ttt",0),�����첽����)<<"�������ʧ��";
		EXPECT_EQ2(0, sgclient.cancel(handle),ȡ������);
		EXPECT_EQ(95,sgclient.getprio());
}
TEST3(SGƽ̨���Է��񷢲�,������ȡ��){
	string out;
	string in="aaaaaaaaaaaaaaaaaaaaaaaaaa";
	EXPECT_EQ2(6,sgclient.call("TESTSERVICE2",in,out),����δ��������);

	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","advertise:TESTSERVICE2:svc_test",out),��������);
	EXPECT_EQ2(0,sgclient.call("TESTSERVICE2",in,out),���÷����ķ���);
	EXPECT_STREQ2(in.c_str(),out.c_str(),�����ý��);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","unadvertise:TESTSERVICE2",out),ȡ����������);
	EXPECT_EQ2(6,sgclient.call("TESTSERVICE2",in,out),����δ��������);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","advertise:TESTSERVICE2:svc_test",out),�ٴη�������);
	EXPECT_EQ2(0,sgclient.call("TESTSERVICE2",in,out),���÷����ķ���);
	EXPECT_STREQ2(in.c_str(),out.c_str(),�����ý��);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","unadvertise:TESTSERVICE2",out),�ٴ�ȡ����������);
	EXPECT_EQ2(6,sgclient.call("TESTSERVICE2",in,out),����δ��������);
}
TEST4(SGƽ̨���Բ���sg_svr������){
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
	EXPECT_EQ2(0,sgclient.stopServer(groupId,430),ֹͣ����)<<"�������ʧ��";
	msgsrv.recive("serveroverload",out,5);
	EXPECT_STREQ2(stop.c_str(), out.c_str(),�������ֹͣ���ص���);
	msgsrv.clean("serveroverload");
	EXPECT_EQ2(0,sgclient.startServer(groupId,430),��������)<<"�������ʧ��";
	msgsrv.recive("serveroverload",out,5);
	EXPECT_STREQ2(start.c_str(), out.c_str(),������������ص���);
}
TEST4(SGƽ̨����svc_fini()�����ĸ�������ֵ){
	string out;
	int urcode;
	EXPECT_EQ2(0,sgclient.callurcode("TESTSERVICEADVERTISE","svc_fini:SGSUCCESS",out,urcode),����SGSUCCESS����);
	EXPECT_EQ2(77,urcode,���urcode);
	EXPECT_EQ2(11,sgclient.callurcode("TESTSERVICEADVERTISE","svc_fini:SGFAIL",out,urcode),����SGFAIL����);
	EXPECT_EQ2(88,urcode,���urcode);
	switch(sgTestType){
		case Single:
			EXPECT_EQ2(11,sgclient.callurcode("TESTSERVICEADVERTISE","svc_fini:SGEXIT",out,urcode),����SGEXIT����);
			EXPECT_EQ2(99,urcode,���urcode);
			EXPECT_EQ2(0,Sys::noutsys("./shell/isprocexist TestServerAllInOne "),�������Ƿ��˳�);
			sgclient.fini();
			EXPECT_EQ2(0,Sys::noutsys("./shell/killSG "),ǿͣƽ̨);
			EXPECT_EQ2(0,sgclient.startsg(),�ٴ�����ƽ̨)<<"��������ʧ��";
			break;
		case Other:
		case TowSide:
			break;
	}

}
TEST4(SGƽ̨����forward����){
	string out;
	string in="bbbbbbbbbbbbb";
	msgsrv.clean("servicecallback");
	EXPECT_EQ2(0,sgclient.call("TESTSERVICEADVERTISE","forward:TESTSERVICECALLBACK:"+in,out),����forward����);
	EXPECT_STREQ2(in.c_str(), out.c_str(),��鱻forward���ؽ�� );
	out="";
	msgsrv.recive("servicecallback",out,5);
	EXPECT_STREQ2(in.c_str(), out.c_str(),��鱻forward�Ƿ񱻵� );
}
TEST4(��������˳���){
	string out;
	EXPECT_EQ2(0,sgclient.stopServer(),ֹͣsg���з���);
	msgsrv.clean("serversequence");
	EXPECT_EQ2(0,sgclient.startServer(),�������з���);
	msgsrv.recive("serversequence",out,5);
	EXPECT_STREQ2("123", out.c_str(),�������˳��);
	msgsrv.clean("serversequence");
	EXPECT_EQ2(0,sgclient.stopServer(),ֹͣsg���з���);
	out="";
	msgsrv.recive("serversequence",out,5);
	EXPECT_STREQ2("321", out.c_str(),���ֹͣ˳��);
	EXPECT_EQ2(0,sgclient.startServer(),�ٴ��������з���);
	msgsrv.clean("serversequence");
}
TEST4(����MSSQ�Ƿ�������){
	const char* strs[] ={"aaaaaaaaaaa","bbbbbbbbbbbbb","cccccccccccccccccccccc",
			"dddddddddddddddddddddd","eeeeeeeeeeeeeee","ffffffffffffffffffff","ggggggggggggggggggggg","hhhhhhhhhhhhhhhhhhhh","iiiiiiiiiiiiiiiiiiii","jjjjjjjjjjjjjj"};
	const int size=sizeof(strs)/sizeof(char*);
	int handle[size];
	for(int i=0;i<size;i++){
		EXPECT_NE2(0,handle[i]=sgclient.acall("TESTMSSQ",strs[i],0),�����첽����)<<"�������ʧ��";
	}
	//for(int i=0;i<size;i++){
	for(int i=size-1;i>=0;i--){
		string out="";
		EXPECT_EQ2(0, sgclient.getrply(handle[i], out,0),�����첽����);
		EXPECT_STREQ2(strs[i], out.c_str(),��鷵�ؽ��);
	}

}
TEST4(SERVERѡ�����) {
	if(sgTestType==Single){
		EXPECT_EQ2(0,sgclient.stopsg(),ֹͣ����);
		EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sg_clopt_temp.xml",true,TEMPLATECONFIG),���������ļ�);
		msgsrv.clean("serverclopt");
		EXPECT_EQ2(0,sgclient.startsg(),��������);
		sgclient.fini();
		string appdir = getenv("APPROOT");
		string stdoutlog = appdir + "/TestServerCloptstdout.log";
		string stderrlog = appdir + "/TestServerCloptstderr.log";
		EXPECT_TRUE2(FileUtils::exists(stdoutlog.c_str()), ����׼����Ƿ����);
		EXPECT_TRUE2(FileUtils::contain(stdoutlog.c_str(),"servercloptaaaaaaaaaaa"),����׼��������Ƿ���ȷ);
		FileUtils::del(stdoutlog.c_str());

		EXPECT_TRUE2(FileUtils::exists(stderrlog.c_str()),����׼����Ƿ����);
		EXPECT_TRUE2(FileUtils::contain(stderrlog.c_str(),"servercloptaaaaaaaaaaa"),����׼��������Ƿ���ȷ);
		FileUtils::del(stderrlog.c_str());

		string out;
		msgsrv.recive("serverclopt",out,5);
		EXPECT_STREQ2("--bb=bbbb", out.c_str(),����Զ������);

		EXPECT_EQ2(3,Sys::noutsys("./shell/getnice TestServerClopt "),���nice����);

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
			EXPECT_EQ2(0, results[i],��鷵����);
			if(!results[i]) {
				EXPECT_STREQ2(ins[i].c_str(), outs[i].c_str(),��鷵�ؽ��);
			}
		}
		msgsrv.clean("serverclopt");
		EXPECT_EQ2(0,sgclient.stopsg(),ֹͣ����);
		EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sg_temp.xml",true,TEMPLATECONFIG),���������ļ�);
		EXPECT_EQ2(0,sgclient.startsg(),��������);
	}
}
TEST4(ULOG����) {
	if(sgTestType==Single){
		EXPECT_EQ2(0, sgclient.stopsg(), ֹͣ����);

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
		EXPECT_EQ2(0, sgclient.sgpack(TEMPLATEPATH"server_sg_ulogpfx_temp.xml",context,TEMPLATECONFIG), ���������ļ�);
		FileUtils::del(ULOGfile);
		EXPECT_EQ2(0,sgclient.startsg(),��������);
		EXPECT_FALSE2(FileUtils::exists(ULOGfile), ��黷������������־�Ƿ����);

		EXPECT_TRUE2(FileUtils::exists(machinelogfile), �������������־�Ƿ����);
		EXPECT_TRUE2(FileUtils::contain(machinelogfile,"ulogtestaaaaaaaaaaaaaaaaaaaaa"),�������������־�Ƿ���ȷ);

		EXPECT_TRUE2(FileUtils::exists(ulogpfxfile), ���Ulogpfx������־�Ƿ����);
		EXPECT_TRUE2(FileUtils::contain(ulogpfxfile,"ulogtestaaaaaaaaaaaaaaaaaaaaa"),���Ulogpfx������־�Ƿ���ȷ);

		EXPECT_EQ2(0, sgclient.stopsg(), ֹͣ����);
		FileUtils::del(ULOGfile);
		FileUtils::del(machinelogfile);
		FileUtils::del(ulogpfxfile);
		context.erase("machineulog");

		EXPECT_EQ2(0, sgclient.sgpack(TEMPLATEPATH"server_sg_ulogpfx_temp.xml",context,TEMPLATECONFIG), ���������ļ�);
		EXPECT_EQ2(0,sgclient.startsg(),��������);
		EXPECT_TRUE2(FileUtils::exists(ULOGfile), ��黷������������־�Ƿ����);
		EXPECT_TRUE2(FileUtils::contain(ULOGfile,"ulogtestaaaaaaaaaaaaaaaaaaaaa"),��黷����������־�Ƿ���ȷ);

		EXPECT_FALSE2(FileUtils::exists(machinelogfile), �������������־�Ƿ����);

		EXPECT_TRUE2(FileUtils::exists(ulogpfxfile), ���Ulogpfx������־�Ƿ����);
		EXPECT_TRUE2(FileUtils::contain(ulogpfxfile,"ulogtestaaaaaaaaaaaaaaaaaaaaa"),���Ulogpfx������־�Ƿ���ȷ);
		EXPECT_EQ2(0, sgclient.stopsg(), ֹͣ����);
		if(!oldULOGPFX){
			unsetenv("ULOGPFX");
		}else{
			setenv("ULOGPFX", oldULOGPFX, 1);
		}
		FileUtils::del(ULOGfile);
		FileUtils::del(machinelogfile);
		FileUtils::del(ulogpfxfile);
		EXPECT_EQ2(0, sgclient.sgpack(TEMPLATEPATH"server_sg_temp.xml",true,TEMPLATECONFIG), ���������ļ�);
		EXPECT_EQ2(0, sgclient.startsg(), ��������);
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
