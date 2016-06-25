#include "dbc_internal.h"

namespace ai
{
namespace sg
{

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
using namespace std;

sdc_config& sdc_config::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<sdc_config *>(DBCT->_DBCT_mgr_array[SDC_CONFIG_MANAGER]);
}

set<sdc_config_t>& sdc_config::get_config_set()
{
	if (!loaded)
		load();

	return config_set;
}

int sdc_config::get_partitions(int table_id)
{
	if (!loaded)
		load();

	map<int, int>::const_iterator iter = partitions_map.find(table_id);
	if (iter == partitions_map.end())
		return -1;
	else
		return iter->second;
}

sdc_config::sdc_config()
	: loaded(false)
{
}

sdc_config::~sdc_config()
{
}

void sdc_config::load()
{
	bpt::iptree pt;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (loaded)
		return;

	if (DBCP->_DBCP_sdcconfig.empty()) {
		char *ptr = ::getenv("SDCPROFILE");
		if (ptr == NULL)
			throw sg_exception(__FILE__, __LINE__, DBCESYSTEM, 0, (_("ERROR: SDCPROFILE environment variable not set")).str(SGLOCALE));
		DBCP->_DBCP_sdcconfig = ptr;
	}

	bpt::read_xml(DBCP->_DBCP_sdcconfig, pt);

	load_table(pt);
	loaded = true;
}

void sdc_config::load_table(const bpt::iptree& pt)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", NULL);
#endif
	sgc_ctx *SGC = SGT->SGC();
	dbc_config& config_mgr = dbc_config::instance();
	gpenv& env_mgr = gpenv::instance();

	if (!config_mgr.open())
		throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: Can't open DBCPROFILE.")).str(SGLOCALE));

	map<int, dbc_data_t>& data_map = config_mgr.get_data_map();
	map<dbc_index_key_t, dbc_index_t>& index_map = config_mgr.get_index_map();
	map<string, int> table_map;
	set<int> correct_tables;
	map<int, dbc_data_t>::const_iterator data_iter;
	map<dbc_index_key_t, dbc_index_t>::const_iterator index_iter;
	set<int>::iterator correct_table_iter;

	// 设置表名与表ID之间的映射关系
	for (data_iter = data_map.begin(); data_iter != data_map.end(); ++data_iter)
		table_map[data_iter->second.table_name] = data_iter->first;

	// 初始化正确的表
	for (index_iter = index_map.begin(); index_iter != index_map.end(); ++index_iter) {
		const dbc_index_key_t& index_key = index_iter->first;

		correct_tables.insert(index_key.table_id);
	}

	// 去除索引不正确的表
	for (index_iter = index_map.begin(); index_iter != index_map.end(); ++index_iter) {
		const dbc_index_key_t& index_key = index_iter->first;
		const dbc_index_t& index = index_iter->second;

		data_iter = data_map.find(index_key.table_id);
		if (data_iter == data_map.end())
			continue;

		const dbc_data_t& data = data_iter->second;
		for (int i = 0; i < data.field_size; i++) {
			const dbc_data_field_t& data_field = data.fields[i];

			if (!data_field.is_partition)
				continue;

			bool found = false;
			for (int j = 0; j < index.field_size; j++) {
				const dbc_index_field_t& index_field = index.fields[j];

				if (!index_field.is_hash)
					continue;

				if (strcasecmp(data_field.field_name, index_field.field_name) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				correct_table_iter = correct_tables.find(index_key.table_id);
				if (correct_table_iter != correct_tables.end()) {
					GPP->write_log((_("WARN: Index {1} on table {2} - parition field {3} is not of hash key") % index.index_name % data.table_name % data_field.field_name).str(SGLOCALE));
					correct_tables.erase(index_key.table_id);
				}
				break;
			}
		}
	}

	try {
		BOOST_FOREACH(const bpt::iptree::value_type& v, pt.get_child("sdc.tables")) {
			if (v.first != "table")
				continue;

			const bpt::iptree& v_pt = v.second;
			sdc_config_t item;

			item.table_name = boost::to_lower_copy(v_pt.get<string>("table_name"));
			if (item.table_name.length() > TABLE_NAME_SIZE)
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.table_name is too long.")).str(SGLOCALE));

			map<string, int>::const_iterator table_iter = table_map.find(item.table_name);
			if (table_iter == table_map.end())
				throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: table {1} is not defined in DBCPROFILE.") % item.table_name).str(SGLOCALE));

			item.table_id = table_iter->second;

			correct_table_iter = correct_tables.find(item.table_id);
			if (correct_table_iter == correct_tables.end()) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Operation for table {1} is not advertised for lack of its definition") % table_iter->first).str(SGLOCALE));
				continue;
			}

			int partitions = 0;
			if (v_pt.find("hostid") != v_pt.not_found()) {
				vector<string> the_vector;
				string lmid;
				env_mgr.expand_var(lmid, v_pt.get<string>("hostid"));
				boost::char_separator<char> sep(" \t\b");
				tokenizer tokens(lmid, sep);
				for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
					the_vector.push_back(*iter);

				int redundancy;
				if (!the_vector.empty()) {
					redundancy = get_value(v_pt, "redundancy", static_cast<int>(the_vector.size()));
					if (redundancy < 0)
						redundancy = the_vector.size();
					else if (redundancy > the_vector.size())
						throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: tables.table.redundancy value invalid.")).str(SGLOCALE));
				} else {
					redundancy = 0;
				}

				data_iter = data_map.find(item.table_id);
				if (data_iter == data_map.end())
					throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: table {1} is not defined in DBCPROFILE.") % item.table_name).str(SGLOCALE));

				partitions = data_iter->second.partitions;
				if (partitions < the_vector.size())
					partitions = the_vector.size();

				int offset = 0;
				BOOST_FOREACH(const string& str, the_vector) {
					item.mid = SGC->lmid2mid(str.c_str());
					if (item.mid == BADMID)
						throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: hostid {1} is not defined in SGPROFILE.") % str).str(SGLOCALE));

					for (int i = 0; i < redundancy; i++) {
						item.partition_id = (offset + i) % partitions;
						config_set.insert(item);
					}

					offset++;
				}
			} else if (v_pt.find("partitions") != v_pt.not_found()) {
				partitions = 0;
				BOOST_FOREACH(const bpt::iptree::value_type& vf, v_pt.get_child("partitions")) {
					if (vf.first != "partition")
						continue;

					const bpt::iptree& v_pf = vf.second;
					boost::char_separator<char> sep(" \t\b");

					string partition_id = v_pf.get<string>("partition_id");

					vector<string> the_vector2;
					tokenizer partition_tokens(partition_id, sep);
					for (tokenizer::iterator iter = partition_tokens.begin(); iter != partition_tokens.end(); ++iter)
						the_vector2.push_back(*iter);

					string lmid;
					env_mgr.expand_var(lmid, v_pt.get<string>("hostid"));
					tokenizer lmid_tokens(lmid, sep);
					for (tokenizer::iterator iter = lmid_tokens.begin(); iter != lmid_tokens.end(); ++iter) {
						string v1 = *iter;

						item.mid = SGC->lmid2mid(v1.c_str());
						if (item.mid == BADMID)
							throw sg_exception(__FILE__, __LINE__, SGEINVAL, 0, (_("ERROR: hostid {1} is not defined in SGPROFILE.") % v1).str(SGLOCALE));

						BOOST_FOREACH(const string& v2, the_vector2) {
							item.partition_id = atoi(v2.c_str());
							config_set.insert(item);

							if (partitions < item.partition_id + 1)
								partitions = item.partition_id + 1;
						}
					}
				}
			}

			// 设置表的分区数
			map<int, int>::iterator iter = partitions_map.find(item.table_id);
			if (iter == partitions_map.end()) {
				partitions_map[item.table_id] = partitions;
			} else {
				if (iter->second < partitions)
					iter->second = partitions;
			}
		}
	} catch (bpt::ptree_bad_data& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section tables failure, {1} ({2})") % ex.what() % ex.data<string>()).str(SGLOCALE));
	} catch (bpt::ptree_error& ex) {
		throw sg_exception(__FILE__, __LINE__, SGESYSTEM, 0, (_("ERROR: Parse section tables failure, {1}") % ex.what()).str(SGLOCALE));
	}
}

}
}

