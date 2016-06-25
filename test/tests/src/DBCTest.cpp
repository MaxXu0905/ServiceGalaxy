#include "gtest/gtest.h"
#include "DBCTestContext.h"
#include "test_manager.h"
#include "test_thread.h"
#include "test_action.h"
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
		ctx.loadConfig(DBCCONFIGPATH"dbc.xml",TEMPLATECONFIG);
	}
	virtual void TearDown() {
		ctx.destory();
	}
};
TEST4(大表情况下的数据加载测试) {
	map<string,string> context;
	context["dbc_test_bigload_loadtype"]="DB BFILE";
	EXPECT_TRUE2(ctx.loadConfig(DBCCONFIGPATH"dbc.xml",TEMPLATECONFIG,context),加载大表数据);
	int rowcount=ctx.getdb_count("dbc_test_bigload");
	ASSERT_GT2(rowcount,0,判断数据行数);
	EXPECT_EQ2(rowcount,ctx.getdbc_count("dbc_test_bigload"),测试加载行数);
	EXPECT_EQ2(rowcount>10000?10000:rowcount, ctx.sampleCompare<t_dbc_test_bigload>("dbc_test_bigload",0,rowcount>10000?10000:rowcount),抽样比较内容);
}
TEST4(三种数据加载方式测试){
	int rowcount;
	map<string,string> context;
	context["dbc_test_load_loadtype"]="DB";
	EXPECT_TRUE2(ctx.loadConfig(DBCCONFIGPATH"dbc.xml",TEMPLATECONFIG,context),从数据库加载数据);
	rowcount=ctx.getdb_count("dbc_test_load");
	ASSERT_GT2(rowcount,0,判断数据行数);
	EXPECT_EQ2(rowcount,ctx.getdbc_count("dbc_test_load"),测试加载行数);
	EXPECT_EQ2(rowcount, ctx.sampleCompare<t_dbc_test_load>("dbc_test_load",0,rowcount),抽样比较内容);
	context["dbc_test_load_loadtype"]="BFILE";
	EXPECT_TRUE2(ctx.loadConfig(DBCCONFIGPATH"dbc.xml",TEMPLATECONFIG,context),从二进制文件加载数据);

	EXPECT_EQ2(rowcount,ctx.getdbc_count("dbc_test_load"),测试加载行数);
	EXPECT_EQ2(rowcount, ctx.sampleCompare<t_dbc_test_load>("dbc_test_load",0,rowcount),抽样比较内容);

	context["dbc_test_load_loadtype"]="FILE";
	ctx.loadFILE(DBCCONFIGPATH"staticlen.data",TEMPLATECONFIG);
	EXPECT_TRUE2(ctx.loadConfig(DBCCONFIGPATH"dbc.xml",TEMPLATECONFIG,context),从定长文本文件加载数据);
	EXPECT_EQ2(rowcount,ctx.getdbc_count("dbc_test_load"),测试加载行数);
	EXPECT_EQ2(rowcount, ctx.sampleCompare<t_dbc_test_load>("dbc_test_load",0,rowcount),抽样比较内容);
	context["isdelimiter"]="<delimiter>,</delimiter>";
	ctx.loadFILE(DBCCONFIGPATH"separator.data",TEMPLATECONFIG);
	EXPECT_TRUE2(ctx.loadConfig(DBCCONFIGPATH"dbc.xml",TEMPLATECONFIG,context),从分隔符文本文件加载数据);
	EXPECT_EQ2(rowcount,ctx.getdbc_count("dbc_test_load"),测试加载行数);
	EXPECT_EQ2(rowcount, ctx.sampleCompare<t_dbc_test_load>("dbc_test_load",0,rowcount),抽样比较内容);
	ctx.loadConfig(DBCCONFIGPATH"dbc.xml",TEMPLATECONFIG);
}
TEST4(索引方式测试){
	int rowcount=ctx.getdb_count("dbc_test_load");
	ASSERT_GT2(rowcount,0,判断数据行数);
	EXPECT_EQ2(rowcount, ctx.sampleCompare<t_dbc_test_load>("dbc_test_load",1,rowcount),顺序查找);
	EXPECT_EQ2(rowcount, ctx.sampleCompare<t_dbc_test_load>("dbc_test_load",2,rowcount),二分查找);
	EXPECT_EQ2(rowcount, ctx.sampleCompare<t_dbc_test_load>("dbc_test_load",3,rowcount),哈希+顺序查找);
	EXPECT_EQ2(rowcount, ctx.sampleCompare<t_dbc_test_load>("dbc_test_load",4,rowcount),哈希+二分查找);
}
struct IndexTypeParm{
	IndexTypeParm(int intvalue,
			const string& intime="1970-01-02",const string& outtime="2018-01-01"):intime(intime),outtime(outtime),intvalue(intvalue){
		ostringstream where;
		where<<"test_int_t="<<intvalue;
		normalwhere=where.str();
		where<<" and test_date_t<=to_date('"<<intime
			<<"','yyyy-mm-dd') and test_date_t2>to_date('"
			<<intime<<"','yyyy-mm-dd')";

		inscopewhere=where.str();
		ostringstream where2;
		where2<<"test_int_t="<<intvalue
			<<" and test_date_t<=to_date('"<<outtime
			<<"','yyyy-mm-dd') and test_date_t2>to_date('"
			<<outtime<<"','yyyy-mm-dd')";

		outscopewhere=where2.str();
		struct tm tm;
		strptime(intime.c_str(), "%Y-%m-%d", &tm);
		inscopevalue=mktime(&tm);
		strptime(outtime.c_str(), "%Y-%m-%d", &tm);
		outscopevalue=mktime(&tm);
	}

	string normalwhere;
	string inscopewhere;
	string outscopewhere;
	string intime;
	string outtime;
	int intvalue;
	time_t inscopevalue;
	time_t outscopevalue;

};
std::ostream & operator  <<(std::ostream &stream,const IndexTypeParm & parm){
		return stream<<"顺序号="<<parm.intvalue<<":范围内时间="
				<<parm.intime<<":范围外时间=" <<parm.outtime;
}

class CharsParmTest: public ::testing::TestWithParam<IndexTypeParm> {

};
TEST_P4(CharsParmTest,索引类型测试){
	const IndexTypeParm& parm= GetParam();
	t_dbc_test_load data;
	data.test_int_t=parm.intvalue;
	int rowcount=ctx.getdb_count("dbc_test_load",parm.normalwhere);
	ASSERT_GT2(rowcount,0,判断Normal数据行数);
	EXPECT_EQ2(rowcount,ctx.sampleCompare(data,"dbc_test_load",5,rowcount),Normal类型测试);
	rowcount=ctx.getdb_count("dbc_test_load",parm.inscopewhere);
	ASSERT_GT2(rowcount,0,判断选定时间范围数据行数);
	data.test_date_t=parm.inscopevalue;
	EXPECT_EQ2(rowcount,ctx.sampleCompare(data,"dbc_test_load",6,rowcount),选定时间范围测试);
	rowcount=ctx.getdb_count("dbc_test_load",parm.outscopewhere);
	data.test_date_t=parm.outscopevalue;
	EXPECT_EQ2(rowcount,ctx.sampleCompare(data,"dbc_test_load",6,rowcount),未选定时间范围测试);

}

INSTANTIATE_TEST_CASE_P3(CharsParmTest, 索引类型测试,
		Values(IndexTypeParm(1),IndexTypeParm(2), IndexTypeParm(3),IndexTypeParm(4),
			IndexTypeParm(5),IndexTypeParm(6), IndexTypeParm(7),IndexTypeParm(8),IndexTypeParm(9),IndexTypeParm(10))
		);

TEST4(dbc_api函数功能测试){
	dbc_api *db=&dbc_api::instance();
	EXPECT_EQ2(3,db->get_table("dbc_test_apicalL"),get_table测试);
	EXPECT_EQ2(0,db->get_index("dbc_test_apicall", "dbc_test_apicalL_0"),get_index测试);
	t_dbc_test_apicall datas[]={
			{"aaaaaaaaaaa1",1},
			{"aaaaaaaaaaa2",2},
			{"aaaaaaaaaaa3",3},
			{"aaaaaaaaaaa4",4},
			{"aaaaaaaaaaa5",4},
			{"aaaaaaaaaaa6",4}
	};
	//bool insert(int table_id, const void *data);
	EXPECT_TRUE2(db->insert(3,&datas[0]),插入数据);
	for(int i=1;i<6;i++){
		db->insert(3,&datas[i]);
	}
	EXPECT_EQ2(6,ctx.getdbc_count("dbc_test_apicall"),判断插入的行数);
	t_dbc_test_apicall data={"aaaaaaaaaaa1",1};
	auto_ptr<dbc_cursor> find_ptr;
//	long find(int table_id, const void *data, void *result, long max_rows = 1);
//	long find(int table_id, int index_id, const void *data, void *result, long max_rows = 1);
	t_dbc_test_apicall find1;
	EXPECT_EQ2(1,db->find(3,&data,&find1),测试普通find);
	EXPECT_EQ2(1,find1.test_int_t,测试普通find反回内容);
	data.test_int_t=4;
	t_dbc_test_apicall fined4[4];
	EXPECT_EQ2(3,db->find(3,1,&data,fined4,3),测试索引find);
	EXPECT_EQ2(4,fined4[0].test_int_t,测试索引find反回内容);
//	auto_ptr<dbc_cursor> find(int table_id, const void *data);
//	auto_ptr<dbc_cursor> find(int table_id, int index_id, const void *data);
//	long find_by_rowid(int table_id, long rowid, void *result, long max_rows = 1);
	//auto_ptr<dbc_cursor> find_by_rowid(int table_id, long rowid);
	data.test_int_t=1;
	find_ptr=db->find(3,&data);
	EXPECT_TRUE2(find_ptr->next(),测试普通游标find);
	EXPECT_EQ2(1,((t_dbc_test_apicall *)find_ptr->data())->test_int_t,测试普通游标find内容);
	data.test_int_t=4;
	find_ptr=db->find(3,1,&data);
	EXPECT_TRUE2(find_ptr->next(),测试索引游标find);
	EXPECT_EQ2(4,((t_dbc_test_apicall *)find_ptr->data())->test_int_t,测试索引游标find内容);

	data.test_int_t=1;
	EXPECT_EQ2(1,db->find_by_rowid(3,find_ptr->rowid(),&find1),find_by_rowid测试);
	EXPECT_EQ2(4,find1.test_int_t,find_by_rowid内容);

	auto_ptr<dbc_cursor> find_ptr2=db->find_by_rowid(3, find_ptr->rowid());
	EXPECT_TRUE2(find_ptr2->next(),游标find_by_rowid测试);
	EXPECT_EQ2(4,((t_dbc_test_apicall *)find_ptr2->data())->test_int_t,测试游标find_by_rowid内容);

//	long update(int table_id, const void *old_data, const void *new_data);
//	long update(int table_id, int index_id, const void *old_data, const void *new_data);
//	long update_by_rowid(int table_id, long rowid, const void *new_data);
	data.test_int_t=1;
	t_dbc_test_apicall new_data={"cccccccccc1",1};
	EXPECT_EQ2(1,db->update(3,&data,&new_data),测试普通update);
	find_ptr=db->find(3,&new_data);
	EXPECT_TRUE2(find_ptr->next(),检查更新行数);
	EXPECT_STREQ2(new_data.id,((t_dbc_test_apicall *)find_ptr->data())->id,检查更新的内容);

	EXPECT_EQ2(1,db->update_by_rowid(3,find_ptr->rowid(),&data),测试update_by_rowid);

	db->find(3,&data,&new_data);
	EXPECT_STREQ2(new_data.id,data.id,检查update_by_rowid更新的内容);
	new_data.test_int_t=5;
	EXPECT_EQ2(1,db->update(3,0,&data,&new_data),测试索引update);
	db->find(3,&new_data,&data);
	EXPECT_EQ2(5,data.test_int_t,测试索引update内容);

	EXPECT_EQ2(1,db->erase(3,&datas[2]),测试普通删除);
	EXPECT_EQ2(3,db->erase(3,1,&datas[3]),测试索引删除);
	db->find(3,&new_data,&data);
	EXPECT_TRUE2(db->erase_by_rowid(3, db->get_rowid(1)),测试erase_by_rowid);
	db->commit();
	//EXPECT_TRUE2(db->truncate("dbc_test_apicall"),测试truncate);
	//EXPECT_TRUE2(db->truncate("dbc_test_apicall"),测试truncate);
	//EXPECT_EQ2(0,ctx.getdbc_count("dbc_test_apicall"),检查truncate结果);

	for(int i=1;i<8;i++){
		new_data.test_int_t=i;
		db->erase(3,1,&new_data);
	}
	db->commit();
	EXPECT_EQ2(0,ctx.getdbc_count("dbc_test_apicall"),检查truncate结果);



}
class insertSetter:public DBTemplate::RowSetter{
	public:
		insertSetter():data_(0),count_(0){}
		void set(Generic_Statement * stmt){
			for(int i=0;i<count_;i++){
				if(i){
					stmt->addIteration();
				}
				stmt->setString(1, data_[i].id);
				stmt->setInt(2,data_[i].test_int_t);
			}
		}
		void setdata(t_dbc_test_apicall * data,int count){
			this->data_=data;
			this->count_=count;
		}
	private:
		t_dbc_test_apicall * data_;
		int count_;
	};
TEST3(DBC结合SCCI功能测试,动态方式){
	insertSetter inSetter;
	t_dbc_test_apicall datas1[]={
			{"aaaaaaaaaaa1",1},
	};
	t_dbc_test_apicall datas2[]={
			{"aaaaaaaaaaa2",2},
			{"aaaaaaaaaaa3",3},
			{"aaaaaaaaaaa4",4},
			{"aaaaaaaaaaa5",4},
			{"aaaaaaaaaaa6",4}
	};
	DBTemplate * dbc=ctx.dbc;
	inSetter.setdata(datas1,sizeof(datas1)/sizeof(t_dbc_test_apicall));
	dbc->update("insert into dbc_test_apicall(id,TEST_INT_T) values(:id{char[18]},:test_int_t{int})",inSetter);
	EXPECT_EQ2(1,ctx.getdbc_count("dbc_test_apicall"),判断插入的行数);
	inSetter.setdata(datas2,sizeof(datas2)/sizeof(t_dbc_test_apicall));
	dbc->update("insert into dbc_test_apicall(id,TEST_INT_T) values(:id{char[18]},:test_int_t{int})",inSetter);
	EXPECT_EQ2(6,ctx.getdbc_count("dbc_test_apicall"),判断第二次插入的行数);
	dbc->commit();
	t_dbc_test_apicall searchData;
	EXPECT_EQ2(1,ctx.get_dbcdata("dbc_test_apicall",&searchData,1,"id='aaaaaaaaaaa1'"),单行查找);
	EXPECT_EQ2(1,searchData.test_int_t,判断查找内容);
	t_dbc_test_apicall searchDatas[3];
	EXPECT_EQ2(3,ctx.get_dbcdata("dbc_test_apicall",searchDatas,3,"test_int_t=4"),多行查找);
	EXPECT_EQ2(4,searchDatas[0].test_int_t,判断查找内容);
	searchData.id[0]='\0';
	searchData.test_int_t=0;
	EXPECT_EQ2(1,dbc->update("update dbc_test_apicall set test_int_t=5 where id='aaaaaaaaaaa1'"),单行更新);
	dbc->commit();
	EXPECT_EQ2(1,ctx.get_dbcdata("dbc_test_apicall",&searchData,1,"id='aaaaaaaaaaa1'"),判断更新);
	EXPECT_EQ2(5,searchData.test_int_t,判断更新内容);

	EXPECT_EQ2(3,dbc->update("update dbc_test_apicall set test_int_t=6  where test_int_t=4"),多行更新);
	dbc->commit();
	EXPECT_EQ2(3,ctx.get_dbcdata("dbc_test_apicall",searchDatas,3,"test_int_t=6"),判断多更新);
	EXPECT_EQ2(6,searchDatas[0].test_int_t,判断查找内容);

	EXPECT_EQ2(1,dbc->update("delete from  dbc_test_apicall  where id='aaaaaaaaaaa1'"),单行删除);
	dbc->commit();
	EXPECT_EQ2(5,ctx.getdbc_count("dbc_test_apicall"),判断单行删除结果);
	EXPECT_EQ2(3,dbc->update("delete from  dbc_test_apicall  where test_int_t=6"),多行删除);
	dbc->commit();
	EXPECT_EQ2(2,ctx.getdbc_count("dbc_test_apicall"),判断多行删除结果);
	EXPECT_EQ2(2,dbc->update("delete from dbc_test_apicall "),清理数据);
	dbc->commit();
	EXPECT_EQ(0,ctx.getdbc_count("dbc_test_apicall"));
	dbc->commit();
}
TEST3(DBC结合SCCI功能测试,静态方式){
	insertSetter inSetter;
	t_dbc_test_apicall datas1[]={
			{"aaaaaaaaaaa1",1},
	};
	t_dbc_test_apicall datas2[]={
			{"aaaaaaaaaaa2",2},
			{"aaaaaaaaaaa3",3},
			{"aaaaaaaaaaa4",4},
			{"aaaaaaaaaaa5",4},
			{"aaaaaaaaaaa6",4}
	};
	DBTemplate * dbc=ctx.dbc;
	inSetter.setdata(datas1,sizeof(datas1)/sizeof(t_dbc_test_apicall));
	dbc->update<dbc_test_insert_class>(inSetter);
	dbc->commit();
	EXPECT_EQ2(1,ctx.getdbc_count("dbc_test_apicall"),判断插入的行数);
	inSetter.setdata(datas2,sizeof(datas2)/sizeof(t_dbc_test_apicall));
	dbc->update<dbc_test_insert_class>(inSetter);
	dbc->commit();
	EXPECT_EQ2(6,ctx.getdbc_count("dbc_test_apicall"),判断第二次插入的行数);

	t_dbc_test_apicall searchData;
	EXPECT_EQ2(1,(ctx.get_dbcdata<t_dbc_test_apicall,dbc_test_select_class >(pvalues("aaaaaaaaaaa1"),&searchData,1)),单行查找);
	EXPECT_EQ2(1,searchData.test_int_t,判断查找内容);
	t_dbc_test_apicall searchDatas[3];
	EXPECT_EQ2(3,(ctx.get_dbcdata<t_dbc_test_apicall,dbc_test_mselect_class>(pvalues(4),searchDatas,3)),多行查找);
	EXPECT_EQ2(4,searchDatas[0].test_int_t,判断查找内容);
	searchData.id[0]='\0';
	searchData.test_int_t=0;
	EXPECT_EQ2(1,dbc->update<dbc_test_update_class>(pvalues(5,"aaaaaaaaaaa1")),单行更新);
	dbc->commit();
	EXPECT_EQ2(1,(ctx.get_dbcdata<t_dbc_test_apicall,dbc_test_select_class>(pvalues("aaaaaaaaaaa1"),&searchData,1)),判断更新);
	EXPECT_EQ2(5,searchData.test_int_t,判断更新内容);

	EXPECT_EQ2(3,dbc->update<dbc_test_mupdate_class>(pvalues(6,4)),多行更新);
	dbc->commit();
	EXPECT_EQ2(3,(ctx.get_dbcdata<t_dbc_test_apicall,dbc_test_mselect_class>(pvalues(6),searchDatas,3)),判断多行更新);
	EXPECT_EQ2(6,searchDatas[0].test_int_t,判断查找内容);

	EXPECT_EQ2(1,dbc->update<dbc_test_delete_class>(pvalues("aaaaaaaaaaa1")),单行删除);
	dbc->commit();
	EXPECT_EQ2(5,ctx.getdbc_count("dbc_test_apicall"),判断单行删除结果);
	EXPECT_EQ2(3,dbc->update<dbc_test_mdelete_class>(pvalues(6)),多行删除);
	dbc->commit();
	EXPECT_EQ2(2,ctx.getdbc_count("dbc_test_apicall"),判断多行删除结果);
	EXPECT_EQ2(2,dbc->update("delete from dbc_test_apicall"),清理数据);
	dbc->commit();
	EXPECT_EQ(0,ctx.getdbc_count("dbc_test_apicall"));
	dbc->commit();
}
TEST4(事务隔离机制测试){
	using namespace ai::sg;
	map<string,string> context;
	AttributeProvider attribute(context,TEMPLATECONFIG);
	string user_name=attribute.get("dbc_username");
	string password=attribute.get("dbc_password");
	int task_count=sizeof(test_plan)/sizeof(dbc_task);
	for(int i=0;i<task_count;i++){
		int max_thread = -1;
		ctx.destory();
		string cmd="rm -fr "+attribute.get("dbc_data")+"/sync/*";
		Sys::noutsys(cmd.c_str(),"test.log");
		ctx.loadConfig(DBCCONFIGPATH"dbc.xml",TEMPLATECONFIG,context);
		for (const dbc_action_t *dbc_action = test_plan[i].test_action; dbc_action->action_type != ACTION_TYPE_END; ++dbc_action) {
			if (dbc_action->thread_id > max_thread)
				max_thread = dbc_action->thread_id;
		}
		test_manager test_mgr(user_name, password);
		test_thread *threads;
		bool result = true;
		boost::thread_group thread_group;
		if (max_thread >= 0) {
			threads = new test_thread[max_thread + 1];
			for (int i = 0; i <= max_thread; i++){
				thread_group.create_thread(boost::bind(&test_thread::run, &threads[i], user_name, password));
			}
		} else {
			threads = NULL;
		}
		GTEST_STEPDESC_(test_plan[i].desc);
		for (const dbc_action_t *current_action = test_plan[i].test_action; current_action->action_type != ACTION_TYPE_END; ++current_action) {
			int thread_id = current_action->thread_id;
			if (thread_id == -1) {
				result = test_mgr.execute(*current_action);
				EXPECT_TRUE(result)<<"第"<<current_action-test_plan[i].test_action<<"条测试";
				if (!result)
					break;
			}else {
				result = threads[thread_id].execute(*current_action);
				EXPECT_TRUE(result)<<"第"<<current_action-test_plan[i].test_action<<"条测试";
				if (!result)
					break;
			}
		}
		for(int i=0;i<=max_thread;i++){
				threads[i].stop();
		}
		thread_group.join_all();
		if (threads)
			delete []threads;
	}
	string cmd="rm -fr "+attribute.get("dbc_data")+"/sync/*";
	Sys::noutsys(cmd.c_str(),"test.log");
	ctx.loadConfig(DBCCONFIGPATH"dbc.xml",TEMPLATECONFIG,context);
}


int main(int argc, char **argv) {
	testing::AddGlobalTestEnvironment(new DBCEnvironment);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
