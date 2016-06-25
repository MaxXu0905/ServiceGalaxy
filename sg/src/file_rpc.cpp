#include "file_rpc.h"
#include "file_struct.h"

namespace ai
{
namespace sg
{

/* 需要调整优先级，
 * 保证除了文件传输外的操作优先处理
 */

file_rpc& file_rpc::instance(sgt_ctx *SGT)
{
	return *reinterpret_cast<file_rpc *>(SGT->_SGT_mgr_array[FRPC_MANAGER]);
}

bool file_rpc::dir(vector<string>& files, const string& sftp_prefix, const string& dir_name, const string& pattern)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("sftp_prefix={1}, dir_name={2}, pattern={3}") % sftp_prefix % dir_name % pattern).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(file_rqst_t) - sizeof(long) + sftp_prefix.length() + dir_name.length() + pattern.length() + 3;

	send_msg->set_length(datalen);
	file_rqst_t *rqst = reinterpret_cast<file_rqst_t *>(send_msg->data());
	rqst->opcode = FILE_OPCODE_DIR;
	rqst->datalen = datalen;

	char *data = rqst->data();
	memcpy(data, sftp_prefix.c_str(), sftp_prefix.length() + 1);
	data += sftp_prefix.length() + 1;
	memcpy(data, dir_name.c_str(), dir_name.length() + 1);
	data += dir_name.length() + 1;
	memcpy(data, pattern.c_str(), pattern.length() + 1);

	sgc_ctx *SGC = SGT->SGC();
	SGC->_SGC_special_mid = SGC->_SGC_proc.mid;
	BOOST_SCOPE_EXIT((&SGC)) {
		SGC->_SGC_special_mid = BADMID;
	} BOOST_SCOPE_EXIT_END

	// 提高优先级
	api_mgr.setprio(1, 0);

	if (!api_mgr.call(FMNGR_OSVC, send_msg, recv_msg, 0)) {
		// error_code already set
		return retval;
	}

	file_rply_t *rply = reinterpret_cast<file_rply_t *>(recv_msg->data());
	if (rply->error_code) {
		// error_code already set
		return retval;
	}

	const char *rply_data = rply->data();
	const char *rply_end = reinterpret_cast<const char *>(rply) + rply->datalen;
	while (rply_data < rply_end) {
		string file = rply_data;
		rply_data += file.length() + 1;

		files.push_back(file);
	}

	retval = true;
	return retval;
}

bool file_rpc::mkdir(const string& sftp_prefix, const string& dir_name)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("sftp_prefix={1}, dir_name={2}") % sftp_prefix % dir_name).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(file_rqst_t) - sizeof(long) + sftp_prefix.length() + dir_name.length() + 2;

	send_msg->set_length(datalen);
	file_rqst_t *rqst = reinterpret_cast<file_rqst_t *>(send_msg->data());
	rqst->opcode = FILE_OPCODE_MKDIR;
	rqst->datalen = datalen;

	char *data = rqst->data();
	memcpy(data, sftp_prefix.c_str(), sftp_prefix.length() + 1);
	data += sftp_prefix.length() + 1;
	memcpy(data, dir_name.c_str(), dir_name.length() + 1);

	sgc_ctx *SGC = SGT->SGC();
	SGC->_SGC_special_mid = SGC->_SGC_proc.mid;
	BOOST_SCOPE_EXIT((&SGC)) {
		SGC->_SGC_special_mid = BADMID;
	} BOOST_SCOPE_EXIT_END

	// 提高优先级
	api_mgr.setprio(1, 0);

	if (!api_mgr.call(FMNGR_OSVC, send_msg, recv_msg, 0)) {
		// error_code already set
		return retval;
	}

	file_rply_t *rply = reinterpret_cast<file_rply_t *>(recv_msg->data());
	if (rply->error_code) {
		// error_code already set
		return retval;
	}

	retval = true;
	return retval;
}

bool file_rpc::unlink(const string& sftp_prefix, const string& filename)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("sftp_prefix={1}, filename={2}") % sftp_prefix % filename).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(file_rqst_t) - sizeof(long) + sftp_prefix.length() + filename.length() + 2;

	send_msg->set_length(datalen);
	file_rqst_t *rqst = reinterpret_cast<file_rqst_t *>(send_msg->data());
	rqst->opcode = FILE_OPCODE_UNLINK;
	rqst->datalen = datalen;

	char *data = rqst->data();
	memcpy(data, sftp_prefix.c_str(), sftp_prefix.length() + 1);
	data += sftp_prefix.length() + 1;
	memcpy(data, filename.c_str(), filename.length() + 1);

	sgc_ctx *SGC = SGT->SGC();
	SGC->_SGC_special_mid = SGC->_SGC_proc.mid;
	BOOST_SCOPE_EXIT((&SGC)) {
		SGC->_SGC_special_mid = BADMID;
	} BOOST_SCOPE_EXIT_END

	// 提高优先级
	api_mgr.setprio(1, 0);

	if (!api_mgr.call(FMNGR_OSVC, send_msg, recv_msg, 0)) {
		// error_code already set
		return retval;
	}

	file_rply_t *rply = reinterpret_cast<file_rply_t *>(recv_msg->data());
	if (rply->error_code) {
		// error_code already set
		return retval;
	}

	retval = true;
	return retval;
}

bool file_rpc::rename(const string& sftp_prefix, const string& src_name, const string& dst_name)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("sftp_prefix={1}, src_name={2}, dst_name={3}") % sftp_prefix % src_name % dst_name).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(file_rqst_t) - sizeof(long) + sftp_prefix.length() + src_name.length() + dst_name.length() + 3;

	send_msg->set_length(datalen);
	file_rqst_t *rqst = reinterpret_cast<file_rqst_t *>(send_msg->data());
	rqst->opcode = FILE_OPCODE_RENAME;
	rqst->datalen = datalen;

	char *data = rqst->data();
	memcpy(data, sftp_prefix.c_str(), sftp_prefix.length() + 1);
	data += sftp_prefix.length() + 1;
	memcpy(data, src_name.c_str(), src_name.length() + 1);
	data += src_name.length() + 1;
	memcpy(data, dst_name.c_str(), dst_name.length() + 1);

	sgc_ctx *SGC = SGT->SGC();
	SGC->_SGC_special_mid = SGC->_SGC_proc.mid;
	BOOST_SCOPE_EXIT((&SGC)) {
		SGC->_SGC_special_mid = BADMID;
	} BOOST_SCOPE_EXIT_END

	// 提高优先级
	api_mgr.setprio(1, 0);

	if (!api_mgr.call(FMNGR_OSVC, send_msg, recv_msg, 0)) {
		// error_code already set
		return retval;
	}

	file_rply_t *rply = reinterpret_cast<file_rply_t *>(recv_msg->data());
	if (rply->error_code) {
		// error_code already set
		return retval;
	}

	retval = true;
	return retval;
}

bool file_rpc::get(const string& sftp_prefix, const string& src_name, const string& dst_name)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("sftp_prefix={1}, src_name={2}, dst_name={3}") % sftp_prefix % src_name % dst_name).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(file_rqst_t) - sizeof(long) + sftp_prefix.length() + src_name.length() + dst_name.length() + 3;

	send_msg->set_length(datalen);
	file_rqst_t *rqst = reinterpret_cast<file_rqst_t *>(send_msg->data());
	rqst->opcode = FILE_OPCODE_GET;
	rqst->datalen = datalen;

	char *data = rqst->data();
	memcpy(data, sftp_prefix.c_str(), sftp_prefix.length() + 1);
	data += sftp_prefix.length() + 1;
	memcpy(data, src_name.c_str(), src_name.length() + 1);
	data += src_name.length() + 1;
	memcpy(data, dst_name.c_str(), dst_name.length() + 1);

	sgc_ctx *SGC = SGT->SGC();
	SGC->_SGC_special_mid = SGC->_SGC_proc.mid;
	BOOST_SCOPE_EXIT((&SGC)) {
		SGC->_SGC_special_mid = BADMID;
	} BOOST_SCOPE_EXIT_END

	if (!api_mgr.call(FMNGR_GSVC, send_msg, recv_msg, 0)) {
		// error_code already set
		return retval;
	}

	file_rply_t *rply = reinterpret_cast<file_rply_t *>(recv_msg->data());
	if (rply->error_code) {
		// error_code already set
		return retval;
	}

	retval = true;
	return retval;
}

bool file_rpc::put(const string& sftp_prefix, const string& src_name, const string& dst_name)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(500, __PRETTY_FUNCTION__, (_("sftp_prefix={1}, src_name={2}, dst_name={3}") % sftp_prefix % src_name % dst_name).str(SGLOCALE), &retval);
#endif
	sg_api& api_mgr = sg_api::instance(SGT);
	int datalen = sizeof(file_rqst_t) - sizeof(long) + sftp_prefix.length() + src_name.length() + dst_name.length() + 3;

	send_msg->set_length(datalen);
	file_rqst_t *rqst = reinterpret_cast<file_rqst_t *>(send_msg->data());
	rqst->opcode = FILE_OPCODE_PUT;
	rqst->datalen = datalen;

	char *data = rqst->data();
	memcpy(data, sftp_prefix.c_str(), sftp_prefix.length() + 1);
	data += sftp_prefix.length() + 1;
	memcpy(data, src_name.c_str(), src_name.length() + 1);
	data += src_name.length() + 1;
	memcpy(data, dst_name.c_str(), dst_name.length() + 1);

	sgc_ctx *SGC = SGT->SGC();
	SGC->_SGC_special_mid = SGC->_SGC_proc.mid;
	BOOST_SCOPE_EXIT((&SGC)) {
		SGC->_SGC_special_mid = BADMID;
	} BOOST_SCOPE_EXIT_END

	if (!api_mgr.call(FMNGR_PSVC, send_msg, recv_msg, 0)) {
		// error_code already set
		return retval;
	}

	file_rply_t *rply = reinterpret_cast<file_rply_t *>(recv_msg->data());
	if (rply->error_code) {
		// error_code already set
		return retval;
	}

	retval = true;
	return retval;
}

file_rpc::file_rpc()
{
	send_msg = sg_message::create(SGT);
	recv_msg = sg_message::create(SGT);
}

file_rpc::~file_rpc()
{
}

}
}

