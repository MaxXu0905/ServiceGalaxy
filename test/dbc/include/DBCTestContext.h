/*
 * DBCTextContext.h
 *
 *  Created on: 2012-4-23
 *      Author: Administrator
 */

#ifndef DBCTEXTCONTEXT_H_
#define DBCTEXTCONTEXT_H_
#include "sql_control.h"
#include "oracle_control.h"
#include "dbc_api.h"
#include "datastruct_structs.h"
#include "dbc_control.h"
#include "struct_dynamic.h"
#include "sg_api.h"
#include "database.h"
#include <sstream>
#include "DBTemplate.h"
#include "dbc_test_insert.h"
#include "dbc_test_delete.h"
#include "dbc_test_update.h"
#include "dbc_test_select.h"
#include "dbc_test_mdelete.h"
#include "dbc_test_mupdate.h"
#include "dbc_test_mselect.h"
namespace ai {
namespace test {
namespace dbc {
using namespace ai::sg;
using namespace std;
using namespace ai::scci;
template<typename Table>class DataFetchExtractor;
template<typename Table> class CompareMapper;
template <typename Table> void map_to_loaddata(Generic_ResultSet * result,Table &data){
	strcpy(data.id, result->getString(1).c_str());
	data.test_char_t = result->getChar(2);
	data.test_uchar_t = result->getChar(3);
	data.test_short_t = result->getShort(4);
	data.test_ushort_t = result->getShort(5);
	data.test_int_t = result->getInt(6);
	data.test_uint_t = result->getInt(7);
	data.test_long_t = result->getLong(8);
	data.test_ulong_t = result->getLong(9);
	data.test_float_t = result->getFloat(10);
	data.test_double_t = result->getDouble(11);
	strcpy(data.test_string_t, result->getString(12).c_str());
	strcpy(data.test_string_t2, result->getString(13).c_str());
	strcpy(data.test_vstring_t, result->getString(14).c_str());
	strcpy(data.test_vstring_t2, result->getString(15).c_str());
	data.test_date_t = result->getDate(16).duration();
	data.test_date_t2 = result->getDate(17).duration();
	data.test_time_t = result->getTime(18).duration();
	data.test_datetime_t = result->getDatetime(19).duration();
}
template <typename Table> void map_to_data(Generic_ResultSet * result,Table &data);
template <> void map_to_data<t_dbc_test_bigload>(Generic_ResultSet * result,t_dbc_test_bigload &data){
	map_to_loaddata(result,data);
}
template <> void map_to_data<t_dbc_test_load>(Generic_ResultSet * result,t_dbc_test_load &data){
	map_to_loaddata(result,data);
}
template <> void map_to_data<t_sdc_test_find>(Generic_ResultSet * result,t_sdc_test_find &data){
	map_to_loaddata(result,data);
}
template <> void map_to_data<t_dbc_test_apicall>(Generic_ResultSet * result,t_dbc_test_apicall &data){
	strcpy(data.id, result->getString(1).c_str());
	data.test_int_t = result->getInt(2);
}
template<typename Table> string build_sql(const string &tablename,const string &where="");

template<> string build_sql<t_dbc_test_load>(const string &tablename,const string &where){
		ostringstream sql;
		sql << "select id{char[18]},"
				"test_char_t{char},"
				"test_uchar_t{uchar},"
				"test_short_t{short},"
				"test_ushort_t{ushort},"
				"test_int_t{int}, "
				"test_uint_t{uint},"
				"test_long_t{long},"
				"test_ulong_t{ulong},"
				"test_float_t{float},"
				"test_double_t{double},"
				"test_string_t{char[16]},"
				"test_string_t2{char[256]},"
				"test_vstring_t{char[16]},"
				"test_vstring_t2{char[256]},"
				"test_date_t{date},"
				"test_date_t2{date},"
				"test_time_t{time},"
				"test_datetime_t{datetime}"
				" from "<<tablename;
		if(""!=where){
			sql<<" where 1=1 and " << where;
		}
		return sql.str();
}
template<> string build_sql<t_dbc_test_bigload>(const string &tablename,const string &where){
	 return build_sql<t_dbc_test_load>(tablename,where);
}
template<> string build_sql<t_sdc_test_find>(const string &tablename,const string &where){
	 return build_sql<t_dbc_test_load>(tablename,where);
}
template<> string build_sql<t_dbc_test_apicall>(const string &tablename,const string &where){
		ostringstream sql;
			sql << "select id{char[18]},"
					"test_int_t{int}"
					" from "<<tablename;
			if(""!=where){
				sql<<" where 1=1 and " << where;
			}
			return sql.str();
}
class DBCTest_Context {
public:
	DBCTest_Context();
	virtual ~DBCTest_Context();
	void connect(const string & configfile);
	void disconnect();
	void create(const string & configfile);
	void destory();
	bool loadConfig(const string& filename,const string& configfile,map<string,string> &context);
	bool loadConfig(const string& filename,const string& configfile);
	bool loadConfig(const string& filename,map<string,string> &context);
	bool loadFILE(const string& filename,const string& configfile);
	int getdb_count(const string& table_name,const string&  where="");
	int getdbc_count(const string& table_name,const string&  where="");
	template<typename Table> int get_dbdata(const string &table_name,Table *data,int count=1,const string & where=""){
		return get_data(db,table_name,data,count,where);
	}
	template<typename Table> int get_dbcdata(const string &table_name,Table *data,int count=1,const string & where=""){
		return get_data(dbc,table_name,data,count,where);
	}
	template<typename Table,typename StaticSql> int get_dbdata(DBTemplate::RowSetter & rowSetter,Table *data,int count=1){
			return get_data<Table,StaticSql>(db,rowSetter,data,count);
		}
	template<typename Table,typename StaticSql> int get_dbcdata(DBTemplate::RowSetter & rowSetter,Table *data,int count=1){
		return get_data<Table,StaticSql>(dbc,rowSetter,data,count);
	}
	template<typename Table,typename StaticSql> int get_dbdata(vector<DBTemplate::RowSetter*> setters,Table *data,int count=1){
				DBTemplate::GroupSetter setter(setters);
				return get_data<Table,StaticSql>(db,setter,data,count);
			}
	template<typename Table,typename StaticSql> int get_dbcdata(vector<DBTemplate::RowSetter*> setters,Table *data,int count=1){
		DBTemplate::GroupSetter setter(setters);
		return get_data<Table,StaticSql>(dbc,setter,data,count);
	}

	//dbc与数据库的按行比较
	template<typename Table> int sampleCompare(Table &key,const string &table_name,int index_id,int count){
		Table * datas = new Table [count];
		long finded = dbc_mgr->find(dbc_mgr->get_table(table_name), index_id, &key, datas,count);
		int result=0;
		if(finded>0){
			Table data;
			Table * datas_ptr=datas;
			for(int i=0;i<finded;i++){
				ostringstream where;
				where<<" id='"<<datas_ptr->id<<"'";
				get_dbdata(table_name,&data,1,where.str());
				if(!Table::compare_exact0(&data, datas_ptr++))result++;
			}
		}
		delete []datas;
		return result;
	}
	//数据与dbc的按行比较
	template<typename Table> int sampleCompare(const string &table_name,int index_id,int rowcount) {
		CompareMapper<Table> mapper(dbc_mgr,table_name, dbc_mgr->get_table(table_name), index_id,rowcount);
		return db->query(mapper.sql(),mapper);
	}
	template<typename Table> int sampleCompare(const string &table_name,const string &index_name,int rowcount) {
		return sampleCompare<Table>(table_name,dbc_mgr->get_index(table_name,index_name),rowcount);
	}
	template<typename Table> static int get_data(DBTemplate * dbt,const string &table_name,Table *data,int rowcount=1,const string & where=""){
				DataFetchExtractor<Table> extractor(data,rowcount);
				return dbt->query(build_sql<Table>(table_name,where),extractor);
	}
	template<typename Table,typename StaticSql> static int get_data(DBTemplate * dbt,DBTemplate::RowSetter & rowSetter,Table *data,int rowcount=1){
					DataFetchExtractor<Table> extractor(data,rowcount);
					return dbt->query<StaticSql>(extractor,rowSetter);
	}
	DBTemplate *dbc;
private:
	DBTemplate *db;
	dbc_api *dbc_mgr;
	int dbcpid;
	int startDbc();
	int stopDbc();
};
template<typename Table> class CompareMapper: public DBTemplate::RowMapper{
public:
	CompareMapper(dbc_api *dbc_mgr, const string  table_name,int table_id,
				int index_id,int rowcount):dbc_mgr(dbc_mgr),table_name(table_name),
		table_id(table_id),index_id(index_id),count(rowcount){
	}
	bool map(Generic_ResultSet * result) {
		Table data1;
		Table data2;
		map_to_data<Table>(result,data1);
		long findre = dbc_mgr->find(table_id, index_id, &data1, &data2);
		if (findre > 0 && !Table::compare_exact0(&data1, &data2)) {
			return true;
		}else{
			return false;
		}
	}
	const string sql() {
		ostringstream where;
		where<<" rownum<="<<count;
		return build_sql<Table>(table_name,where.str());
	}
private:
	dbc_api *dbc_mgr;
	string table_name;
	int table_id;
	int index_id;
	int count;

};
template<typename Table>class DataFetchExtractor:public DBTemplate::Extractor{
	public:
		virtual ~DataFetchExtractor() {
		}
		DataFetchExtractor(Table * data,int maxcount):data(data),maxcount(maxcount){
		}
		virtual int extract(Generic_ResultSet * result) {
			int count=0;
			while (result->next()&&count<maxcount){
				map_to_data(result,*data);
				data++;
				count++;
			};
			return count;
		}
		Table *data;
		int maxcount;
};
} /* namespace dbc */
} /* namespace test */
} /* namespace ai */
#endif /* DBCTEXTCONTEXT_H_ */
