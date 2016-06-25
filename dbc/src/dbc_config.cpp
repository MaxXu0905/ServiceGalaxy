#include "dbc_internal.h"

namespace ai
{
namespace sg
{

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
using namespace std;
namespace bi = boost::interprocess;

static ostream& operator<<(ostream& os, const dbc_data_field_t& rhs)
{
	os << rhs.field_name << '\0'
		<< rhs.field_type << '\0'
		<< rhs.is_primary << '\0'
		<< rhs.is_partition << '\0'
		<< rhs.partition_format << '\0';

	return os;
}

static ostream& operator<<(ostream& os, const dbc_data_t& rhs)
{
	os << rhs.table_name << '\0'
		<< rhs.field_size << '\0';
	for (int i = 0; i < rhs.field_size; i++)
		os << rhs.fields[i] << '\0';

	return os;
}

static ostream& operator<<(ostream& os, const dbc_index_field_t& rhs)
{
	os << rhs.field_name << '\0'
		<< rhs.is_hash << '\0'
		<< rhs.hash_format << '\0'
		<< rhs.search_type << '\0'
		<< rhs.special_type << '\0'
		<< rhs.range_group << '\0'
		<< rhs.range_offset << '\0';

	return os;
}

static ostream& operator<<(ostream& os, const dbc_index_t& rhs)
{
	os << rhs.index_name << '\0'
		<< rhs.index_type << '\0'
		<< rhs.method_type << '\0'
		<< rhs.field_size << '\0';
	for (int i = 0; i < rhs.field_size; i++)
		os << rhs.fields[i] << '\0';

	return os;
}

static ostream& operator<<(ostream& os, const dbc_func_field_t& rhs)
{
	os << rhs.field_name << '\0'
		<< rhs.nullable << '\0'
		<< rhs.field_ref << '\0';

	return os;
}

static ostream& operator<<(ostream& os, const dbc_func_t& rhs)
{
	os << rhs.field_size << '\0';
	for (int i = 0; i < rhs.field_size; i++)
		os << rhs.fields[i] << '\0';

	return os;
}

dbc_config dbc_config::_instance;

dbc_config& dbc_config::instance()
{
	return _instance;
}

bool dbc_config::open()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif
	bpt::iptree pt;
	sgt_ctx *SGT = sgt_ctx::instance();
	bi::scoped_lock<boost::mutex> lock(mutex);

	if (opened) {
		retval = true;
		return retval;
	}

	if (DBCP->_DBCP_dbcconfig.empty()) {
		char *ptr = ::getenv("DBCPROFILE");
		if (ptr == NULL) {
			SGT->_SGT_error_code = SGESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: DBCPROFILE environment variable not set")).str(SGLOCALE));
			return retval;
		}
		DBCP->_DBCP_dbcconfig = ptr;
	} else {
		gpenv& env_mgr = gpenv::instance();
		string env_str = "DBCPROFILE=";
		env_str += DBCP->_DBCP_dbcconfig;
		env_mgr.putenv(env_str.c_str());
	}

	try {
		bpt::read_xml(DBCP->_DBCP_dbcconfig, pt);

		load_bbparms(pt);
		load_user(pt);
		load_sys_table();
		load_data(pt);
		load_index(pt);
		load_func(pt);
		load_libs(bbparms.libs);
	} catch (exception& ex) {
		SGT->_SGT_error_code = SGEINVAL;
		GPP->write_log(__FILE__, __LINE__, ex.what());
		return retval;
	}

	for (map<int, dbc_data_t>::const_iterator data_iter = data_map.begin(); data_iter != data_map.end(); ++data_iter) {
		if (data_iter->second.refresh_type & REFRESH_TYPE_NO_LOAD)
			continue;

		meta.push_back(data_index_t());
		data_index_t& item = *meta.rbegin();
		dbc_te_t& data = item.data;

		data.table_id = data_iter->first;
		data.flags = 0;
		data.max_index = -1;
		data.cur_count = 0;
		data.te_meta = data_iter->second;

		dbc_index_key_t index_key;
		index_key.table_id = data.table_id;
		index_key.index_id = 0;

		int primary_count = 0;
		map<dbc_index_key_t, dbc_index_t>::const_iterator index_iter;
		for (index_iter = index_map.lower_bound(index_key); index_iter != index_map.end() && index_iter->first.table_id == data.table_id; ++index_iter) {
			item.index_vector.push_back(dbc_ie_t());
			dbc_ie_t& ie_item = *item.index_vector.rbegin();

			ie_item.index_id = index_iter->first.index_id;
			ie_item.ie_meta = index_iter->second;
			if (ie_item.ie_meta.index_type & INDEX_TYPE_PRIMARY)
				primary_count++;

			data.max_index = std::max(data.max_index, ie_item.index_id);
		}

		if (primary_count > 1)
			throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: too many primary keys for table {1}.") % data.te_meta.table_name).str(SGLOCALE));
	}

	opened = true;
	retval = true;
	return retval;
}

void dbc_config::close()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif
	bi::scoped_lock<boost::mutex> lock(mutex);

	if (!opened)
		return;

	DBCP->_DBCP_dbcconfig = "";
	opened = false;
	meta.clear();
	users.clear();
	data_map.clear();
	index_map.clear();
	func_map.clear();
	primary_map.clear();
	unload_libs();
}

dbc_bbparms_t& dbc_config::get_bbparms()
{
	return bbparms;
}

vector<data_index_t>& dbc_config::get_meta()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	return meta;
}

vector<dbc_user_t>& dbc_config::get_users()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	return users;
}

map<int, dbc_data_t>& dbc_config::get_data_map()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	return data_map;
}

map<dbc_index_key_t, dbc_index_t>& dbc_config::get_index_map()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	return index_map;
}

map<dbc_index_key_t, dbc_func_t>& dbc_config::get_func_map()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	return func_map;
}

map<int, int>& dbc_config::get_primary_map()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	return primary_map;
}

// 检查libdbcclient.so和libdbcsearch.so/libsdcsearch.so是否需要重新编译
bool dbc_config::md5sum(string& result, bool is_client) const
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("is_client={1}") % is_client).str(SGLOCALE), &retval);
#endif
	sgt_ctx *SGT = sgt_ctx::instance();

	string tmpname;
	sys_func& func_mgr = sys_func::instance();

	func_mgr.tempnam(tmpname);

	BOOST_SCOPE_EXIT((&tmpname)) {
		unlink(tmpname.c_str());
	} BOOST_SCOPE_EXIT_END

	ofstream ofs(tmpname.c_str());
	if (!ofs) {
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = UOPEN;
		return retval;
	}

	if (is_client) {
		for (map<int, dbc_data_t>::const_iterator iter = data_map.begin(); iter != data_map.end(); ++iter) {
			ofs << iter->first << '\0'
				<< iter->second << '\0';
		}

		for (map<dbc_index_key_t, dbc_index_t>::const_iterator iter = index_map.begin(); iter != index_map.end(); ++iter) {
			ofs << iter->first.table_id << '\0'
				<< iter->first.index_id << '\0'
				<< iter->second << '\0';
		}
	} else {
		for (map<dbc_index_key_t, dbc_func_t>::const_iterator iter = func_map.begin(); iter != func_map.end(); ++iter) {
			ofs << iter->first.table_id << '\0'
				<< iter->first.index_id << '\0'
				<< iter->second << '\0';

			// 表
			map<int, dbc_data_t>::const_iterator data_iter = data_map.find(iter->first.table_id);
			if (data_iter != data_map.end()) {
				ofs << data_iter->first << '\0'
					<< data_iter->second << '\0';
			}

			// 索引
			map<dbc_index_key_t, dbc_func_t>::const_iterator index_iter = func_map.find(iter->first);
			if (index_iter != func_map.end()) {
				ofs << index_iter->first.table_id << '\0'
					<< index_iter->first.index_id << '\0'
					<< index_iter->second << '\0';
			}
		}
	}

	string command;
	if (access("/usr/bin/md5sum", X_OK) != -1)
		command = "/usr/bin/md5sum ";
	else if (access("/bin/md5sum", X_OK) != -1)
		command = "/bin/md5sum ";
	else
		command = "md5sum ";

	command += tmpname;
	FILE *fp = popen(command.c_str(), "r");
	if (!fp) {
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = UOPEN;
		return retval;
	}

	BOOST_SCOPE_EXIT((&fp)) {
		fclose(fp);
	} BOOST_SCOPE_EXIT_END

	char buf[32];
	int cnt = fread(buf, sizeof(buf), 1, fp);
	if (cnt != 1) {
		SGT->_SGT_error_code = SGEOS;
		SGT->_SGT_native_code = UREAD;
		return retval;
	}

	result = string(buf, buf + 32);
	retval = true;
	return retval;
}


dbc_config::dbc_config()
{
	GPP = gpp_ctx::instance();
	DBCP = dbcp_ctx::instance();
	opened = false;
}

dbc_config::~dbc_config()
{
	close();
}

void dbc_config::load_sys_table()
{
	map<int, dbc_data_t>::iterator data_iter;
	map<dbc_index_key_t, dbc_index_t>::iterator index_iter;
	dbc_data_t *dbc_data;
	dbc_data_field_t *item;
	dbc_index_key_t key;
	dbc_index_t *dbc_index;
	dbc_index_field_t *index_item;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	// 设置系统表dual
	{
		data_iter = data_map.insert(std::make_pair(TE_DUAL, dbc_data_t())).first;
		dbc_data = &data_iter->second;

		strcpy(dbc_data->table_name, "dual");
		dbc_data->conditions[0] = '\0';
		dbc_data->escape = '\0';
		dbc_data->delimiter = '\0';
		dbc_data->segment_rows = 1;
		dbc_data->refresh_type = 0;
		dbc_data->partition_type = PARTITION_TYPE_STRING;
		dbc_data->partitions = 1;
		dbc_data->struct_size = STRUCT_ALIGN;
		dbc_data->internal_size = dbc_data->struct_size;
		dbc_data->field_size = 1;

		item = &dbc_data->fields[0];
		strcpy(item->field_name, "dummy");
		strcpy(item->fetch_name, "dummy");
		strcpy(item->update_name, ":dummy");
		strcpy(item->column_name, "dummy");
		item->field_type = SQLTYPE_STRING;
		item->field_size = 2;
		item->field_offset = 0;
		item->is_primary = true;
		item->is_partition = false;
		item->partition_format[0] = '\0';
		item->load_size = 0;
	}

	// 设置系统表sys$index_lock
	{
		data_iter = data_map.insert(std::make_pair(TE_SYS_INDEX_LOCK, dbc_data_t())).first;
		dbc_data = &data_iter->second;

		strcpy(dbc_data->table_name, "sys_index_lock");
		dbc_data->conditions[0] = '\0';
		dbc_data->segment_rows = 1000;
		dbc_data->refresh_type = 0;
		dbc_data->partition_type = PARTITION_TYPE_STRING;
		dbc_data->partitions = 1;
		dbc_data->struct_size = sizeof(long) * 2 + sizeof(int) * 2;
		dbc_data->internal_size = dbc_data->struct_size;
		dbc_data->field_size = 4;

		item = &dbc_data->fields[0];
		strcpy(item->field_name, "pid");
		strcpy(item->fetch_name, "pid");
		strcpy(item->update_name, ":pid");
		strcpy(item->column_name, "pid");
		item->field_type = SQLTYPE_LONG;
		item->field_size = sizeof(long);
		item->field_offset = 0;
		item->is_primary = true;
		item->is_partition = false;
		item->partition_format[0] = '\0';
		item->load_size = 0;

		item = &dbc_data->fields[1];
		strcpy(item->field_name, "table_id");
		strcpy(item->fetch_name, "table_id");
		strcpy(item->update_name, ":table_id");
		strcpy(item->column_name, "table_id");
		item->field_type = SQLTYPE_INT;
		item->field_size = sizeof(int);
		item->field_offset = sizeof(long);
		item->is_primary = true;
		item->is_partition = false;
		item->partition_format[0] = '\0';
		item->load_size = 0;

		item = &dbc_data->fields[2];
		strcpy(item->field_name, "index_id");
		strcpy(item->fetch_name, "index_id");
		strcpy(item->update_name, ":index_id");
		strcpy(item->column_name, "index_id");
		item->field_type = SQLTYPE_INT;
		item->field_size = sizeof(int);
		item->field_offset = sizeof(long) + sizeof(int);
		item->is_primary = true;
		item->is_partition = false;
		item->partition_format[0] = '\0';
		item->load_size = 0;

		item = &dbc_data->fields[3];
		strcpy(item->field_name, "lock_date");
		strcpy(item->fetch_name, "lock_date");
		strcpy(item->update_name, ":lock_date");
		strcpy(item->column_name, "lock_date");
		item->field_type = SQLTYPE_DATETIME;
		item->field_size = sizeof(long);
		item->field_offset = sizeof(long) + sizeof(int) * 2;
		item->is_primary = true;
		item->is_partition = false;
		item->partition_format[0] = '\0';
		item->load_size = 0;

		// sys_index_lock_0
		key.table_id = TE_SYS_INDEX_LOCK;
		key.index_id = 0;
		index_iter = index_map.insert(std::make_pair(key, dbc_index_t())).first;

		dbc_index = &index_iter->second;
		strcpy(dbc_index->index_name, "sys_index_lock_0");
		dbc_index->index_type = INDEX_TYPE_PRIMARY;
		dbc_index->method_type = METHOD_TYPE_BINARY;
		dbc_index->buckets = 1;
		dbc_index->segment_rows = 1000;
		dbc_index->field_size = 3;
		primary_map[key.table_id] = key.index_id;

		index_item = &dbc_index->fields[0];
		strcpy(index_item->field_name, "pid");
		index_item->is_hash = false;
		index_item->hash_format[0] = '\0';
		index_item->search_type = SEARCH_TYPE_EQUAL;
		index_item->special_type = 0;
		index_item->range_group = 0;
		index_item->range_offset = 0;

		index_item = &dbc_index->fields[1];
		strcpy(index_item->field_name, "table_id");
		index_item->is_hash = false;
		index_item->hash_format[0] = '\0';
		index_item->search_type = SEARCH_TYPE_EQUAL;
		index_item->special_type = 0;
		index_item->range_group = 0;
		index_item->range_offset = 0;

		index_item = &dbc_index->fields[2];
		strcpy(index_item->field_name, "index_id");
		index_item->is_hash = false;
		index_item->hash_format[0] = '\0';
		index_item->search_type = SEARCH_TYPE_EQUAL;
		index_item->special_type = 0;
		index_item->range_group = 0;
		index_item->range_offset = 0;

		// sys_index_lock_1
		key.table_id = TE_SYS_INDEX_LOCK;
		key.index_id = 1;
		index_iter = index_map.insert(std::make_pair(key, dbc_index_t())).first;

		dbc_index = &index_iter->second;
		strcpy(dbc_index->index_name, "sys_index_lock_1");
		dbc_index->index_type = INDEX_TYPE_NORMAL;
		dbc_index->method_type = METHOD_TYPE_BINARY;
		dbc_index->buckets = 1;
		dbc_index->segment_rows = 1000;
		dbc_index->field_size = 2;

		index_item = &dbc_index->fields[0];
		strcpy(index_item->field_name, "table_id");
		index_item->is_hash = false;
		index_item->hash_format[0] = '\0';
		index_item->search_type = SEARCH_TYPE_EQUAL;
		index_item->special_type = 0;
		index_item->range_group = 0;
		index_item->range_offset = 0;

		index_item = &dbc_index->fields[1];
		strcpy(index_item->field_name, "index_id");
		index_item->is_hash = false;
		index_item->hash_format[0] = '\0';
		index_item->search_type = SEARCH_TYPE_EQUAL;
		index_item->special_type = 0;
		index_item->range_group = 0;
		index_item->range_offset = 0;
	}
}

void dbc_config::load_bbparms(const bpt::iptree& pt)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif
	gpenv& env_mgr = gpenv::instance();

	try {
		memset(&bbparms, '\0', sizeof(dbc_bbparms_t));

		bbparms.magic = 0;
		bbparms.bbrelease = DBC_RELEASE;
		bbparms.bbversion = time(0);

		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("dbc")) {
			if (v.first != "resource")
				continue;

			const bpt::iptree& v_pt = v.second;

			string ipckey;
			env_mgr.expand_var(ipckey, v_pt.get<string>("ipckey"));
			try {
				bbparms.ipckey = boost::lexical_cast<int>(ipckey);
			} catch (exception& ex) {
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.ipckey invalid, {1}.") % ex.what()).str(SGLOCALE));
			}
			if (bbparms.ipckey < 0 || bbparms.ipckey >= (1 << MCHIDSHIFT))
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.ipckey too large.")).str(SGLOCALE));

			bbparms.uid = get_value(v_pt, "uid", ::getuid());
			bbparms.gid = get_value(v_pt, "gid", ::getgid());

			parse_perm(get_value(v_pt, "perm", string("")), bbparms.perm, "resource.perm");

			bbparms.scan_unit = get_value(v_pt, "polltime", DFLT_POLLTIME);
			bbparms.sanity_scan = get_value(v_pt, "robustint", DFLT_ROBUSTINT);
			bbparms.stack_size = get_value(v_pt, "stacksize", DFLT_STACKSIZE);
			bbparms.stat_level = get_value(v_pt, "statlevel", 0);

			string sys_dir;
			env_mgr.expand_var(sys_dir, v_pt.get<string>("sysdir"));
			if (*sys_dir.rbegin() != '/')
				sys_dir += '/';
			if (sys_dir.empty())
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.sysdir is empty.")).str(SGLOCALE));
			else if (sys_dir.length() > MAX_DIR_LEN)
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.sysdir is too long.")).str(SGLOCALE));
			strcpy(bbparms.sys_dir, sys_dir.c_str());

			string sync_dir;
			env_mgr.expand_var(sync_dir, v_pt.get<string>("syncdir"));
			if (*sync_dir.rbegin() != '/')
				sync_dir += '/';
			if (sync_dir.empty())
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.syncdir is empty.")).str(SGLOCALE));
			else if (sync_dir.length() > MAX_DIR_LEN)
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.syncdir is too long.")).str(SGLOCALE));
			strcpy(bbparms.sync_dir, sync_dir.c_str());

			string data_dir;
			env_mgr.expand_var(data_dir, v_pt.get<string>("datadir"));
			if (*data_dir.rbegin() != '/')
				data_dir += '/';
			if (data_dir.empty())
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.datadir is empty.")).str(SGLOCALE));
			else if (data_dir.length() > MAX_DIR_LEN)
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.datadir is too long.")).str(SGLOCALE));
			strcpy(bbparms.data_dir, data_dir.c_str());

			bbparms.shm_size = get_value(v_pt, "shmsize", DFLT_SHM_SIZE);

			string shmcount_str;
			env_mgr.expand_var(shmcount_str, get_value(v_pt, "shmcount", string("")));
			bbparms.shm_count = atoi(shmcount_str.c_str());
			if (bbparms.shm_count <= 0)
				bbparms.shm_count = DFLT_SHM_COUNT;
			else if (bbparms.shm_count > MAX_SHMS)
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.shmcount must be equal or less than {1}.") % MAX_SHMS).str(SGLOCALE));

			string reservecount_str;
			env_mgr.expand_var(reservecount_str, get_value(v_pt, "reservecount", string("")));
			bbparms.reserve_count = atoi(reservecount_str.c_str());
			if (bbparms.reserve_count <= 0)
				bbparms.reserve_count = DFLT_RESERVE_COUNT;
			if (bbparms.reserve_count < bbparms.shm_count)
				bbparms.reserve_count = bbparms.shm_count;

			bbparms.post_signo = get_value(v_pt, "postsigno", SIGUSR1);
			bbparms.maxusers = get_value(v_pt, "maxusers", DFLT_MAXUSERS);
			bbparms.maxtes = get_value(v_pt, "maxtes", DFLT_MAXTES);
			bbparms.maxses = get_value(v_pt, "maxses", DFLT_MAXSES);
			bbparms.maxrtes = get_value(v_pt, "maxclts", DFLT_MAXRTES);
			bbparms.sebkts = get_value(v_pt, "sebkts", DFLT_SEBKTS);
			bbparms.segment_redos = get_value(v_pt, "segredos", DFLT_SEGMENT_REDOS);

			string libs = get_value(v_pt, "libs", string(""));
			if (libs.length() > MAX_LIBS_LEN)
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: resource.libs is too long.")).str(SGLOCALE));
			strcpy(bbparms.libs, libs.c_str());

			bbparms.initial_size = sizeof(dbc_bboard_t)
				+ sizeof(dbc_ue_t) * bbparms.maxusers
				+ sizeof(dbc_te_t) * bbparms.maxtes
				+ sizeof(shash_t) * bbparms.maxses
				+ sizeof(dbc_se_t) * bbparms.maxses
				+ sizeof(dbc_rte_t) * bbparms.maxrtes
				+ sizeof(dbc_redo_t);

			// Master节点上需要调整参数
			if (DBCP->_DBCP_proc_type & DBC_MASTER) {
				bbparms.ipckey |= (1 << MCHIDSHIFT);

				long min_size = bbparms.initial_size + 100L * 1024 * 1024;
				if (min_size > DFLT_SHM_SIZE) {
					bbparms.shm_size = DFLT_SHM_SIZE;
					bbparms.shm_count = (min_size + DFLT_SHM_SIZE - 1) / DFLT_SHM_SIZE;;
					bbparms.reserve_count = bbparms.shm_count;
				} else {
					bbparms.shm_size = min_size;
					bbparms.shm_count = 1;
					bbparms.reserve_count = 1;
				}
			}

			bbparms.total_size = bbparms.shm_size * bbparms.shm_count;
			if (bbparms.total_size < bbparms.initial_size + sizeof(dbc_mem_block_t))
				throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: shm_size * shm_count too small")).str(SGLOCALE));
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section resource failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section resource failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

void dbc_config::load_user(const bpt::iptree& pt)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif
	boost::char_separator<char> sep(" \t\b");
	gpenv& env_mgr = gpenv::instance();

	// 设置默认用户system
	dbc_user_t item;
	item.user_name = "system";
	item.password = "0ffea914bfac201b60140a5925192bbf";
	item.openinfo = "";
	item.perm = DBC_PERM_SYSTEM;
	users.clear();
	users.push_back(item);

	try {
		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("dbc.users")) {
			if (v.first != "user")
				continue;

			const bpt::iptree& v_pt = v.second;

			string user_name = v_pt.get<string>("usrname");
			if (user_name.length() > MAX_DBC_USER_NAME)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: users.user.usrname is too long.")).str(SGLOCALE));
			item.user_name = user_name;

			string password = v_pt.get<string>("password");
			if (password.length() > MAX_DBC_PASSWORD)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: users.user.password is too long.")).str(SGLOCALE));
			item.password = password;

			string dbname = get_value(v_pt, "dbname", string("ORACLE"));
			if (dbname.length() > MAX_DBNAME)
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: users.user.dbname is too long.")).str(SGLOCALE));
			item.dbname = dbname;

			string openinfo;
			env_mgr.expand_var(openinfo, get_value(v_pt, "openinfo", string("")));
			if (openinfo.length() > MAX_OPENINFO)
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: users.user.openinfo is too long.")).str(SGLOCALE));
			item.openinfo = openinfo;

			item.perm = 0;
			string options = get_value(v_pt, "perm", string(""));
			tokenizer tokens(options, sep);
			for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter) {
				string str = *iter;

				if (strcasecmp(str.c_str(), "INSERT") == 0)
					item.perm |= DBC_PERM_INSERT;
				else if (strcasecmp(str.c_str(), "UPDATE") == 0)
					item.perm |= DBC_PERM_UPDATE;
				else if (strcasecmp(str.c_str(), "DELETE") == 0)
					item.perm |= DBC_PERM_DELETE;
				else if (strcasecmp(str.c_str(), "SELECT") == 0)
					item.perm |= DBC_PERM_SELECT;
				else if (strcasecmp(str.c_str(), "CREATE") == 0)
					item.perm |= DBC_PERM_CREATE;
				else if (strcasecmp(str.c_str(), "DROP") == 0)
					item.perm |= DBC_PERM_DROP;
				else if (strcasecmp(str.c_str(), "ALTER") == 0)
					item.perm |= DBC_PERM_ALTER;
				else if (strcasecmp(str.c_str(), "TRUNCATE") == 0)
					item.perm |= DBC_PERM_TRUNCATE;
				else if (strcasecmp(str.c_str(), "RELOAD") == 0)
					item.perm |= DBC_PERM_RELOAD;
			}

			if (item.user_name != "system") {
				users.push_back(item);
			} else {
				users[0].password = item.password;
				users[0].dbname = item.dbname;
				users[0].openinfo = item.openinfo;
			}
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section users failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section users failure, {1}") % ex.what()).str(SGLOCALE));
	}
}


void dbc_config::load_data(const bpt::iptree& pt)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif
	gpenv& env_mgr = gpenv::instance();
	boost::char_separator<char> sep(" \t\b");

	try {
		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("dbc.tables")) {
			if (v.first != "table")
				continue;

			const bpt::iptree& v_pt = v.second;
			bool is_variable = false;
			int extra_size = 0;
			int struct_align = CHAR_ALIGN;
			int internal_align = INT_ALIGN;
			int struct_offset = 0;
			int internal_offset = 0;
			int table_id;
			dbc_data_t item;

			table_id = v_pt.get<int>("table_id");
			if (table_id <= 0 || table_id >= TE_MIN_RESERVED)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.table_id out of range.")).str(SGLOCALE));

			string table_name = boost::to_lower_copy(v_pt.get<string>("table_name"));
			if (table_name.length() > TABLE_NAME_SIZE)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.table_name is too long.")).str(SGLOCALE));
			strcpy(item.table_name, table_name.c_str());

			string dbtable_name;
			env_mgr.expand_var(dbtable_name, get_value(v_pt, "dbtable_name", table_name));
			if (dbtable_name.length() > TABLE_NAME_SIZE)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.dbtable_name is too long.")).str(SGLOCALE));
			boost::to_lower(dbtable_name);
			strcpy(item.dbtable_name, dbtable_name.c_str());

			string conditions;
			env_mgr.expand_var(conditions, get_value(v_pt, "conditions", string("")));
			if (conditions.length() > CONDITIONS_SIZE)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.conditions is too long.")).str(SGLOCALE));
			strcpy(item.conditions, conditions.c_str());

			string dbname = get_value(v_pt, "dbname", string(""));
			if (dbname.length() > MAX_DBNAME)
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: tables.table.dbname is too long.")).str(SGLOCALE));
			strcpy(item.dbname, dbname.c_str());

			string openinfo;
			env_mgr.expand_var(openinfo, get_value(v_pt, "openinfo", string("")));
			if (openinfo.length() > MAX_OPENINFO)
				throw sg_exception(__FILE__, __LINE__, DBCEINVAL, 0, (_("ERROR: tables.table.openinfo is too long.")).str(SGLOCALE));
			strcpy(item.openinfo, openinfo.c_str());

			item.escape = static_cast<char>(get_value(v_pt, "escape", 0));
			item.delimiter = static_cast<char>(get_value(v_pt, "delimiter", 0));
			item.segment_rows = v_pt.get<long>("segment_rows");
			if (item.segment_rows <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.segment_rows out of range.")).str(SGLOCALE));

			item.refresh_type = 0;
			string options = v_pt.get<string>("options");
			tokenizer tokens(options, sep);
			for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter) {
				string str = *iter;

				if (strcasecmp(str.c_str(), "DB") == 0)
					item.refresh_type |= REFRESH_TYPE_DB;
				else if (strcasecmp(str.c_str(), "FILE") == 0)
					item.refresh_type |= REFRESH_TYPE_FILE;
				else if (strcasecmp(str.c_str(), "BFILE") == 0)
					item.refresh_type |= REFRESH_TYPE_BFILE;
				else if (strcasecmp(str.c_str(), "INCREMENT") == 0)
					item.refresh_type |= REFRESH_TYPE_INCREMENT;
				else if (strcasecmp(str.c_str(), "IN_MEM") == 0)
					item.refresh_type |= REFRESH_TYPE_IN_MEM;
				else if (strcasecmp(str.c_str(), "NO_LOAD") == 0)
					item.refresh_type |= REFRESH_TYPE_NO_LOAD;
				else if (strcasecmp(str.c_str(), "AGG") == 0)
					item.refresh_type |= REFRESH_TYPE_AGG;
				else if (strcasecmp(str.c_str(), "DISCARD") == 0)
					item.refresh_type |= REFRESH_TYPE_DISCARD;
			}

			if (DBCP->_DBCP_proc_type & DBC_MASTER) {
				// Master模式下必须生成二进制文件
				item.refresh_type |= REFRESH_TYPE_BFILE;
				// Master模式下不会装载数据
				item.segment_rows = 1;
			} else if (DBCP->_DBCP_proc_type & DBC_SLAVE) {
				// Slave模式下只允许读取二进制文件，其文件由Master节点生成
				item.refresh_type |= REFRESH_TYPE_BFILE;
			}

			item.partition_type = 0;
			string partition_type = get_value(v_pt, "partition_type", string("STRING"));
			if (strcasecmp(partition_type.c_str(), "NUMBER") == 0)
				item.partition_type = PARTITION_TYPE_NUMBER;
			else if (strcasecmp(partition_type.c_str(), "STRING") == 0)
				item.partition_type = PARTITION_TYPE_STRING;
			else
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.partition_type value invalid.")).str(SGLOCALE));

			item.partitions = get_value(v_pt, "partitions", 1);
			item.field_size = 0;

			BOOST_FOREACH(const bpt::iptree::value_type& vf, v_pt.get_child("fields")) {
				if (vf.first != "field")
					continue;

				if (item.field_size >= MAX_FIELDS)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields too many children.")).str(SGLOCALE));

				const bpt::iptree& v_pf = vf.second;
				dbc_data_field_t& field = item.fields[item.field_size++];

				string field_name = boost::to_lower_copy(v_pf.get<string>("field_name"));
				if (field_name.length() > FIELD_NAME_SIZE)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.field_name is too long.")).str(SGLOCALE));
				strcpy(field.field_name, field_name.c_str());

				string fetch_name = v_pf.get<string>("fetch_name", field_name);
				if (fetch_name.length() > FETCH_NAME_SIZE)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.fetch_name is too long.")).str(SGLOCALE));
				strcpy(field.fetch_name, fetch_name.c_str());

				string update_name = v_pf.get<string>("update_name", string(":") + field_name);
				if (update_name.length() > UPDATE_NAME_SIZE)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.update_name is too long.")).str(SGLOCALE));
				strcpy(field.update_name, update_name.c_str());

				string column_name = boost::to_lower_copy(v_pf.get<string>("column_name", field_name));
				if (column_name.length() > COLUMN_NAME_SIZE)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.column_name is too long.")).str(SGLOCALE));
				strcpy(field.column_name, column_name.c_str());

				string field_type = v_pf.get<string>("field_type");
				if (strcasecmp(field_type.c_str(), "char") == 0)
					field.field_type = SQLTYPE_CHAR;
				else if (strcasecmp(field_type.c_str(), "uchar") == 0)
					field.field_type = SQLTYPE_UCHAR;
				else if (strcasecmp(field_type.c_str(), "short") == 0)
					field.field_type = SQLTYPE_SHORT;
				else if (strcasecmp(field_type.c_str(), "ushort") == 0)
					field.field_type = SQLTYPE_USHORT;
				else if (strcasecmp(field_type.c_str(), "int") == 0)
					field.field_type = SQLTYPE_INT;
				else if (strcasecmp(field_type.c_str(), "uint") == 0)
					field.field_type = SQLTYPE_UINT;
				else if (strcasecmp(field_type.c_str(), "long") == 0)
					field.field_type = SQLTYPE_LONG;
				else if (strcasecmp(field_type.c_str(), "ulong") == 0)
					field.field_type = SQLTYPE_ULONG;
				else if (strcasecmp(field_type.c_str(), "float") == 0)
					field.field_type = SQLTYPE_FLOAT;
				else if (strcasecmp(field_type.c_str(), "double") == 0)
					field.field_type = SQLTYPE_DOUBLE;
				else if (strcasecmp(field_type.c_str(), "string") == 0)
					field.field_type = SQLTYPE_STRING;
				else if (strcasecmp(field_type.c_str(), "vstring") == 0)
					field.field_type = SQLTYPE_VSTRING;
				else if (strcasecmp(field_type.c_str(), "date") == 0)
					field.field_type = SQLTYPE_DATE;
				else if (strcasecmp(field_type.c_str(), "time") == 0)
					field.field_type = SQLTYPE_TIME;
				else if (strcasecmp(field_type.c_str(), "datetime") == 0)
					field.field_type = SQLTYPE_DATETIME;
				else
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.field_type value invalid.")).str(SGLOCALE));

				switch (field.field_type) {
				case SQLTYPE_CHAR:
				case SQLTYPE_UCHAR:
					field.field_size = sizeof(char);
					break;
				case SQLTYPE_SHORT:
				case SQLTYPE_USHORT:
 					field.field_size = sizeof(short);
					struct_align = std::max(struct_align, SHORT_ALIGN);
					internal_align = std::max(internal_align, SHORT_ALIGN);
					struct_offset = common::round_up(struct_offset, SHORT_ALIGN);
					internal_offset = common::round_up(internal_offset, SHORT_ALIGN);
					break;
				case SQLTYPE_INT:
				case SQLTYPE_UINT:
					field.field_size = sizeof(int);
					struct_align = std::max(struct_align, INT_ALIGN);
					internal_align = std::max(internal_align, INT_ALIGN);
					struct_offset = common::round_up(struct_offset, INT_ALIGN);
					internal_offset = common::round_up(internal_offset, INT_ALIGN);
					break;
				case SQLTYPE_LONG:
				case SQLTYPE_ULONG:
					field.field_size = sizeof(long);
					struct_align = std::max(struct_align, LONG_ALIGN);
					internal_align = std::max(internal_align, LONG_ALIGN);
					struct_offset = common::round_up(struct_offset, LONG_ALIGN);
					internal_offset = common::round_up(internal_offset, LONG_ALIGN);
					break;
				case SQLTYPE_FLOAT:
					field.field_size = sizeof(float);
					struct_align = std::max(struct_align, FLOAT_ALIGN);
					internal_align = std::max(internal_align, FLOAT_ALIGN);
					struct_offset = common::round_up(struct_offset, FLOAT_ALIGN);
					internal_offset = common::round_up(internal_offset, FLOAT_ALIGN);
					break;
				case SQLTYPE_DOUBLE:
					field.field_size = sizeof(double);
					struct_align = std::max(struct_align, DOUBLE_ALIGN);
					internal_align = std::max(internal_align, DOUBLE_ALIGN);
					struct_offset = common::round_up(struct_offset, DOUBLE_ALIGN);
					internal_offset = common::round_up(internal_offset, DOUBLE_ALIGN);
					break;
				case SQLTYPE_STRING:
					field.field_size = v_pf.get<int>("field_size") + 1;
					break;
				case SQLTYPE_VSTRING:
					field.field_size = v_pf.get<int>("field_size") + 1;
					if (field.field_size < sizeof(long))
						throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.field_size should be at least {1} long when field_type is vstring") % (sizeof(long) - 1)).str(SGLOCALE));
					internal_align = std::max(internal_align, INT_ALIGN);
					internal_offset = common::round_up(internal_offset, INT_ALIGN);
					is_variable = true;
					extra_size += field.field_size;
					break;
				case SQLTYPE_DATE:
				case SQLTYPE_TIME:
				case SQLTYPE_DATETIME:
					field.field_size = sizeof(time_t);
					struct_align = std::max(struct_align, TIME_T_ALIGN);
					internal_align = std::max(internal_align, TIME_T_ALIGN);
					struct_offset = common::round_up(struct_offset, TIME_T_ALIGN);
					internal_offset = common::round_up(internal_offset, TIME_T_ALIGN);
					break;
				default:
					break;
				}

				string is_primary = v_pf.get<string>("is_primary", "N");
				if (strcasecmp(is_primary.c_str(), "Y") == 0)
					field.is_primary = true;
				else if (strcasecmp(is_primary.c_str(), "N") == 0)
					field.is_primary = false;
				else
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.is_primary value invalid.")).str(SGLOCALE));

				if (item.refresh_type & REFRESH_TYPE_AGG) {
					if (field.is_primary) {
						field.agg_type = AGGTYPE_NONE;
					} else {
						string agg_type = v_pf.get<string>("agg_type");
						if (strcasecmp(agg_type.c_str(), "sum") == 0) {
							if (field.field_type == SQLTYPE_CHAR || field.field_type == SQLTYPE_UCHAR
								|| field.field_type == SQLTYPE_STRING || field.field_type == SQLTYPE_VSTRING)
								throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.agg_type should not be sum for field_type char/uchar/string/vstring")).str(SGLOCALE));
							field.agg_type = AGGTYPE_SUM;
						} else if (strcasecmp(field_type.c_str(), "min") == 0) {
							field.agg_type = AGGTYPE_MIN;
						} else if (strcasecmp(field_type.c_str(), "max") == 0) {
							field.agg_type = AGGTYPE_MAX;
						} else {
							throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.agg_type should be sum/min/max")).str(SGLOCALE));
						}
					}
				}

				string is_partition = v_pf.get<string>("is_partition", "N");
				if (strcasecmp(is_partition.c_str(), "Y") == 0)
					field.is_partition = true;
				else if (strcasecmp(is_partition.c_str(), "N") == 0)
					field.is_partition = false;
				else
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.fields.field.is_partition value invalid.")).str(SGLOCALE));

				string partition_format = v_pf.get<string>("partition_format", "");
				if (partition_format.length() > PARTITION_FORMAT_SIZE)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.fields.field.partition_format is too long.")).str(SGLOCALE));
				strcpy(field.partition_format, partition_format.c_str());

				field.load_size = v_pf.get<int>("load_size", 0);
				if (field.load_size == 0) {
					if (field.field_type == SQLTYPE_STRING || field.field_type == SQLTYPE_VSTRING)
						field.load_size = field.field_size - 1;
					else
						field.load_size = field.field_size;
				}

				field.field_offset = struct_offset;
				struct_offset += field.field_size;
				field.internal_offset = internal_offset;
				if (field.field_type == SQLTYPE_VSTRING)
					internal_offset += sizeof(int);
				else
					internal_offset += field.field_size;
			}

			if (extra_size > MAX_VSLOTS * sizeof(long))
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.fields.field.field_size should be at most {1} long when field_type is vstring") % (MAX_VSLOTS * sizeof(long))).str(SGLOCALE));

			item.struct_size = common::round_up(struct_offset, struct_align);
			struct_align = std::max(struct_align, STRUCT_ALIGN);
			// 内部结构不需要对齐，因为最后一个字段是字符串型
			if (is_variable)
				item.internal_size = internal_offset;
			else
				item.internal_size = common::round_up(struct_offset, struct_align);
			item.extra_size = extra_size;
			data_map[table_id] = item;
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section tables failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section tables failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

void dbc_config::load_index(const bpt::iptree& pt)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif
	boost::char_separator<char> sep(" \t\b");

	try {
		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("dbc.indexes")) {
			if (v.first != "index")
				continue;

			const bpt::iptree& v_pt = v.second;
			dbc_index_key_t key;
			dbc_index_t item;

			key.table_id = v_pt.get<int>("table_id");
			if (key.table_id <= 0 || key.table_id >= TE_MIN_RESERVED)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.table_id out of range.")).str(SGLOCALE));

			key.index_id = v_pt.get<int>("index_id");
			if (key.index_id < 0 || key.index_id >= MAX_INDEXES)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.index_id out of range.")).str(SGLOCALE));

			string index_name = v_pt.get<string>("index_name");
			if (index_name.length() > INDEX_NAME_SIZE)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.index_name is too long.")).str(SGLOCALE));
			strcpy(item.index_name, index_name.c_str());

			item.index_type = 0;
			string index_type = v_pt.get<string>("index_type");
			tokenizer index_tokens(index_type, sep);
			for (tokenizer::iterator iter = index_tokens.begin(); iter != index_tokens.end(); ++iter) {
				string str = *iter;

				if (strcasecmp(str.c_str(), "PRIMARY") == 0)
					item.index_type |= INDEX_TYPE_PRIMARY;
				else if (strcasecmp(str.c_str(), "UNIQUE") == 0)
					item.index_type |= INDEX_TYPE_UNIQUE;
				else if (strcasecmp(str.c_str(), "NORMAL") == 0)
					item.index_type |= INDEX_TYPE_NORMAL;
				else if (strcasecmp(str.c_str(), "RANGE") == 0)
					item.index_type |= INDEX_TYPE_RANGE;
			}

			item.method_type = 0;
			string method_type = v_pt.get<string>("method_type");
			tokenizer method_tokens(method_type, sep);
			for (tokenizer::iterator iter = method_tokens.begin(); iter != method_tokens.end(); ++iter) {
				string str = *iter;

				if (strcasecmp(str.c_str(), "SEQ") == 0)
					item.method_type |= METHOD_TYPE_SEQ;
				else if (strcasecmp(str.c_str(), "BINRARY") == 0)
					item.method_type |= METHOD_TYPE_BINARY;
				else if (strcasecmp(str.c_str(), "HASH") == 0)
					item.method_type |= METHOD_TYPE_HASH;
				else if (strcasecmp(str.c_str(), "STRING") == 0)
					item.method_type |= METHOD_TYPE_STRING;
				else if (strcasecmp(str.c_str(), "NUMBER") == 0)
					item.method_type |= METHOD_TYPE_NUMBER;
			}

			item.buckets = get_value(v_pt, "buckets", DFLT_BUCKETS);
			if (item.buckets <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.buckets out of range.")).str(SGLOCALE));

			item.segment_rows = get_value(v_pt, "segment_rows", DFLT_SEGMENT_ROWS);
			if (item.segment_rows <= 0)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.segment_rows out of range.")).str(SGLOCALE));

			if (!(item.method_type & METHOD_TYPE_HASH))
				item.buckets = 1;

			if (item.index_type & INDEX_TYPE_PRIMARY)
				primary_map[key.table_id] = key.index_id;

			item.field_size = 0;

			BOOST_FOREACH(const bpt::iptree::value_type& vf, v_pt.get_child("fields")) {
				if (vf.first != "field")
					continue;

				if (item.field_size >= MAX_FIELDS)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.fields too many children.")).str(SGLOCALE));

				const bpt::iptree& v_pf = vf.second;
				dbc_index_field_t& field = item.fields[item.field_size++];

				string field_name = boost::to_lower_copy(v_pf.get<string>("field_name"));
				if (field_name.length() > FIELD_NAME_SIZE)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.fields.field.field_name is too long.")).str(SGLOCALE));
				strcpy(field.field_name, field_name.c_str());

				string is_hash = v_pf.get<string>("is_hash", "N");
				if (strcasecmp(is_hash.c_str(), "Y") == 0)
					field.is_hash = true;
				else if (strcasecmp(is_hash.c_str(), "N") == 0)
					field.is_hash = false;
				else
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.fields.field.is_hash value invalid.")).str(SGLOCALE));

				string hash_format = v_pf.get<string>("hash_format", "");
				if (hash_format.length() > HASH_FORMAT_SIZE)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: indexes.index.fields.field.hash_format is too long.")).str(SGLOCALE));
				strcpy(field.hash_format, hash_format.c_str());

				string search_type = v_pf.get<string>("search_type", "");
				std::transform(search_type.begin(), search_type.end(), search_type.begin(), toupper_predicate());
				field.search_type = 0;
				if (search_type.find("EQUAL") != string::npos || search_type.find("=") != string::npos)
					field.search_type |= SEARCH_TYPE_EQUAL;
				if (search_type.find("STRCHR") != string::npos)
					field.search_type |= SEARCH_TYPE_STRCHR;
				if (search_type.find("STRSTR") != string::npos)
					field.search_type |= SEARCH_TYPE_STRSTR;
				if (search_type.find("WILDCARD") != string::npos || search_type.find("*") != string::npos)
					field.search_type |= SEARCH_TYPE_WILDCARD;
				if (search_type.find("LESS") != string::npos || search_type.find("<") != string::npos)
					field.search_type |= SEARCH_TYPE_LESS;
				if (search_type.find("MORE")!= string::npos || search_type.find(">") != string::npos)
					field.search_type |= SEARCH_TYPE_MORE;
				if (search_type.find("IGNORE_CASE")!= string::npos)
					field.search_type |= SEARCH_TYPE_IGNORE_CASE;
				if (search_type.find("MATCH_N") != string::npos)
					field.search_type |= SEARCH_TYPE_MATCH_N;

				if (field.search_type == SEARCH_TYPE_WILDCARD)
					field.search_type |= SEARCH_TYPE_EQUAL;
				else if (field.search_type & SEARCH_TYPE_MATCH_N)
					field.search_type |= SEARCH_TYPE_EQUAL;

				field.special_type = v_pf.get<int>("special_type", 0);
				field.range_group = v_pf.get<int>("range_group", 0);
				field.range_offset = v_pf.get<int>("range_offset", 0);
			}

			if (DBCP->_DBCP_proc_type & DBC_MASTER) {
				// Master模式下不会装载数据
				item.buckets = 1;
				item.segment_rows = 1;
			}

			index_map[key] = item;
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section indexes failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section indexes failure, {1}") % ex.what()).str(SGLOCALE));
	}

	// 验证参数的合法性
	map<dbc_index_key_t, dbc_index_t>::iterator iter;
	for (iter = index_map.begin(); iter != index_map.end(); ++iter) {
		const dbc_index_key_t& dbc_key = iter->first;
		const dbc_index_t& dbc_index = iter->second;

		if ((dbc_index.index_type & INDEX_TYPE_RANGE) && !(dbc_index.method_type & METHOD_TYPE_SEQ))
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: METHOD_TYPE_SEQ should be set for method_type if index_type is INDEX_TYPE_RANGE, table_id={1}, index_id={2}") % dbc_key.table_id % dbc_key.index_id).str(SGLOCALE));

		int i;
		for (i = 0; i < dbc_index.field_size; i++) {
			const dbc_index_field_t& field = dbc_index.fields[i];
			if (field.range_group > 0) {
				int group_count = 0;
				for (int j = 0; j < dbc_index.field_size; j++) {
					if (i == j)
						continue;

					const dbc_index_field_t& field2 = dbc_index.fields[j];
					if (field2.range_group == -field.range_group) {
						if ((field2.search_type & field.search_type) || (field2.search_type | field.search_type)
							!= (SEARCH_TYPE_MORE | SEARCH_TYPE_EQUAL | SEARCH_TYPE_LESS))
							throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: search_type for range_group {1} is not a closure, table_id={2}, index_id={3}") % field.range_group % dbc_key.table_id % dbc_key.index_id).str(SGLOCALE));
						group_count++;
					}
				}

				if (group_count == 0)
					throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: missing corresponding field for range_group={1}, table_id={2}, index_id={3}") % field.range_group % dbc_key.table_id % dbc_key.index_id).str(SGLOCALE));
				else if (group_count > 1)
					throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: too many corresponding field for range_group={1}, table_id={2}, index_id={3}") % field.range_group % dbc_key.table_id % dbc_key.index_id).str(SGLOCALE));
			}
		}

		if (dbc_index.method_type & METHOD_TYPE_BINARY) {
			for (i = 0; i < dbc_index.field_size; i++) {
				const dbc_index_field_t& field = dbc_index.fields[i];

				if (field.search_type & ~(SEARCH_TYPE_MORE | SEARCH_TYPE_EQUAL
					| SEARCH_TYPE_LESS | SEARCH_TYPE_IGNORE_CASE))
					throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: search_type can only be in any combination of MORE, EQUAL, LESS and IGNORE_CASE, field_name={1}, table_id={2}, index_id={3}") % field.field_name % dbc_key.table_id % dbc_key.index_id).str(SGLOCALE));
			}
		}
	}
}

void dbc_config::load_func(const bpt::iptree& pt)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	try {
		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("dbc.functions")) {
			if (v.first != "function")
				continue;

			const bpt::iptree& v_pt = v.second;
			dbc_index_key_t key;
			dbc_func_t item;

			key.table_id = v_pt.get<int>("table_id");
			if (key.table_id <= 0 || key.table_id >= TE_MIN_RESERVED)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: functions.function.table_id out of range.")).str(SGLOCALE));

			key.index_id = v_pt.get<int>("index_id");
			if (key.index_id < 0 || key.index_id >= MAX_INDEXES)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: functions.function.index_id out of range.")).str(SGLOCALE));

			item.field_size = 0;

			BOOST_FOREACH(const bpt::iptree::value_type& vf, v_pt.get_child("fields")) {
				if (vf.first != "field")
					continue;

				if (item.field_size >= MAX_FIELDS)
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: functions.function.fields too many children.")).str(SGLOCALE));

				const bpt::iptree& v_pf = vf.second;
				dbc_func_field_t& field = item.fields[item.field_size++];

				field.field_name = boost::to_lower_copy(v_pf.get<string>("field_name"));

				string nullable = v_pf.get<string>("nullable", "N");
				if (strcasecmp(nullable.c_str(), "Y") == 0)
					field.nullable = true;
				else if (strcasecmp(nullable.c_str(), "N") == 0)
					field.nullable = false;
				else
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: functions.function.fields.field.nullable value invalid.")).str(SGLOCALE));

				field.field_ref = boost::to_lower_copy(v_pf.get<string>("field_ref", ""));
			}

			func_map[key] = item;
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section functions failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section functions failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

void dbc_config::load_libs(const string& libs)
{
	boost::char_separator<char> sep(" \t\b");
	string local_libs = libs;
	tokenizer tokens(local_libs, sep);
	for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter) {
		string lib_name = *iter;

		void *handle = ::dlopen(lib_name.c_str(), RTLD_LAZY);
		if (handle == NULL) {
			gpp_ctx *GPP = gpp_ctx::instance();
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't load library {1}, {2}") % lib_name % dlerror()).str(SGLOCALE));
			::exit(1);
		}

		lib_handles.push_back(handle);
	}
}

void dbc_config::unload_libs()
{
	BOOST_FOREACH(void *handle, lib_handles){
		::dlclose(handle);
	}

	lib_handles.clear();
}

void dbc_config::parse_perm(const string& perm_str, int& perm, const string& node_name)
{
	if (perm_str.empty()) {
		perm = DFLT_PERM;
	} else if (perm_str[0] != '0') {
		perm = 0;
		BOOST_FOREACH(char ch, perm_str) {
			if (ch < '0' || ch > '9')
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: {1} must be numeric.") % node_name).str(SGLOCALE));
			perm *= 10;
			perm += ch - '0';
		}
	} else {
		perm = 0;
		BOOST_FOREACH(char ch, perm_str) {
			if (ch < '0' || ch > '8')
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: {1} must be numeric.") % node_name).str(SGLOCALE));
			perm *= 8;
			perm += ch - '0';
		}
	}

	if (perm <= 0 || perm > 0777)
		throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: {1} must be between 1 and 0777.") % node_name).str(SGLOCALE));

	if ((perm & 0600) != 0600)
		throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: {1} must have permission 0600.") % node_name).str(SGLOCALE));
}

}
}

