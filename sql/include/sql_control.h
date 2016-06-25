#if !defined(__SQL_CONTROL_H__)
#define __SQL_CONTROL_H__

#include <boost/tokenizer.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include "common.h"
#include "sql_base.h"
#include "struct_base.h"
#include "struct_dynamic.h"

typedef unsigned int ub4;
typedef int sb4;

namespace ai
{
namespace scci
{

using namespace ai::sg;
using namespace std;

class bad_sql : public exception
{
public:
	bad_sql(const string& file, int line, int sys_code_, int error_code, const string& message) throw();
	~bad_sql() throw();
	int get_error_code() const throw();
	const char* error_msg() const throw();
	const char* what () const throw();

private:
	int error_code_;
	string error_msg_;
	string what_;
};

class Generic_ResultSet;
class Generic_Statement;

struct Generic_Metadata
{
	string field_name;
	int field_type;
	int field_size;
};

int const SEQ_ACTION_CURRVAL = 0;
int const SEQ_ACTION_NEXTVAL = 1;

class Generic_Database
{
public:
	Generic_Database();
	virtual ~Generic_Database();

	virtual void connect(const map<string, string>& conn_info) = 0;
	virtual void disconnect() = 0;
	virtual void commit() = 0;
	virtual void rollback() = 0;
	virtual Generic_Statement * create_statement() = 0;
	virtual void terminate_statement(Generic_Statement *& stmt) = 0;
	virtual dbtype_enum get_dbtype() = 0;
	struct_dynamic * create_data(bool ind_required = false, int size = 1000);
	void terminate_data(struct_dynamic *& data);

	// 获取序列的选择项表达式
	virtual string get_seq_item(const string& seq_name, int seq_action);
};

enum execute_mode_enum
{
	EM_DEFAULT,
	EM_DESCRIBE_ONLY,
	EM_COMMIT_ON_SUCCESS,
	EM_EXACT_FETCH,
	EM_BATCH_ERRORS
};

class Generic_Statement
{
public:
	enum Status
	{
		UNPREPARED,
		PREPARED,
		RESULT_SET_AVAILABLE,
		UPDATE_COUNT_AVAILABLE,
		NEEDS_STREAM_DATA,
		STREAM_DATA_AVAILABLE
	};

	Generic_Statement();
	virtual ~Generic_Statement();

	virtual dbtype_enum get_dbtype() = 0;
	struct_dynamic * create_data(bool ind_required = false, int size = 1000);
	void terminate_data(struct_dynamic *& data);
	void setTable(const string& newTable_, const string& oldTable_ = "");
	void bind(struct_base *data_, int index_ = 0);
	const string& getColumnName(int colIndex) const;
	int getColumnCount() const;
	sqltype_enum getColumnType(int colIndex) const;
	const string& getBindName(int paramIndex) const;
	int getBindCount() const;
	sqltype_enum getBindType(int paramIndex) const;

	void setMaxIterations(int maxIterations_);
	int getMaxIterations() const;
	void resetIteration();
	void addIteration();
	int getCurrentIteration() const;
	// 复制记录到目标结构中，两个结构必须一致
	void copy_bind(int dst_idx, const Generic_Statement& src_stmt, int src_idx);
	// 给定关键字列表，对记录进行排序，关键字指定绑定位置，从0开始
	void sortArray(int arrayLength, const vector<int>& keys, int *sort_array);
	void sort(const vector<int>& keys, int *sort_array);

	virtual void setExecuteMode(execute_mode_enum execute_mode_) = 0;
	virtual execute_mode_enum getExecuteMode() const = 0;

	virtual int getUpdateCount() const = 0;
	virtual Generic_ResultSet * executeQuery() = 0;
	virtual void executeUpdate() = 0;
	virtual void executeArrayUpdate(int arrayLength) = 0;
	virtual void closeResultSet(Generic_ResultSet *& rset) = 0;

	virtual bool isNull(int colIndex) = 0;
	virtual bool isTruncated(int colIndex) = 0;
	virtual void setNull(int paramIndex) = 0;

	virtual char getChar(int colIndex);
	virtual unsigned char getUChar(int colIndex);
	virtual short getShort(int colIndex);
	virtual unsigned short getUShort(int colIndex);
	virtual int getInt(int colIndex);
	virtual unsigned int getUInt(int colIndex);
	virtual long getLong(int colIndex);
	virtual unsigned long getULong(int colIndex);
	virtual float getFloat(int colIndex);
	virtual double getDouble(int colIndex);
	virtual string getString(int colIndex);
	virtual date getDate(int colIndex);
	virtual ptime getTime(int colIndex);
	virtual datetime getDatetime(int colIndex);
	virtual sql_datetime getSQLDatetime(int colIndex);

	virtual void setChar(int paramIndex, char x);
	virtual void setUChar(int paramIndex, unsigned char x);
	virtual void setShort(int paramIndex, short x);
	virtual void setUShort(int paramIndex, unsigned short x);
	virtual void setInt(int paramIndex, int x);
	virtual void setUInt(int paramIndex, unsigned int x);
	virtual void setLong(int paramIndex, long x);
	virtual void setULong(int paramIndex, unsigned long x);
	virtual void setFloat(int paramIndex, float x);
	virtual void setDouble(int paramIndex, double x);
	virtual void setString(int paramIndex, const string& x);
	virtual void setDate(int paramIndex, const date& x);
	virtual void setTime(int paramIndex, const ptime& x);
	virtual void setDatetime(int paramIndex, const datetime& x);
	virtual void setSQLDatetime(int paramIndex, const sql_datetime& x);

	virtual date date2native(void *x) const = 0;
	virtual ptime time2native(void *x) const = 0;
	virtual datetime datetime2native(void *x) const = 0;
	virtual sql_datetime sqldatetime2native(void *x) const = 0;
	virtual void native2date(void *addr, const date& x) const = 0;
	virtual void native2time(void *addr, const ptime& x) const = 0;
	virtual void native2datetime(void *addr, const datetime& x) const = 0;
	virtual void native2sqldatetime(void *addr, const sql_datetime& x) const = 0;

	virtual void setRowOffset(ub4 rowOffset_) = 0;
	virtual int getRowsErrorCount() const = 0;
	virtual ub4 * getRowsError() = 0;
	virtual sb4 * getRowsErrorCode() = 0;

protected:
	virtual void prepare() = 0;
	virtual void clearNull(char *addr) = 0;

	void qsort(int lower, int upper, const vector<int>& keys, int *sort_array);
	long compare(int first, int second, const vector<int>& keys);

	int iteration;
	int maxIterations;
	struct_base *data;
	int index;
	bind_field_t *select_fields;
	int select_count;
	bind_field_t *bind_fields;
	int bind_count;
	char **sort_buf;

	ostringstream fmt;
	string newTable;
	string oldTable;
	bool prepared;
};

class Generic_ResultSet
{
public:
	enum Status
	{
		END_OF_FETCH = 0,
		DATA_AVAILABLE,
		STREAM_DATA_AVAILABLE,
		FIRST_DATA_CALL
	};

	Generic_ResultSet();
	virtual ~Generic_ResultSet();

	Status status() const;
	const string& getColumnName(int colIndex) const;
	int getColumnCount() const;
	sqltype_enum getColumnType(int colIndex) const;

	virtual Status next() = 0;
	virtual Status batchNext() = 0;
	virtual int getFetchRows() const = 0;
	virtual bool isNull(int colIndex) = 0;
	virtual bool isTruncated(int paramIndex) = 0;

	virtual char getChar(int colIndex);
	virtual unsigned char getUChar(int colIndex);
	virtual short getShort(int colIndex);
	virtual unsigned short getUShort(int colIndex);
	virtual int getInt(int colIndex);
	virtual unsigned int getUInt(int colIndex);
	virtual long getLong(int colIndex);
	virtual unsigned long getULong(int colIndex);
	virtual float getFloat(int colIndex);
	virtual double getDouble(int colIndex);
	virtual string getString(int colIndex);
	virtual date getDate(int colIndex);
	virtual ptime getTime(int colIndex);
	virtual datetime getDatetime(int colIndex);
	virtual sql_datetime getSQLDatetime(int colIndex);

	virtual date date2native(void *x) const = 0;
	virtual ptime time2native(void *x) const = 0;
	virtual datetime datetime2native(void *x) const = 0;
	virtual sql_datetime sqldatetime2native(void *x) const = 0;

protected:
	void bind(struct_base *data_, int index_ = 0);

	Status _status;
	struct_base *data;
	int index;
	const bind_field_t *select_fields;
	int select_count;
	int iteration;

	ostringstream fmt;
};

struct database_creator_t
{
	std::string db_name;
	Generic_Database * (*create)();
};

class database_factory
{
public:
	static database_factory& instance();
	~database_factory();

	Generic_Database * create(const std::string& db_name);
	void parse(const std::string& openinfo, std::map<string, string>& conn_info);
	void push_back(const database_creator_t& item);

private:
	database_factory();

	std::vector<database_creator_t> database_creators;
	static database_factory _instance;
};

#define DECLARE_DATABASE(_db_name, _class_name) \
namespace \
{ \
\
class database_initializer \
{ \
public: \
	database_initializer() { \
		database_factory& instance = database_factory::instance(); \
		database_creator_t item; \
\
		item.db_name = _db_name; \
		item.create = &database_initializer::create; \
		instance.push_back(item); \
	} \
\
private: \
	static Generic_Database * create() { \
		return new _class_name(); \
	} \
} initializer; \
\
}

}
}

#endif

