#include "dbc_msvc.h"

using namespace std;

namespace ai
{
namespace sg
{

dbc_msvc::dbc_msvc()
	: api_mgr(dbc_api::instance())
{
	DBCT = dbct_ctx::instance();
}

dbc_msvc::~dbc_msvc()
{
}

svc_fini_t dbc_msvc::svc(message_pointer& svcinfo)
{
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(svcinfo->data());
	const char *data = rqst->data();

	message_pointer result_msg = sg_message::create(SGT);
	size_t reserve_size = sizeof(sdc_rply_t) - sizeof(long) + sizeof(int64_t);
	result_msg->reserve(reserve_size);
	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(result_msg->data());

	switch (rqst->opcode) {
	case SDC_OPCODE_CONNECT:
		{
			string user_name;
			string password;
			int total_size = rqst->datalen - sizeof(sdc_rqst_t) + sizeof(long);
			if (data[total_size - 1] != '\0')
				goto ERROR_LABEL;

			user_name = data;
			if (user_name.length() + 1 >= total_size)
				goto ERROR_LABEL;

			password = data + user_name.length() + 1;
			if (user_name.length() + password.length() + 2 != total_size)
				goto ERROR_LABEL;

			dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
			if (ue_mgr.connect(user_name, password)) {
				*reinterpret_cast<int64_t *>(rply->data()) = DBCT->_DBCT_user_id;
				rply->rows = 0;
				rply->datalen = sizeof(sdc_rply_t) - sizeof(long) + sizeof(int64_t);
				rply->error_code = 0;
				rply->native_code = 0;
			} else {
				rply->datalen = sizeof(sdc_rply_t) - sizeof(long);
				rply->rows = -1;
				rply->error_code = api_mgr.get_error_code();
				rply->native_code = api_mgr.get_native_code();
			}
		}
		break;
	case SDC_OPCODE_GET_TABLE:
		{
			string table_name;
			int total_size = rqst->datalen - sizeof(sdc_rqst_t) + sizeof(long);
			if (data[total_size - 1] != '\0')
				goto ERROR_LABEL;

			table_name = data;
			if (table_name.length() + 1 != total_size)
				goto ERROR_LABEL;

			if (!api_mgr.set_user(rqst->user_id))
				goto ERROR_LABEL;

			*reinterpret_cast<int *>(rply->data()) = api_mgr.get_table(table_name);
			rply->datalen = sizeof(sdc_rply_t) - sizeof(long) + sizeof(int);
			rply->rows = 1;
			rply->error_code = 0;
			rply->native_code = 0;
		}
		break;
	case SDC_OPCODE_GET_INDEX:
		{
			string table_name;
			string index_name;
			int total_size = rqst->datalen - sizeof(sdc_rqst_t) + sizeof(long);
			if (data[total_size - 1] != '\0')
				goto ERROR_LABEL;

			table_name = data;
			if (table_name.length() + 1 >= total_size)
				goto ERROR_LABEL;

			index_name = data + table_name.length() + 1;
			if (table_name.length() + index_name.length() + 2 != total_size)
				goto ERROR_LABEL;

			if (!api_mgr.set_user(rqst->user_id))
				goto ERROR_LABEL;

			*reinterpret_cast<int *>(rply->data()) = api_mgr.get_index(table_name, index_name);
			rply->datalen = sizeof(sdc_rply_t) - sizeof(long) + sizeof(int);
			rply->rows = 1;
			rply->error_code = 0;
			rply->native_code = 0;
		}
		break;
	case SDC_OPCODE_GET_META:
		{
			dbc_bboard_t *bboard = DBCT->_DBCT_bbp;
			dbc_bbparms_t& bbparms = bboard->bbparms;
			dbc_bbmeters_t& bbmeters = bboard->bbmeters;
			sdc_config& config_mgr = sdc_config::instance(DBCT);
			set<sdc_config_t>& config_set = config_mgr.get_config_set();

			int64_t user_id = -1;
			int i;
			dbc_ue_t *ue;
			// 第一个用户为系统用户，跳过
			for (i = 1, ue = DBCT->_DBCT_ues + 1; i < bbparms.maxusers; i++, ue++) {
				if (ue->in_use()) {
					user_id = ue->user_id;
					break;
				}
			}

			dbc_te_t *te;
			int table_size = 0;
			for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
				if (!te->in_active())
					continue;

				if (te->user_id != user_id)
					continue;

				table_size += 5 * sizeof(int);

				const dbc_data_t& te_meta = te->te_meta;
				for (int i = 0; i < te_meta.field_size; i++) {
					const dbc_data_field_t& field = te_meta.fields[i];
					if (field.is_partition) {
						table_size += 3 * sizeof(int);
						table_size += common::round_up(static_cast<int>(strlen(field.partition_format) + 1), static_cast<int>(sizeof(int)));
					}
				}
			}

			reserve_size += -sizeof(int64_t) + table_size + config_set.size() * sizeof(int) * 2;
			result_msg->reserve(reserve_size);
			rply = reinterpret_cast<sdc_rply_t *>(result_msg->data());
			rply->rows = 0;

			int *start_addr = reinterpret_cast<int *>(rply->data());
			for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
				if (!te->in_active())
					continue;

				if (te->user_id != user_id)
					continue;

				start_addr[0] = te->table_id;
				start_addr[1] = te->te_meta.partition_type;
				start_addr[2] = te->te_meta.partitions;
				start_addr[3] = te->te_meta.struct_size;

				int *data_addr = start_addr + 5;
				int fields = 0;
				const dbc_data_t& te_meta = te->te_meta;
				for (int i = 0; i < te_meta.field_size; i++) {
					const dbc_data_field_t& field = te_meta.fields[i];
					if (field.is_partition) {
						data_addr[0] = static_cast<int>(field.field_type);
						data_addr[1] = field.field_size;
						data_addr[2] = field.field_offset;
						data_addr += 3;
						int partition_len = strlen(field.partition_format);
						memcpy(data_addr, field.partition_format, partition_len + 1);
						data_addr += (partition_len + 1 + sizeof(int) - 1) / sizeof(int);
						fields++;
					}
				}

				start_addr[4] = fields;
				start_addr = data_addr;
				rply->rows++;
			}

			int rows = 0;
			sg_msghdr_t& msghdr = *svcinfo;
			for (set<sdc_config_t>::const_iterator iter = config_set.begin(); iter != config_set.end(); ++iter) {
				if (iter->mid != msghdr.sender.mid)
					continue;

				start_addr[0] = iter->table_id;
				start_addr[1] = iter->partition_id;
				start_addr += 2;
				rows++;
			}

			rply->datalen = sizeof(sdc_rply_t) - sizeof(long) + table_size + rows * sizeof(int) * 2;
			rply->error_code = 0;
			rply->native_code = 0;
		}
		break;
	default:
		goto ERROR_LABEL;
	}

	result_msg->set_length(rply->datalen);
	return svc_fini(SGSUCCESS, 0, result_msg);

ERROR_LABEL:
	rply->datalen = sizeof(sdc_rply_t) - sizeof(long);
	rply->rows = -1;
	rply->error_code = SGEINVAL;
	rply->native_code = 0;
	result_msg->set_length(rply->datalen);
	return svc_fini(SGSUCCESS, 0, result_msg);
}

}
}

