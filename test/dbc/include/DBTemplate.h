/*
 * DBTemplate.h
 *
 *  Created on: 2012-5-7
 *      Author: renyz
 */

#ifndef DBTEMPLATE_H_
#define DBTEMPLATE_H_
#include "sql_control.h"
#include "struct_dynamic.h"
namespace ai {
namespace test {
namespace dbc {
using namespace std;
using namespace ai::scci;

class DBTemplate{
public:
	DBTemplate(Generic_Database *db,dbtype_enum db_type):db(db),db_type(db_type){}
	~DBTemplate(){db->disconnect();delete db;}
	class RowMapper {
	public:
		virtual ~RowMapper() {
		}
		virtual bool map(Generic_ResultSet * result)=0;
	};
	class RowSetter {
		public:
			virtual ~RowSetter() {
			}
			virtual void set(Generic_Statement * stmt){};
	};
	class GroupSetter:public RowSetter{
	public:
		GroupSetter(vector<RowSetter*> setters):setters(setters.begin(),setters.end()){}
		void set(Generic_Statement * stmt){
			for (int i = 0; i < setters.size();++i){
				setters[i]->set(stmt);
			}
		};
		~GroupSetter(){
			for (int i = 0; i < setters.size();++i){
				delete setters[i];
			}
		}
	private:
		vector<RowSetter*> setters;
	};
	class StmtCreator{
		public:
			StmtCreator():stmt_(0),data_(0){}
			virtual ~StmtCreator() {
				if(stmt_)delete stmt_;
				if(data_) delete data_;
			}
			virtual Generic_Statement * create(Generic_Database *db,dbtype_enum dbtype){
				stmt_=createStmt(db);
				data_=createData(dbtype);
				stmt_->bind(data_);
				return stmt_;
			}
			virtual Generic_Statement * createStmt(Generic_Database *db){
				return db->create_statement();
			}
			virtual struct_base * createData(dbtype_enum dbtype)=0;
		protected:
			Generic_Statement *stmt_;
			struct_base* data_;
	};
	class SqlStmtCreator:public StmtCreator{
		public:
			SqlStmtCreator(const string & sql):sql_(sql){}
			struct_base * createData(dbtype_enum dbtype){
				struct_dynamic *dydata=new struct_dynamic(dbtype);
				dydata->setSQL(sql_);
				return dydata;
			}

		private:
			const string &sql_;
	};
	template <typename T> class StaticStmtCreator:public StmtCreator{
			public:
				StaticStmtCreator(){}
				struct_base * createData(dbtype_enum dbtype){
					return new T();
				}
	};
	class Extractor {
	public:
			virtual ~Extractor() {
			}
			virtual int extract(Generic_ResultSet * result)=0;
		};

	class RowPassExtractor :public Extractor {
	public:
		virtual ~RowPassExtractor() {
		}
		RowPassExtractor(RowMapper &mapper) :
				mapper(mapper) {
		}
		virtual int extract(Generic_ResultSet * result) {
			int re=0;
			while (result->next() && mapper.map(result))re++;
			return re;
		}
	protected:
		RowMapper &mapper;
	};
	class Transaction{
	public:
		Transaction(DBTemplate * dbtemp):dbtemp(dbtemp){}
		virtual ~Transaction() {}
		virtual void doit();
	private:
		DBTemplate * dbtemp;
	};
	int update(const string &sql);
	int update(const string &sql,RowSetter & rowSetter);
	template<typename T> int update(RowSetter & rowSetter){
		StaticStmtCreator<T> creater;
		return update(creater,rowSetter);
	}
	template<typename T> int update(vector<RowSetter*> setters){
			StaticStmtCreator<T> creater;
			GroupSetter setter(setters);
			return update(creater,setter);
		}

	int update(StmtCreator &creater);
	int update(StmtCreator &creater,RowSetter & rowSetter);
	int query(const string &sql,RowMapper &mapper);
	int query(const string &sql,RowMapper &mapper,RowSetter & rowSetter);
	int query(const string &sql,Extractor &extractor);
	int query(const string &sql,Extractor &extractor,RowSetter & rowSetter);

	template<typename T> int query(RowMapper &mapper){
		StaticStmtCreator<T> creater;
		return query(creater,mapper);
	}
	template<typename T> int query(RowMapper &mapper,RowSetter & rowSetter){
		StaticStmtCreator<T> creater;
		return query(creater,mapper,rowSetter);
	}
	template<typename T> int query(Extractor &extractor){
		StaticStmtCreator<T> creater;
		return query(creater,extractor);
	}
	template<typename T> int query(Extractor &extractor,RowSetter & rowSetter){
		StaticStmtCreator<T> creater;
		return query(creater,extractor,rowSetter);
	}
	int query(StmtCreator &creater,RowMapper &mapper);
	int query(StmtCreator &creater,RowMapper &mapper,RowSetter & rowSetter);
	int query(StmtCreator &creater,Extractor &extractor);
	int query(StmtCreator &creater,Extractor &extractor,RowSetter & rowSetter);
	void commit(){
		db->commit();
	}
	int rowcount(const string & table_name,const string &where="");
	int doTransaction(Transaction * action);
	private:
		Generic_Database *db;
		dbtype_enum db_type;
};
inline void setparm(Generic_Statement * stmt,int index,char v){
	stmt->setChar(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,unsigned char v){
	stmt->setUChar(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,short v){
	stmt->setShort(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,unsigned short v){
	stmt->setUShort(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,int v){
	stmt->setInt(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,unsigned int v){
	stmt->setUInt(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,long v){
	stmt->setLong(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,unsigned long  v){
	stmt->setULong(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,float v){
	stmt->setFloat(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,double v){
	stmt->setDouble(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,const string &v){
	stmt->setString(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,const date& v){
	stmt->setDate(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,const ptime& v){
	stmt->setTime(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,const datetime& v){
	stmt->setDatetime(index,v);
}
inline void setparm(Generic_Statement * stmt,int index,const sql_datetime& v){
	stmt->setSQLDatetime(index,v);
}

template<typename T> class PvaluesSetter:public DBTemplate::RowSetter{
public:
	PvaluesSetter(T v,int index):v_(v),index_(index){}
	virtual ~PvaluesSetter(){}
	void set(Generic_Statement * stmt){
		setparm(stmt,index_,v_);
	}
private:
	T v_;
	int index_;
};
template<> class PvaluesSetter<char *>:public DBTemplate::RowSetter{
public:
	PvaluesSetter(char * v,int index):v_(v),index_(index){}
	virtual ~PvaluesSetter(){}
	void set(Generic_Statement * stmt){
		setparm(stmt,index_,v_);
	}
private:
	string v_;
	int index_;
};

template<typename T1> vector<DBTemplate::RowSetter*> pvalues(T1 v1){
	vector<DBTemplate::RowSetter*> setters;
	setters.push_back(new PvaluesSetter<T1>(v1,1));
	return setters;
}
template<typename T1,typename T2> vector<DBTemplate::RowSetter*> pvalues(T1 v1,T2 v2){
	vector<DBTemplate::RowSetter*> setters;
	setters.push_back(new PvaluesSetter<T1>(v1,1));
	setters.push_back(new PvaluesSetter<T2>(v2,2));
	return setters;
}
template<typename T1,typename T2,typename T3> vector<DBTemplate::RowSetter*> pvalues(T1 v1,T2 v2,T3 v3){
	vector<DBTemplate::RowSetter*> setters;
	setters.push_back(new PvaluesSetter<T1>(v1,1));
	setters.push_back(new PvaluesSetter<T2>(v2,2));
	setters.push_back(new PvaluesSetter<T3>(v3,3));
	return setters;
}

} /* namespace dbc */
} /* namespace test */
} /* namespace ai */
#endif /* DBTEMPLATE_H_ */
