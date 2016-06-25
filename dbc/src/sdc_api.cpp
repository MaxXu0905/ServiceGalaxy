#include "sdc_api.h"

namespace ai
{
namespace sg
{

using namespace ai::scci;

size_t hash_value(const sdc_partition_t& rhs)
{
    size_t seed = boost::hash_value(rhs.table_id);
    boost::hash_combine(seed, rhs.partition_id);

    return seed;
}

boost::thread_specific_ptr<sdc_api> sdc_api::_instance;

const char DBC_METASVC[] = "DBC_METASVC";

sdc_api& sdc_api::instance()
{
	sdc_api *api_mgr = _instance.get();
	if (api_mgr == NULL) {
		api_mgr = new sdc_api();
		_instance.reset(api_mgr);
	}
	return *api_mgr;
}

sdc_api::sdc_api()
	: dbc_mgr(dbc_api::instance())
{
	_SDC_login = false;
	DBCT = dbct_ctx::instance();
	send_msg = sg_message::create(SGT);
	recv_msg = sg_message::create(SGT);
}

sdc_api::~sdc_api()
{
}

bool sdc_api::login()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", &retval);
#endif

	_SDC_login = true;

	GPP->write_log((_("INFO: client application login SDC successfully.")).str(SGLOCALE));
	retval = true;
	return retval;
}

void sdc_api::logout()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", NULL);
#endif

	if (!_SDC_login)
		return;

	_SDC_login = false;
	GPP->write_log((_("INFO: client application logout SDC successfully.")).str(SGLOCALE));
}

bool sdc_api::get_meta()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long);

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_GET_META;
	rqst->datalen = datalen;

	if (!api_mgr.call(DBC_METASVC, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows < 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
		return retval;
	}

	if (rply->datalen < sizeof(sdc_rply_t) - sizeof(long) + rply->rows * sizeof(int) * 5) {
		DBCT->_DBCT_error_code = DBCEPROTO;
		return retval;
	}

	int *start_addr = reinterpret_cast<int *>(rply->data());
	for (int i = 0; i < rply->rows; i++) {
		int table_id;
		sdc_meta_t item;

		table_id = start_addr[0];
		item.partition_type = start_addr[1];
		item.partitions = start_addr[2];
		item.struct_size = start_addr[3];

		int *data_addr = start_addr + 5;
		for (int j = 0; j < start_addr[4]; j++) {
			sdc_pmeta_t field;

			field.field_type = static_cast<sqltype_enum>(data_addr[0]);
			field.field_size = data_addr[1];
			field.field_offset = data_addr[2];

			char *partition_format = reinterpret_cast<char *>(&data_addr[3]);
			int partition_len = strlen(partition_format);
			if (partition_len == 0) {
				switch (field.field_type) {
				case SQLTYPE_CHAR:
					strcpy(field.partition_format, "%c");
					break;
				case SQLTYPE_UCHAR:
					strcpy(field.partition_format, "%uc");
					break;
				case SQLTYPE_SHORT:
				case SQLTYPE_INT:
					strcpy(field.partition_format, "%d");
					break;
				case SQLTYPE_USHORT:
				case SQLTYPE_UINT:
					strcpy(field.partition_format, "%ud");
					break;
				case SQLTYPE_LONG:
					strcpy(field.partition_format, "%ld");
					break;
				case SQLTYPE_ULONG:
					strcpy(field.partition_format, "%uld");
					break;
				case SQLTYPE_FLOAT:
					strcpy(field.partition_format, "%f");
					break;
				case SQLTYPE_DOUBLE:
					strcpy(field.partition_format, "%lf");
					break;
				case SQLTYPE_STRING:
				case SQLTYPE_VSTRING:
					strcpy(field.partition_format, "%s");
					break;
				case SQLTYPE_DATE:
				case SQLTYPE_TIME:
				case SQLTYPE_DATETIME:
					strcpy(field.partition_format, "%ld");
					break;
				case SQLTYPE_UNKNOWN:
				case SQLTYPE_ANY:
				default:
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: table {1}: Unknown field_type {2}") % table_id % field.field_type).str(SGLOCALE));
					return retval;
				}
			} else {
				memcpy(field.partition_format, partition_format, partition_len + 1);
			}

			item.fields.push_back(field);

			data_addr += 3 + (partition_len + 1 + sizeof(int) - 1) / sizeof(int);
		}

		start_addr = data_addr;
		meta_map[table_id] = item;
	}

	int rows = (rply->datalen - sizeof(sdc_rply_t) + sizeof(long) - (reinterpret_cast<char *>(start_addr) - rply->data())) / (sizeof(int) * 2);
	for (int i = 0; i < rows; i++) {
		sdc_partition_t item;

		item.table_id = start_addr[0];
		item.partition_id = start_addr[1];
		start_addr += 2;
		partition_set.insert(item);
	}

	if (!partition_set.empty()) {
		if (!dbc_mgr.login()) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't login on DBC - {1}") % DBCT->strerror()).str(SGLOCALE));
			return retval;
		}

		if (DBCT->_DBCT_user_id != -1) {
			if (!dbc_mgr.set_user(DBCT->_DBCT_user_id)) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't connect on DBC - {1}") % DBCT->strerror()).str(SGLOCALE));
				return retval;
			}
		}
	}

	retval = true;
	return retval;
}

bool sdc_api::connect(const string& user_name, const string& password)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, (_("user_name={1}, password={2}") % user_name % password).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + user_name.length() + password.length() + 2;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_CONNECT;
	rqst->datalen = datalen;
	memcpy(rqst->data(), user_name.c_str(), user_name.length() + 1);
	memcpy(rqst->data() + user_name.length() + 1, password.c_str(), password.length() + 1);

	if (!api_mgr.call(DBC_METASVC, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows < 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
		return retval;
	}

	if (rply->datalen != sizeof(sdc_rply_t) - sizeof(long) + sizeof(int64_t)) {
		DBCT->_DBCT_error_code = DBCEPROTO;
		return retval;
	}

	DBCT->_DBCT_user_id = *reinterpret_cast<int64_t *>(rply->data());

	retval = true;
	return retval;
}

bool sdc_api::connected() const
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", &retval);
#endif

	retval = (DBCT->_DBCT_user_id != -1);
	return retval;
}

void sdc_api::disconnect()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", NULL);
#endif

	DBCT->_DBCT_user_id = -1;
	meta_map.clear();
}

bool sdc_api::set_user(int64_t user_id)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(600, __PRETTY_FUNCTION__, (_("user_id={1}") % user_id).str(SGLOCALE), &retval);
#endif

	DBCT->_DBCT_user_id = user_id;
	retval = true;
	return retval;
}

int64_t sdc_api::get_user() const
{
	int64_t retval = -1;
#if defined(DEBUG)
	scoped_debug<int64_t> debug(600, __PRETTY_FUNCTION__, "", &retval);
#endif

	retval = DBCT->_DBCT_user_id;
	return retval;
}

int sdc_api::get_table(const string& table_name)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(900, __PRETTY_FUNCTION__, (_("table_name={1}") % table_name).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + table_name.length() + 1;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_GET_TABLE;
	rqst->datalen = datalen;
	rqst->user_id = DBCT->_DBCT_user_id;
	memcpy(rqst->data(), table_name.c_str(), table_name.length() + 1);

	if (!api_mgr.call(DBC_METASVC, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows < 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
		return retval;
	}

	retval = *reinterpret_cast<int *>(rply->data());
	return retval;
}

int sdc_api::get_index(const string& table_name, const string& index_name)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(900, __PRETTY_FUNCTION__, (_("table_name={1}, index_name={2}") % table_name % index_name).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + table_name.length() + index_name.length() + 2;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_GET_INDEX;
	rqst->datalen = datalen;
	rqst->user_id = DBCT->_DBCT_user_id;
	memcpy(rqst->data(), table_name.c_str(), table_name.length() + 1);
	memcpy(rqst->data() + table_name.length() + 1, index_name.c_str(), index_name.length() + 1);

	if (!api_mgr.call(DBC_METASVC, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows < 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
		return retval;
	}

	retval = *reinterpret_cast<int *>(rply->data());
	return retval;
}

long sdc_api::find_dynamic(int table_id, const void *data, void *result, long max_rows)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}, result={3}, max_rows={4}") % table_id % data % result % max_rows).str(SGLOCALE), &retval);
#endif

	int struct_size;
	int partition_id = get_partition(table_id, data, struct_size);
	if (partition_id < 0) {
		DBCT->_DBCT_error_code = DBCENOENT;
		return retval;
	}

	if (DBCT->_DBCT_login && local(table_id, partition_id)) {
		dbc_mgr.set_user(DBCT->_DBCT_user_id);
		retval = dbc_mgr.find(table_id, data, result, max_rows);
	} else {
		retval = find(table_id, data, result, max_rows, partition_id, struct_size);
	}

	return retval;
}

long sdc_api::find_dynamic(int table_id, int index_id, const void *data, void *result, long max_rows)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, data={3}, result={4}, max_rows={5}") % table_id % index_id % data % result % max_rows).str(SGLOCALE), &retval);
#endif

	int struct_size;
	int partition_id = get_partition(table_id, data, struct_size);
	if (partition_id < 0) {
		DBCT->_DBCT_error_code = DBCENOENT;
		return retval;
	}

	if (DBCT->_DBCT_login && local(table_id, partition_id)) {
		dbc_mgr.set_user(DBCT->_DBCT_user_id);
		retval = dbc_mgr.find(table_id, index_id, data, result, max_rows);
	} else {
		retval = find(table_id, index_id, data, result, max_rows, partition_id, struct_size);
	}

	return retval;
}

long sdc_api::erase_dynamic(int table_id, const  void *data)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}") % table_id % data).str(SGLOCALE), &retval);
#endif

	int struct_size;
	int partition_id = get_partition(table_id, data, struct_size);
	if (partition_id < 0) {
		DBCT->_DBCT_error_code = DBCENOENT;
		return retval;
	}

	retval = erase(table_id, data, partition_id, struct_size);
	return retval;
}

long sdc_api::erase_dynamic(int table_id, int index_id, const void *data)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, data={3}") % table_id % index_id % data).str(SGLOCALE), &retval);
#endif

	int struct_size;
	int partition_id = get_partition(table_id, data, struct_size);
	if (partition_id < 0) {
		DBCT->_DBCT_error_code = DBCENOENT;
		return retval;
	}

	retval = erase(table_id, index_id, data, partition_id, struct_size);
	return retval;
}

bool sdc_api::insert_dynamic(int table_id, const void *data)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}") % table_id % data).str(SGLOCALE), &retval);
#endif

	int struct_size;
	int partition_id = get_partition(table_id, data, struct_size);
	if (partition_id < 0) {
		DBCT->_DBCT_error_code = DBCENOENT;
		return retval;
	}

	retval = insert(table_id, data, partition_id, struct_size);
	return retval;
}

long sdc_api::update_dynamic(int table_id, const void *old_data, const void *new_data)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, old_data={2}, new_data={3}") % table_id % old_data % new_data).str(SGLOCALE), &retval);
#endif

	int struct_size;
	int partition_id = get_partition(table_id, old_data, struct_size);
	if (partition_id < 0) {
		DBCT->_DBCT_error_code = DBCENOENT;
		return retval;
	}

	retval = update(table_id, old_data, new_data, partition_id, struct_size);
	return retval;
}

long sdc_api::update_dynamic(int table_id, int index_id, const void *old_data, const void *new_data)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, old_data={3}, new_data={4}") % table_id % index_id % old_data % new_data).str(SGLOCALE), &retval);
#endif

	int struct_size;
	int partition_id = get_partition(table_id, old_data, struct_size);
	if (partition_id < 0) {
		DBCT->_DBCT_error_code = DBCENOENT;
		return retval;
	}

	retval = update(table_id, index_id, old_data, new_data, partition_id, struct_size);
	return retval;
}

long sdc_api::find(int table_id, const void *data, void *result, long max_rows,
	int partition_id, int struct_size)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}, result={3}, max_rows={4}, partition_id={5}, struct_size={6}") % table_id % data % result % max_rows % partition_id % struct_size).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + struct_size;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_FIND;
	rqst->datalen = datalen;
	rqst->struct_size = struct_size;
	rqst->max_rows = max_rows;
	rqst->flags = 0;
	rqst->user_id = DBCT->_DBCT_user_id;
	rqst->table_id = table_id;
	rqst->index_id = -1;
	memcpy(rqst->data(), data, struct_size);

	char svc_name[MAX_SVC_NAME + 1];
	sprintf(svc_name, "SDC_%d_%d", table_id, partition_id);

	if (!api_mgr.call(svc_name, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows < 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
	} else if (rply->rows > 0) {
		const char *rply_data = rply->data();
		memcpy(result, rply_data, struct_size * rply->rows);
	}

	retval = rply->rows;
	return retval;
}

long sdc_api::find(int table_id, int index_id, const void *data, void *result, long max_rows,
	int partition_id, int struct_size)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, data={3}, result={4}, max_rows={5}, partition_id={6}, struct_size={7}") % table_id % index_id % data % result % max_rows % partition_id % struct_size).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + struct_size;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_FIND;
	rqst->datalen = datalen;
	rqst->struct_size = struct_size;
	rqst->max_rows = max_rows;
	rqst->flags = 0;
	rqst->user_id = DBCT->_DBCT_user_id;
	rqst->table_id = table_id;
	rqst->index_id = index_id;
	memcpy(rqst->data(), data, struct_size);

	char svc_name[MAX_SVC_NAME + 1];
	sprintf(svc_name, "SDC_%d_%d", table_id, partition_id);

	if (!api_mgr.call(svc_name, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows < 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
	} else if (rply->rows > 0) {
		const char *rply_data = rply->data();
		memcpy(result, rply_data, struct_size * rply->rows);
	}

	retval = rply->rows;
	return retval;
}

long sdc_api::erase(int table_id, const void *data, int partition_id, int struct_size)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}, partition_id={3}, struct_size={4}") % table_id % data % partition_id % struct_size).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + struct_size;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_ERASE;
	rqst->datalen = datalen;
	rqst->struct_size = struct_size;
	rqst->max_rows = 0;
	rqst->flags = 0;
	rqst->user_id = DBCT->_DBCT_user_id;
	rqst->table_id = table_id;
	rqst->index_id = -1;
	memcpy(rqst->data(), data, struct_size);

	char svc_name[MAX_SVC_NAME + 1];
	sprintf(svc_name, "SDC_%d_%d", table_id, partition_id);

	if (!api_mgr.call(svc_name, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows <= 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
	}

	retval = rply->rows;
	return retval;
}

long sdc_api::erase(int table_id, int index_id, const void *data, int partition_id, int struct_size)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, data={3}, partition_id={4}, struct_size={5}") % table_id % index_id % data % partition_id % struct_size).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + struct_size;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_ERASE;
	rqst->datalen = datalen;
	rqst->struct_size = struct_size;
	rqst->max_rows = 0;
	rqst->flags = 0;
	rqst->user_id = DBCT->_DBCT_user_id;
	rqst->table_id = table_id;
	rqst->index_id = index_id;
	memcpy(rqst->data(), data, struct_size);

	char svc_name[MAX_SVC_NAME + 1];
	sprintf(svc_name, "SDC_%d_%d", table_id, partition_id);

	if (!api_mgr.call(svc_name, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows <= 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
	}

	retval = rply->rows;
	return retval;
}

bool sdc_api::insert(int table_id, const void *data, int partition_id, int struct_size)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}, partition_id={3}, struct_size={4}") % table_id % data % partition_id % struct_size).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + struct_size;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_INSERT;
	rqst->datalen = datalen;
	rqst->struct_size = struct_size;
	rqst->max_rows = 0;
	rqst->flags = 0;
	rqst->user_id = DBCT->_DBCT_user_id;
	rqst->table_id = table_id;
	rqst->index_id = -1;
	memcpy(rqst->data(), data, struct_size);

	char svc_name[MAX_SVC_NAME + 1];
	sprintf(svc_name, "SDC_%d_%d", table_id, partition_id);

	if (!api_mgr.call(svc_name, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows <= 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
	}

	retval = (rply->rows > 0);
	return retval;
}

long sdc_api::update(int table_id, const void *old_data, const void *new_data,
	int partition_id, int struct_size)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, old_data={2}, new_data={3}, partition_id={4}, struct_size={5}") % table_id % old_data % new_data % partition_id % struct_size).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + struct_size * 2;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_UPDATE;
	rqst->datalen = datalen;
	rqst->struct_size = struct_size;
	rqst->max_rows = 0;
	rqst->flags = 0;
	rqst->user_id = DBCT->_DBCT_user_id;
	rqst->table_id = table_id;
	rqst->index_id = -1;
	memcpy(rqst->data(), old_data, struct_size);
	memcpy(rqst->data() + struct_size, new_data, struct_size);

	char svc_name[MAX_SVC_NAME + 1];
	sprintf(svc_name, "SDC_%d_%d", table_id, partition_id);

	if (!api_mgr.call(svc_name, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows <= 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
	}

	retval = rply->rows;
	return retval;
}

long sdc_api::update(int table_id, int index_id, const void *old_data, const void *new_data,
	int partition_id, int struct_size)
{
	long retval = -1;
#if defined(DEBUG)
	scoped_debug<long> debug(500, __PRETTY_FUNCTION__, (_("table_id={1}, index_id={2}, old_data={3}, new_data={4}, partition_id={5}, struct_size={6}") % table_id % index_id % old_data % new_data % partition_id % struct_size).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(sdc_rqst_t) - sizeof(long) + struct_size * 2;

	send_msg->set_length(datalen);
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(send_msg->data());
	rqst->opcode = SDC_OPCODE_UPDATE;
	rqst->datalen = datalen;
	rqst->struct_size = struct_size;
	rqst->max_rows = 0;
	rqst->flags = 0;
	rqst->user_id = DBCT->_DBCT_user_id;
	rqst->table_id = table_id;
	rqst->index_id = index_id;
	memcpy(rqst->data(), old_data, struct_size);
	memcpy(rqst->data() + struct_size, new_data, struct_size);

	char svc_name[MAX_SVC_NAME + 1];
	sprintf(svc_name, "SDC_%d_%d", table_id, partition_id);

	if (!api_mgr.call(svc_name, send_msg, recv_msg, SGSIGRSTRT)) {
		// error_code already set
		return retval;
	}

	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(recv_msg->data());
	if (rply->rows <= 0) {
		DBCT->_DBCT_error_code = rply->error_code;
		DBCT->_DBCT_native_code = rply->native_code;
	}

	retval = rply->rows;
	return retval;
}

int sdc_api::get_error_code() const
{
	return DBCT->_DBCT_error_code;
}

int sdc_api::get_native_code() const
{
	return DBCT->_DBCT_native_code;
}

const char * sdc_api::strerror() const
{
	return DBCT->strerror();
}

const char * sdc_api::strnative() const
{
	return uunix::strerror(DBCT->_DBCT_native_code);
}

int sdc_api::get_ur_code() const
{
	return SGT->_SGT_ur_code;
}

void sdc_api::set_ur_code(int ur_code)
{
	SGT->_SGT_ur_code = ur_code;
}

int sdc_api::get_partition(int table_id, const void *data, int& struct_size)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(900, __PRETTY_FUNCTION__, (_("table_id={1}, data={2}") % table_id % data).str(SGLOCALE), &retval);
#endif
	char key[1024];

	boost::unordered_map<int, sdc_meta_t>::const_iterator iter = meta_map.find(table_id);
	if (iter == meta_map.end())
		return retval;

	const sdc_meta_t& meta = iter->second;
	char *key_ptr = key;

	key_ptr[0] = '\0';
	BOOST_FOREACH(const sdc_pmeta_t& field, meta.fields) {
		const char *data_addr = reinterpret_cast<const char *>(data) + field.field_offset;

		switch (field.field_type) {
		case SQLTYPE_CHAR:
			key_ptr += sprintf(key_ptr, field.partition_format, *data_addr);
			break;
		case SQLTYPE_UCHAR:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const unsigned char *>(data_addr));
			break;
		case SQLTYPE_SHORT:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const short *>(data_addr));
			break;
		case SQLTYPE_USHORT:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const unsigned short *>(data_addr));
			break;
		case SQLTYPE_INT:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const int *>(data_addr));
			break;
		case SQLTYPE_UINT:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const unsigned int *>(data_addr));
			break;
		case SQLTYPE_LONG:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const long *>(data_addr));
			break;
		case SQLTYPE_ULONG:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const unsigned long *>(data_addr));
			break;
		case SQLTYPE_FLOAT:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const float *>(data_addr));
			break;
		case SQLTYPE_DOUBLE:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const double *>(data_addr));
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			key_ptr += sprintf(key_ptr, field.partition_format, data_addr);
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			key_ptr += sprintf(key_ptr, field.partition_format, *reinterpret_cast<const time_t *>(data_addr));
			break;
		case SQLTYPE_UNKNOWN:
		case SQLTYPE_ANY:
		default:
			throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: table {1}: Unknown field_type {2}") % table_id % field.field_type).str(SGLOCALE));
		}
	}

	struct_size = meta.struct_size;
	retval = dbc_api::get_partition_id(meta.partition_type, meta.partitions, key);
	return retval;
}

bool sdc_api::local(int table_id, int partition_id) const
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(900, __PRETTY_FUNCTION__, "", &retval);
#endif
	sdc_partition_t key;

	key.table_id = table_id;
	key.partition_id = partition_id;
	retval = (partition_set.find(key) != partition_set.end());
	return retval;
}

}
}

