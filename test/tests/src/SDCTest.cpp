#include "gtest/gtest.h"
#include "DBCTestContext.h"
#include "FileUtils.h"
#include "clt_test.h"
#include "sys.h"
#include "sdc_api.h"
using namespace testing;
using namespace ai::test;
using namespace ai::test::dbc;
using namespace ai::test::sg;
#define _TEST_FILE_ dbctest
#define DBCCONFIGPATH "conf/dbc/"
#define TEMPLATECONFIG "conf/testconfig.conf"
DBCTest_Context ctx;
class DBCEnvironment: public testing::Environment {
public:
	virtual void SetUp() {
		map<string,string> context;
		AttributeProvider attribute(context,TEMPLATECONFIG);
		FileUtils::genfile(DBCCONFIGPATH"metarep.xml",attribute.get("metarep_config"),context,TEMPLATECONFIG);
		FileUtils::genfile(DBCCONFIGPATH"sdc.xml",attribute.get("sdc_config"),context,TEMPLATECONFIG);
		FileUtils::genfile(DBCCONFIGPATH"dbc.xml",attribute.get("dbc_config"),context,TEMPLATECONFIG);
	}
	virtual void TearDown(){

	}
};

TEST4(����ģʽ����){
	map<string,string> context;
	AttributeProvider attribute(context,TEMPLATECONFIG);
	string cmd="rm -fr "+attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find*";
	Sys::noutsys(cmd.c_str(),"test.log");
	EXPECT_EQ2(0,SGClient4Test::instance().sgpack(DBCCONFIGPATH"server_sdc_master.xml",context,TEMPLATECONFIG),����Master�����ļ�);
	EXPECT_EQ2(0,SGClient4Test::instance().startsg(),��������);
	EXPECT_EQ2(0,SGClient4Test::instance().stopsg(),ֹͣ����);
	EXPECT_TRUE2(FileUtils::exists(attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find-0.b"),����ļ��Ƿ����);
	EXPECT_TRUE2(FileUtils::exists(attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find-1.b"),����ļ��Ƿ����);
	cmd="scp  "+attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find-1.b "+attribute.get("PMID2_scpaddr")+":"
			+attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find-1.b";
	Sys::noutsys(cmd.c_str(),"test.log");
	//context["dbc_shm_size"]="104857600";
	EXPECT_EQ2(0,SGClient4Test::instance().sgpack(DBCCONFIGPATH"server_sdc_slave.xml",context,TEMPLATECONFIG),����Slave�����ļ�);
	EXPECT_EQ2(0,SGClient4Test::instance().startsg(),��������);
	ctx.connect(TEMPLATECONFIG);
	int rowcount=ctx.getdb_count("sdc_test_find");
	ASSERT_GT2(rowcount,0,�ж���������);
	ASSERT_GT2(ctx.getdbc_count("sdc_test_find"),0,�ж���������);
	ASSERT_GT2(rowcount,ctx.getdbc_count("sdc_test_find"),���Լ�������);
	ctx.disconnect();
	EXPECT_EQ2(0,SGClient4Test::instance().stopsg(),ֹͣ����);

}
TEST4(sdc_api�������ܲ���){
	map<string,string> context;
	context["dbc_shm_size"]="104857600";
	AttributeProvider attribute(context,TEMPLATECONFIG);
	EXPECT_EQ2(0,SGClient4Test::instance().sgpack(DBCCONFIGPATH"server_sdc_slave.xml",context,TEMPLATECONFIG),����Slave�����ļ�);
	EXPECT_EQ2(0,SGClient4Test::instance().startsg(),��������);
	sdc_api & sdc=sdc_api::instance();
	EXPECT_TRUE2(sdc.connect(attribute.get("dbc_username"), attribute.get("dbc_password")),����SDC�з���);
	EXPECT_EQ2(4,sdc.get_table("sdc_test_find"),get_table����);
	EXPECT_EQ2(0,sdc.get_index("sdc_test_find", "sdc_test_find_0"),get_index����);
	ctx.connect(TEMPLATECONFIG);
	int rowcount=ctx.getdb_count("sdc_test_find");
	t_sdc_test_find * dbdata=new t_sdc_test_find[rowcount];
	EXPECT_EQ2(rowcount,ctx.get_dbdata("sdc_test_find",dbdata,rowcount),��ȡ����);
	int samrow=0;
	for(int i=0;i<rowcount;i++){
		t_sdc_test_find data2;
		if(sdc.find(4,0,&dbdata[i],&data2)&&t_sdc_test_find::compare_exact0(&dbdata[i],&data2)==0){
			samrow++;
		}
	}
	delete []dbdata;
	EXPECT_EQ2(rowcount,samrow,���ݱȽϽ��);
	sdc.disconnect();
	ctx.disconnect();
	EXPECT_EQ2(0,SGClient4Test::instance().stopsg(),ֹͣ����);
}


int main(int argc, char **argv) {
	testing::AddGlobalTestEnvironment(new DBCEnvironment);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
