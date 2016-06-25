#include <gtest/gtest.h>
#include <iostream>
#include "clt_test.h"
#include "bps_api.h"

#define _TEST_FILE_ testtitle
#define TEMPLATEPATH "conf/component/"
//g++ -I../include -L../lib -lgtest testtemplate.cpp
using namespace std;
using namespace testing;
using namespace ai::test::sg;
using namespace ai::app;

//static SGClient4Test &sgclient = SGClient4Test::instance();

class MyEnvironment: public testing::Environment {
public:
	virtual void SetUp() {
		   cout<<(boost::format("MyEnvironment setup")).str()<<endl;
	}
	virtual void TearDown() {
		cout<<(boost::format("MyEnvironment TearDown")).str()<<endl;
	}
};
class CaseEnviroment:public Test{
public:
	  static void SetUpTestCase() {
		SGClient4Test::instance().sgpack(TEMPLATEPATH"server_bps_basic.xml");
		SGClient4Test::instance().startsg();
	}

	static void TearDownTestCase() {
		SGClient4Test::instance().stopsg();
	}
protected:
	  virtual void SetUp(){
	  }

	  virtual void TearDown(){
	  }
};

class Case1Enviroment:public Test{
public:
	  static void SetUpTestCase() {
		SGClient4Test::instance().sgpack(TEMPLATEPATH"server_bps_basic_1.xml");
		SGClient4Test::instance().startsg();
	}

	static void TearDownTestCase() {
		SGClient4Test::instance().stopsg();
	}
protected:
	  virtual void SetUp(){
	  }

	  virtual void TearDown(){
	  }
};

class Case2Enviroment:public Test{
public:
	  static void SetUpTestCase() {
		SGClient4Test::instance().sgpack(TEMPLATEPATH"server_bps_basic_2.xml");
		SGClient4Test::instance().startsg();
	}

	static void TearDownTestCase() {
		SGClient4Test::instance().stopsg();
	}
protected:
	  virtual void SetUp(){
	  }

	  virtual void TearDown(){
	  }
};

class Case3Enviroment:public Test{
public:
	  static void SetUpTestCase() {
		SGClient4Test::instance().sgpack(TEMPLATEPATH"server_bps_basic_3.xml");
		SGClient4Test::instance().startsg();
	}

	static void TearDownTestCase() {
		SGClient4Test::instance().stopsg();
	}
protected:
	  virtual void SetUp(){
	  }

	  virtual void TearDown(){
	  }
};

TEST_F3(CaseEnviroment, ��¼ʶ����ת��,�׼�¼����){
	//����ͷ��¼������bps�����õ�ͷ��¼��TOTAL_COUNT=�������
	string record = "TOTAL_COUNT=1111";
	api_bps_rqst_t in = {0, 224, 30, string(224,'\0'), record };
	api_bps_rply_t out;
	bps_api bps_mgr;
	ASSERT_TRUE(bps_mgr.init("BPS" )) ;
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_STREQ2(record.c_str()+12, out.out_str.c_str()+82, ��鷵���ַ���);
}

TEST_F3(CaseEnviroment,��¼ʶ����ת��,���¼����){
	//�������¼������bps�����õ����¼��ԭ������
	string record=string(82,'\0');
	record.insert(0,"36");
	record.insert(7,"360");
	record.insert(14,"360001");
	record.insert(21,"123456");
	record.insert(38,"18601019876");
	record.insert(79,"44");

	api_bps_rqst_t in = {1, 224, 82, string(224,'\0'), record };
	api_bps_rply_t out;
	bps_api bps_mgr;
	ASSERT_TRUE(bps_mgr.init("BPS" )) ;
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_TRUE2(out.out_str.compare(0,82,record), ��鷵���ַ���);
}

TEST_F3(CaseEnviroment, ��¼ʶ����ת��,β��¼����){
	//����β��¼������bps�����õ�ͷ��¼��END_TIME=����ַ���
	string record="END_TIME=20120420151820";
	api_bps_rqst_t in = {2, 224, 30, string(224,'\0'), record };
	api_bps_rply_t out;
	bps_api bps_mgr;
	ASSERT_TRUE(bps_mgr.init("BPS" )) ;
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_STREQ2(record.c_str()+9, out.out_str.c_str()+93, ��鷵���ַ���);
}

TEST_F3(CaseEnviroment, ȫ�ֱ�������,�ڲ�ȫ�ֱ���){
	string record="END_TIME=20120420151820";
	string global=string(224,'\0');
	global.insert(86,"20120420160000");//��ʼ���ڲ�ȫ�ֱ���sysdate
	api_bps_rqst_t in = {2, 224, 30, global, record };
	api_bps_rply_t out;
	bps_api bps_mgr;
	ASSERT_TRUE(bps_mgr.init("BPS" )) ;
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_STREQ2(global.c_str()+86, out.out_str.c_str()+93, ��鷵���ַ���);
	global.insert(86,"20120420120000");//��ʼ���ڲ�ȫ�ֱ���sysdate
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_STREQ2(record.c_str()+9, out.global_str.c_str()+86, ��鷵���ַ���);
}


TEST_F3(Case1Enviroment, ȫ�ֱ�������1,�Զ���ȫ�ֱ�������){
	string record=string(82,'\0');
	record.insert(0,"36");
	record.insert(7,"360");
	record.insert(14,"360001");
	record.insert(21,"123456");
	record.insert(38,"18601019876");
	record.insert(79,"44");
	string global=string(224,'\0');
	api_bps_rqst_t in = {1, 224, 30, global, record };
	api_bps_rply_t out;
	bps_api bps_mgr;
	ASSERT_TRUE(bps_mgr.init("BPS_1" )) ;
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_STREQ2("0123456789", out.out_str.c_str()+34, ��鷵���ַ���);
}

/*
TEST_F3(Case2Enviroment, ����ֵ����1,���ص���������){
	//�������¼������bps�����õ����¼��ԭ������
	string record=string(82,'\0');
	record.insert(0,"36");
	record.insert(7,"360");
	record.insert(14,"360001");
	record.insert(21,"123456");
	record.insert(38,"18601019876");
	record.insert(79,"44");

	api_bps_rqst_t in = {1, 224, 82, string(224,'\0'), record };
	api_bps_rply_t out;
	bps_api bps_mgr;
	ASSERT_TRUE(bps_mgr.init("BPS_2" )) ;
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_EQ2(0, bps_mgr.get_errors(), ��ȡ���ش�����);
	EXPECT_TRUE2(out.out_str.compare(0,82,record), ��鷵���ַ���);
}

TEST_F3(Case3Enviroment, ����ֵ����2,���ض��������){
	//�������¼������bps�����õ����¼��ԭ������
	string record=string(82,'\0');
	record.insert(0,"36");
	record.insert(7,"360");
	record.insert(14,"360001");
	record.insert(21,"123456");
	record.insert(38,"18601019876");
	record.insert(79,"44");

	api_bps_rqst_t in = {1, 224, 82, string(224,'\0'), record };
	api_bps_rply_t out;
	bps_api bps_mgr;
	ASSERT_TRUE(bps_mgr.init("BPS_3" )) ;
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_EQ2(0, bps_mgr.get_errors(), ��ȡ���ش�����);
	EXPECT_TRUE2(out.out_str.compare(0,82,record), ��鷵���ַ���);
}


TEST_F3(CaseEnviroment, �����ڴ����){
	string record="END_TIME=20120420151820";
	string global=string(224,'\0');
	global.insert(86,"20120420160000");//��ʼ���ڲ�ȫ�ֱ���sysdate
	api_bps_rqst_t in = {2, 224, 30, global, record };
	api_bps_rply_t out;
	bps_api bps_mgr;
	ASSERT_TRUE(bps_mgr.init("BPS" )) ;
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_STREQ2(global.c_str()+86, out.out_str.c_str()+93, ��鷵���ַ���);
	global.insert(86,"20120420120000");//��ʼ���ڲ�ȫ�ֱ���sysdate
	EXPECT_EQ2(0, bps_mgr.execute(in, out), �����ý��);
	EXPECT_STREQ2(record.c_str()+9, out.global_str.c_str()+86, ��鷵���ַ���);
}
*/

int main(int argc,char * argv[]){
//	AddGlobalTestEnvironment(new MyEnvironment);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
