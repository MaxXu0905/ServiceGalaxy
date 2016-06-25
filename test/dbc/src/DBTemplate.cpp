/*
 * DBTemplate.cpp
 *
 *  Created on: 2012-5-7
 *      Author: Administrator
 */

#include "DBTemplate.h"

namespace ai {
namespace test {
namespace dbc {
namespace {
class RowCountMapper: public DBTemplate::RowMapper {
public:
	bool map(Generic_ResultSet * result) {
		rowcount = result->getInt(1);
		return false;
	}
	int rowcount;
};
}
int DBTemplate::update(const string &sql){
	RowSetter rowSetter;
	return update(sql,rowSetter);
}
int DBTemplate::update(const string &sql,RowSetter & rowSetter){
	SqlStmtCreator creater(sql);
	return update(creater,rowSetter);
}
int DBTemplate::update(StmtCreator &creater){
	RowSetter rowSetter;
	return update(creater,rowSetter);
}
int DBTemplate::update(StmtCreator &creater,RowSetter & rowSetter){
	Generic_Statement *stmt=creater.create(db,db_type);
	rowSetter.set(stmt);
	stmt->executeUpdate();
	return stmt->getUpdateCount();
}
int DBTemplate::query(const string &sql,RowMapper &mapper){
	RowSetter rowSetter;
	return query(sql,mapper,rowSetter);
}
int DBTemplate::query(const string &sql,RowMapper &mapper,RowSetter & rowSetter){
	RowPassExtractor extractor(mapper);
	return query(sql,extractor,rowSetter);
}
int DBTemplate::query(const string &sql,Extractor &extractor){
	RowSetter rowSetter;
	return query(sql,extractor,rowSetter);
}
int DBTemplate::query(const string &sql,Extractor &extractor,RowSetter & rowSetter){
	SqlStmtCreator creater(sql);
	return query(creater,extractor,rowSetter);
}
int DBTemplate::query(StmtCreator &creater,RowMapper &mapper){
	RowSetter rowSetter;
	return query(creater,mapper,rowSetter);
}
int DBTemplate::query(StmtCreator &creater,RowMapper &mapper,RowSetter & rowSetter){
	RowPassExtractor extractor(mapper);
	return query(creater,extractor,rowSetter);
}
int DBTemplate::query(StmtCreator &creater,Extractor &extractor){
	RowSetter rowSetter;
	return query(creater,extractor,rowSetter);
}
int DBTemplate::query(StmtCreator &creater,Extractor &extractor,RowSetter & rowSetter){
	Generic_Statement *stmt=creater.create(db,db_type);
	rowSetter.set(stmt);
	auto_ptr<Generic_ResultSet> result(stmt->executeQuery());
	return extractor.extract(result.get());
}
int DBTemplate::rowcount(const string &table_name,const string& where){
		RowCountMapper mapper;
		ostringstream sql;
		sql << "select count(1){int} from " << table_name;
		if (""!=where) {
			sql << " where 1=1 and " << where;
		}
		query(sql.str(),mapper);
		return mapper.rowcount;
}

int DBTemplate::doTransaction(Transaction * action){
	return 0;
}

} /* namespace dbc */
} /* namespace test */
} /* namespace ai */
