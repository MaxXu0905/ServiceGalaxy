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

TEST4(启动模式测试){
	map<string,string> context;
	AttributeProvider attribute(context,TEMPLATECONFIG);
	string cmd="rm -fr "+attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find*";
	Sys::noutsys(cmd.c_str(),"test.log");
	EXPECT_EQ2(0,SGClient4Test::instance().sgpack(DBCCONFIGPATH"server_sdc_master.xml",context,TEMPLATECONFIG),加载Master配置文件);
	EXPECT_EQ2(0,SGClient4Test::instance().startsg(),启动服务);
	EXPECT_EQ2(0,SGClient4Test::instance().stopsg(),停止服务);
	EXPECT_TRUE2(FileUtils::exists(attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find-0.b"),检查文件是否加载);
	EXPECT_TRUE2(FileUtils::exists(attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find-1.b"),检查文件是否加载);
	cmd="scp  "+attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find-1.b "+attribute.get("PMID2_scpaddr")+":"
			+attribute.get("dbc_data")+"/dat/"+attribute.get("dbc_username")+"/sdc_test_find-1.b";
	Sys::noutsys(cmd.c_str(),"test.log");
	//context["dbc_shm_size"]="104857600";
	EXPECT_EQ2(0,SGClient4Test::instance().sgpack(DBCCONFIGPATH"server_sdc_slave.xml",context,TEMPLATECONFIG),加载Slave配置文件);
	EXPECT_EQ2(0,SGClient4Test::instance().startsg(),启动服务);
	ctx.connect(TEMPLATECONFIG);
	int rowcount=ctx.getdb_count("sdc_test_find");
	ASSERT_GT2(rowcount,0,判断数据行数);
	ASSERT_GT2(ctx.getdbc_count("sdc_test_find"),0,判断数据行数);
	ASSERT_GT2(rowcount,ctx.getdbc_count("sdc_test_find"),测试加载行数);
	ctx.disconnect();
	EXPECT_EQ2(0,SGClient4Test::instance().stopsg(),停止服务);

}
TEST4(sdc_api函数功能测试){
	map<string,string> context;
	context["dbc_shm_size"]="104857600";
	AttributeProvider attribute(context,TEMPLATECONFIG);
	EXPECT_EQ2(0,SGClient4Test::instance().sgpack(DBCCONFIGPATH"server_sdc_slave.xml",context,TEMPLATECONFIG),加载Slave配置文件);
	EXPECT_EQ2(0,SGClient4Test::instance().startsg(),启动服务);
	sdc_api & sdc=sdc_api::instance();
	EXPECT_TRUE2(sdc.connect(attribute.get("dbc_username"), attribute.get("dbc_password")),连接SDC有服务);
	EXPECT_EQ2(4,sdc.get_table("sdc_test_find"),get_table测试);
	EXPECT_EQ2(0,sdc.get_index("sdc_test_find", "sdc_test_find_0"),get_index测试);
	ctx.connect(TEMPLATECONFIG);
	int rowcount=ctx.getdb_count("sdc_test_find");
	t_sdc_test_find * dbdata=new t_sdc_test_find[rowcount];
	EXPECT_EQ2(rowcount,ctx.get_dbdata("sdc_test_find",dbdata,rowcount),获取数据);
	int samrow=0;
	for(int i=0;i<rowcount;i++){
		t_sdc_test_find data2;
		if(sdc.find(4,0,&dbdata[i],&data2)&&t_sdc_test_find::compare_exact0(&dbdata[i],&data2)==0){
			samrow++;
		}
	}
	delete []dbdata;
	EXPECT_EQ2(rowcount,samrow,内容比较结果);
	sdc.disconnect();
	ctx.disconnect();
	EXPECT_EQ2(0,SGClient4Test::instance().stopsg(),停止服务);
}


int main(int argc, char **argv) {
	testing::AddGlobalTestEnvironment(new DBCEnvironment);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
