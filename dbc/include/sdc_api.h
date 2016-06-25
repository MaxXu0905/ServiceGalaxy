#if !defined(__SDC_MANAGER_H__)
#define __SDC_MANAGER_H__

#include "dbc_internal.h"
#include "sdc_struct.h"

namespace ai
{
namespace sg
{

using namespace ai::scci;

struct sdc_pmeta_t
{
	sqltype_enum field_type;							// 字段类型
	char partition_format[PARTITION_FORMAT_SIZE + 1];	// PARTITION格式化字段
	int field_size; 										// 字段长度
	int field_offset;									// 字段在结构中的偏移量
};

struct sdc_meta_t
{
	int partition_type;
	int partitions;
	int struct_size;
	vector<sdc_pmeta_t> fields;
};

struct sdc_partition_t
{
	int table_id;
	int partition_id;

	bool operator<(const sdc_partition_t& rhs) const {
		if (table_id < rhs.table_id)
			return true;
		else if (table_id > rhs.table_id)
			return false;

		return partition_id < rhs.partition_id;
	}

	bool operator==(const sdc_partition_t& rhs) const {
		return (table_id == rhs.table_id && partition_id == rhs.partition_id);
	}
};

size_t hash_value(const sdc_partition_t& rhs);

class sdc_api : public dbc_manager
{
public:
	static sdc_api& instance();

	virtual ~sdc_api();

	bool login();
	void logout();
	bool get_meta();
	bool connect(const string& user_name, const string& password);
	bool connected() const;
	void disconnect();
	bool set_user(int64_t user_id);
	int64_t get_user() const;

	int get_table(const string& table_name);
	int get_index(const string& table_name, const string& index_name);

	template<typename T>
	long find(int table_id, const T *data, T *result, long max_rows = 1)
	{
		int partition_id = get_partition(table_id, data);
		if (partition_id < 0) {
			DBCT->_DBCT_error_code = DBCENOENT;
			return -1;
		}

		if (DBCT->_DBCT_login && local(table_id, partition_id)) {
			dbc_mgr.set_user(DBCT->_DBCT_user_id);
			return dbc_mgr.find(table_id, data, result, max_rows);
		} else {
			return find(table_id, data, result, max_rows, partition_id, sizeof(T));
		}
	}

	template<typename T>
	long find(int table_id, int index_id, const T *data, T *result, long max_rows = 1)
	{
		int partition_id = get_partition(table_id, data);
		if (partition_id < 0) {
			DBCT->_DBCT_error_code = DBCENOENT;
			return -1;
		}

		if (DBCT->_DBCT_login && local(table_id, partition_id)) {
			dbc_mgr.set_user(DBCT->_DBCT_user_id);
			return dbc_mgr.find(table_id, index_id, data, result, max_rows);
		} else {
			return find(table_id, index_id, data, result, max_rows, partition_id, sizeof(T));
		}
	}

	template<typename T>
	long erase(int table_id, const T *data)
	{
		int partition_id = get_partition(table_id, data);
		if (partition_id < 0) {
			DBCT->_DBCT_error_code = DBCENOENT;
			return -1;
		}

		return erase(table_id, data, partition_id, sizeof(T));
	}

	template<typename T>
	long erase(int table_id, int index_id, const T *data)
	{
		int partition_id = get_partition(table_id, data);
		if (partition_id < 0) {
			DBCT->_DBCT_error_code = DBCENOENT;
			return -1;
		}

		return erase(table_id, index_id, data, partition_id, sizeof(T));
	}

	template<typename T>
	bool insert(int table_id, const T *data)
	{
		int partition_id = get_partition(table_id, data);
		if (partition_id < 0) {
			DBCT->_DBCT_error_code = DBCENOENT;
			return false;
		}

		return insert(table_id, data, partition_id, sizeof(T));
	}

	template<typename T>
	long update(int table_id, const T *old_data, const T *new_data)
	{
		int partition_id = get_partition(table_id, old_data);
		if (partition_id < 0) {
			DBCT->_DBCT_error_code = DBCENOENT;
			return -1;
		}

		return update(table_id, old_data, new_data, partition_id, sizeof(T));
	}

	template<typename T>
	long update(int table_id, int index_id, const T *old_data, const T *new_data)
	{
		int partition_id = get_partition(table_id, old_data);
		if (partition_id < 0) {
			DBCT->_DBCT_error_code = DBCENOENT;
			return -1;
		}

		return update(table_id, index_id, old_data, new_data, partition_id, sizeof(T));
	}

	long find_dynamic(int table_id, const void *data, void *result, long max_rows = 1);
	long find_dynamic(int table_id, int index_id, const void *data, void *result, long max_rows = 1);
	long erase_dynamic(int table_id, const void *data);
	long erase_dynamic(int table_id, int index_id, const void *data);
	bool insert_dynamic(int table_id, const void *data);
	long update_dynamic(int table_id, const void *old_data, const void *new_data);
	long update_dynamic(int table_id, int index_id, const void *old_data, const void *new_data);

	// 获取错误代码
	int get_error_code() const;
	// 获取本地错误代码
	int get_native_code() const;
	// 获取错误消息
	const char * strerror() const;
	// 获取本地错误消息
	const char * strnative() const;
	// 获取用户自定义错误代码
	int get_ur_code() const;
	// 设置用户自定义错误代码
	void set_ur_code(int ur_code);

private:
	sdc_api();

	long find(int table_id, const void *data, void *result, long max_rows, int partition_id, int struct_size);
	long find(int table_id, int index_id, const void *data, void *result, long max_rows, int partition_id, int struct_size);
	long erase(int table_id, const void *data, int partition_id, int struct_size);
	long erase(int table_id, int index_id, const void *data, int partition_id, int struct_size);
	bool insert(int table_id, const void *data, int partition_id, int struct_size);
	long update(int table_id, const void *old_data, const void *new_data, int partition_id, int struct_size);
	long update(int table_id, int index_id, const void *old_data, const void *new_data, int partition_id, int struct_size);

	template<typename T>
	int get_partition(int table_id, const T *data)
	{
		char key[1024];

		boost::unordered_map<int, sdc_meta_t>::const_iterator iter = meta_map.find(table_id);
		if (iter == meta_map.end())
			return -1;

		const sdc_meta_t& meta = iter->second;

		T::get_partition(key, data);
		return dbc_api::get_partition_id(meta.partition_type, meta.partitions, key);
	}

	int get_partition(int table_id, const void *data, int& struct_size);
	bool local(int table_id, int partition_id) const;

	dbc_api& dbc_mgr;
	bool _SDC_login;
	message_pointer send_msg;
	message_pointer recv_msg;
	boost::unordered_map<int, sdc_meta_t> meta_map;
	boost::unordered_set<sdc_partition_t> partition_set;

	static boost::thread_specific_ptr<sdc_api> _instance;
};

}
}

#endif

