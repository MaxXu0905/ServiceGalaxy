#include "gtest/gtest.h"
#include "FileUtils.h"
#include <unistd.h>
#include <stdlib.h>
#include "clt_test.h"
#include <time.h>
#include "sys.h"
using namespace ai::test::sg;
using namespace ai::test;
using namespace std;
using namespace testing;
#define _TEST_FILE_ sgrobusttest
#define TEMPLATEPATH "conf/sg/"
#define TEMPLATECONFIG "conf/testconfig.conf"
static SGClient4Test &sgclient = SGClient4Test::instance();

TEST3(不可重启的SERVER异常时的系统清理,单机){
	map<string,string> context;
	context["ser_restart"]="N";
	EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sg_robust_temp.xml",context,TEMPLATECONFIG),加载配置文件);
	EXPECT_EQ2(0,sgclient.startsg(),启动服务);
	string in("tttttttttttt"),out;
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),调用服务)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务是否正常);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE1",in,out,0),迫使服务异常)<<"服务调用失败";
	sleep(10);
	EXPECT_EQ2(0,Sys::noutsys("./shell/isserverexist TestAbortServer1 "),检查服务是否清理);
	EXPECT_EQ2(0,sgclient.stopsg(),停止服务);
}

TEST3(不可重启的SERVER异常时的系统清理,集群){
	map<string,string> context;
	context["ser_restart"]="N";
	EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sgm_robust_temp.xml",context,TEMPLATECONFIG),加载配置文件);
	EXPECT_EQ2(0,sgclient.startsg(),启动服务);
	string in("tttttttttttt"),out;
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),调用服务1)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务1是否正常);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE1",in,out,0),迫使服务1异常)<<"服务调用失败";
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE2",in,out,0),调用服务2)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务2是否正常);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE2",in,out,0),迫使服务2异常)<<"服务调用失败";
	sleep(10);
	EXPECT_EQ2(0,Sys::noutsys("./shell/isserverexist TestAbortServer1 "),检查服务1是否清理);
	EXPECT_EQ2(0,Sys::noutsys("./shell/isserverexist TestAbortServer2 "),检查服务2是否清理);
	EXPECT_EQ2(0,sgclient.stopsg(),停止服务);
}

TEST3(可重启的SERVER异常时的系统清理,单机){
	map<string,string> context;
	context["ser_restart"]="Y";
	EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sg_robust_temp.xml",context,TEMPLATECONFIG),加载配置文件);
	EXPECT_EQ2(0,sgclient.startsg(),启动服务);
	string in("tttttttttttt"),out;
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),调用服务)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务是否正常);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE1",in,out,0),迫使服务异常)<<"服务调用失败";
	sleep(10);
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),调用服务)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务是否正常);
	EXPECT_EQ2(0,sgclient.stopsg(),停止服务);
}

TEST3(可重启的SERVER异常时的系统清理,集群){
	map<string,string> context;
	context["ser_restart"]="Y";
	EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sgm_robust_temp.xml",context,TEMPLATECONFIG),加载配置文件);
	EXPECT_EQ2(0,sgclient.startsg(),启动服务);
	string in("tttttttttttt"),out;
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),调用服务1)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务1是否正常);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE1",in,out,0),迫使服务1异常)<<"服务调用失败";
	in="xxxxxxxxxxx";
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE2",in,out,0),调用服务2)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务2是否正常);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE2",in,out,0),迫使服务2异常)<<"服务调用失败";
	sleep(10);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),调用服务1)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务1是否正常);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE2",in,out,0),调用服务2)<<"服务调用失败";
	EXPECT_STREQ2(in.c_str(), out.c_str(),检查服务2是否正常);
	EXPECT_EQ2(0,sgclient.stopsg(),停止服务);
}
int main(int argc, char **argv) {

	testing::InitGoogleTest(&argc, argv);
	int result= RUN_ALL_TESTS();
	return result;
}
