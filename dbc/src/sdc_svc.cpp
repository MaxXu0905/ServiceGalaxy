#include "sdc_svc.h"

using namespace std;

namespace ai
{
namespace sg
{

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

sdc_svc::sdc_svc()
	: api_mgr(dbc_api::instance())
{
	DBCT = dbct_ctx::instance();
	result_msg = sg_message::create(SGT);
}

sdc_svc::~sdc_svc()
{
}

svc_fini_t sdc_svc::svc(message_pointer& svcinfo)
{
	sdc_rqst_t *rqst = reinterpret_cast<sdc_rqst_t *>(svcinfo->data());
	int& struct_size = rqst->struct_size;
	int& table_id = rqst->table_id;
	int& index_id = rqst->index_id;
	const char *data = rqst->data();

	size_t reserve_size = sizeof(sdc_rply_t) - sizeof(long) + struct_size * rqst->max_rows;
	result_msg->reserve(reserve_size);
	sdc_rply_t *rply = reinterpret_cast<sdc_rply_t *>(result_msg->data());
	char *result = rply->data();

	// ÇÐ»»ÓÃ»§
	if (!api_mgr.set_user(rqst->user_id))
		return svc_fini(SGFAIL, rqst->table_id, result_msg);

	switch (rqst->opcode) {
	case SDC_OPCODE_FIND:
		if (index_id >= 0)
			rply->rows = api_mgr.find(table_id, index_id, data, result, rqst->max_rows);
		else
			rply->rows = api_mgr.find(table_id, data, result, rqst->max_rows);

		if (rply->rows > 0)
			rply->datalen = sizeof(sdc_rply_t) - sizeof(long) + struct_size * rply->rows;
		else
			rply->datalen = sizeof(sdc_rply_t) - sizeof(long);
		break;
	case SDC_OPCODE_ERASE:
		if (index_id >= 0)
			rply->rows = api_mgr.erase(table_id, index_id, data);
		else
			rply->rows = api_mgr.erase(table_id, data);

		rply->datalen = sizeof(sdc_rply_t) - sizeof(long);
		break;
	case SDC_OPCODE_INSERT:
		if (api_mgr.insert(table_id, data))
			rply->rows = 1;
		else
			rply->rows = 0;

		rply->datalen = sizeof(sdc_rply_t) - sizeof(long);
		break;
	case SDC_OPCODE_UPDATE:
		{
			const char *new_data = data + struct_size;
			if (index_id >= 0)
				rply->rows = api_mgr.update(table_id, index_id, data, new_data);
			else
				rply->rows = api_mgr.update(table_id, data, new_data);

			rply->datalen = sizeof(sdc_rply_t) - sizeof(long);
		}
		break;
	default:
		goto ERROR_LABEL;
	}

	if (rply->rows <= 0) {
		rply->error_code = api_mgr.get_error_code();
		rply->native_code = api_mgr.get_native_code();
	} else {
		rply->error_code = 0;
		rply->native_code = 0;
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

