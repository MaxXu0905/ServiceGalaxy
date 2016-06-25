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

TEST3(����������SERVER�쳣ʱ��ϵͳ����,����){
	map<string,string> context;
	context["ser_restart"]="N";
	EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sg_robust_temp.xml",context,TEMPLATECONFIG),���������ļ�);
	EXPECT_EQ2(0,sgclient.startsg(),��������);
	string in("tttttttttttt"),out;
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),���÷���)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),�������Ƿ�����);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE1",in,out,0),��ʹ�����쳣)<<"�������ʧ��";
	sleep(10);
	EXPECT_EQ2(0,Sys::noutsys("./shell/isserverexist TestAbortServer1 "),�������Ƿ�����);
	EXPECT_EQ2(0,sgclient.stopsg(),ֹͣ����);
}

TEST3(����������SERVER�쳣ʱ��ϵͳ����,��Ⱥ){
	map<string,string> context;
	context["ser_restart"]="N";
	EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sgm_robust_temp.xml",context,TEMPLATECONFIG),���������ļ�);
	EXPECT_EQ2(0,sgclient.startsg(),��������);
	string in("tttttttttttt"),out;
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),���÷���1)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),������1�Ƿ�����);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE1",in,out,0),��ʹ����1�쳣)<<"�������ʧ��";
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE2",in,out,0),���÷���2)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),������2�Ƿ�����);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE2",in,out,0),��ʹ����2�쳣)<<"�������ʧ��";
	sleep(10);
	EXPECT_EQ2(0,Sys::noutsys("./shell/isserverexist TestAbortServer1 "),������1�Ƿ�����);
	EXPECT_EQ2(0,Sys::noutsys("./shell/isserverexist TestAbortServer2 "),������2�Ƿ�����);
	EXPECT_EQ2(0,sgclient.stopsg(),ֹͣ����);
}

TEST3(��������SERVER�쳣ʱ��ϵͳ����,����){
	map<string,string> context;
	context["ser_restart"]="Y";
	EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sg_robust_temp.xml",context,TEMPLATECONFIG),���������ļ�);
	EXPECT_EQ2(0,sgclient.startsg(),��������);
	string in("tttttttttttt"),out;
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),���÷���)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),�������Ƿ�����);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE1",in,out,0),��ʹ�����쳣)<<"�������ʧ��";
	sleep(10);
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),���÷���)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),�������Ƿ�����);
	EXPECT_EQ2(0,sgclient.stopsg(),ֹͣ����);
}

TEST3(��������SERVER�쳣ʱ��ϵͳ����,��Ⱥ){
	map<string,string> context;
	context["ser_restart"]="Y";
	EXPECT_EQ2(0,sgclient.sgpack(TEMPLATEPATH"server_sgm_robust_temp.xml",context,TEMPLATECONFIG),���������ļ�);
	EXPECT_EQ2(0,sgclient.startsg(),��������);
	string in("tttttttttttt"),out;
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),���÷���1)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),������1�Ƿ�����);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE1",in,out,0),��ʹ����1�쳣)<<"�������ʧ��";
	in="xxxxxxxxxxx";
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE2",in,out,0),���÷���2)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),������2�Ƿ�����);
	EXPECT_LT2(0,sgclient.call("TESTABORTSERVICE2",in,out,0),��ʹ����2�쳣)<<"�������ʧ��";
	sleep(10);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE1",in,out,0),���÷���1)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),������1�Ƿ�����);
	out="";
	EXPECT_EQ2(0,sgclient.call("TESTECHOSERVICE2",in,out,0),���÷���2)<<"�������ʧ��";
	EXPECT_STREQ2(in.c_str(), out.c_str(),������2�Ƿ�����);
	EXPECT_EQ2(0,sgclient.stopsg(),ֹͣ����);
}
int main(int argc, char **argv) {

	testing::InitGoogleTest(&argc, argv);
	int result= RUN_ALL_TESTS();
	return result;
}
