#include "file_svc.h"
#include "file_struct.h"
#include "scan_file.h"

using namespace std;

namespace ai
{
namespace sg
{

int const RETRIES = 3;
typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

file_svc::file_svc()
{
	SSHP = sshp_ctx::instance();
	result_msg = sg_message::create(SGT);
}

file_svc::~file_svc()
{
}

svc_fini_t file_svc::svc(message_pointer& svcinfo)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	file_rqst_t *rqst = reinterpret_cast<file_rqst_t *>(svcinfo->data());
	const char *data = rqst->data();
	file_rply_t *rply;
	char *result;
	size_t datalen;
	int retval = 0;
	int retry;
	string sftp_prefix;

	if (rqst->opcode < FILE_OPCODE_MIN || rqst->opcode > FILE_OPCODE_MAX)
		goto ERROR_LABEL;

	retry = 0;
	sftp_prefix = data;
	data += sftp_prefix.length() + 1;

	while (1) {
		try {
			switch (rqst->opcode) {
			case FILE_OPCODE_DIR:
				{
					string dir = data;
					data += dir.length() + 1;

					string pattern = data;
					vector<string> files;

					scan_file<> scan(dir, pattern);
					scan.set_sftp(sftp_prefix);
					scan.get_files(files);

					datalen = sizeof(file_rply_t) - sizeof(long);
					BOOST_FOREACH(const string& file, files) {
						datalen += file.length() + 1;
					}

					result_msg->set_length(datalen);

					rply = reinterpret_cast<file_rply_t *>(result_msg->data());
					rply->datalen = datalen;
					rply->error_code = 0;
					rply->native_code = 0;

					result = rply->data();
					BOOST_FOREACH(const string& file, files) {
						memcpy(result, file.c_str(), file.length() + 1);
						result += file.length() + 1;
					}
				}
				break;
			case FILE_OPCODE_UNLINK:
				{
					const char *filename = data;

					sftp_shared_ptr auto_sftp(sftp_prefix);
					LIBSSH2_SFTP *sftp = auto_sftp.get();

					retval = libssh2_sftp_unlink(reinterpret_cast<LIBSSH2_SFTP *>(sftp), filename);
					if (retval == LIBSSH2_ERROR_SFTP_PROTOCOL) {
						goto ERROR_LABEL;
					} else if (retval) {
						if (++retry == RETRIES) {
							GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't unlink file {1} on node {2} - {3}") % filename % sftp_prefix % auto_sftp.get_error()).str(SGLOCALE));
							goto ERROR_LABEL;
						}

						goto SSH_ERROR;
					}

					datalen = sizeof(file_rply_t) - sizeof(long);
					result_msg->set_length(datalen);

					rply = reinterpret_cast<file_rply_t *>(result_msg->data());
					rply->datalen = datalen;
					rply->error_code = 0;
					rply->native_code = 0;
				}
				break;
			case FILE_OPCODE_MKDIR:
				{
					string dir_name = data;
					dir_name += "/";

					boost::char_separator<char> sep("/");
					tokenizer tokens(dir_name, sep);

					sftp_shared_ptr auto_sftp(sftp_prefix);
					LIBSSH2_SFTP *sftp = auto_sftp.get();

					string part;
					for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter) {
						part += "/";
						part += *iter;

						retval = libssh2_sftp_mkdir(sftp, part.c_str(), 0755);
						if (retval == LIBSSH2_ERROR_SFTP_PROTOCOL) {
							int last_error = libssh2_sftp_last_error(sftp);
							if (last_error != LIBSSH2_FX_FAILURE && last_error != LIBSSH2_FX_FILE_ALREADY_EXISTS) {
								GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create directory {1} on node {2} - {3}") % part % sftp_prefix % auto_sftp.get_error()).str(SGLOCALE));
								goto ERROR_LABEL;
							}
						} else if (retval) {
							if (++retry == RETRIES) {
								GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create directory {1} on node {2} - {3}") % part % sftp_prefix % auto_sftp.get_error()).str(SGLOCALE));
								goto ERROR_LABEL;
							}

							goto SSH_ERROR;
						}
					}

					datalen = sizeof(file_rply_t) - sizeof(long);
					result_msg->set_length(datalen);

					rply = reinterpret_cast<file_rply_t *>(result_msg->data());
					rply->datalen = datalen;
					rply->error_code = 0;
					rply->native_code = 0;
				}
				break;
			case FILE_OPCODE_RENAME:
				{
					string src_name = data;
					data += src_name.length() + 1;

					const char *dst_name = data;

					sftp_shared_ptr auto_sftp(sftp_prefix);
					LIBSSH2_SFTP *sftp = auto_sftp.get();

					retval = libssh2_sftp_rename(reinterpret_cast<LIBSSH2_SFTP *>(sftp), src_name.c_str(), dst_name);
					if (retval == LIBSSH2_ERROR_SFTP_PROTOCOL) {
						libssh2_sftp_unlink(reinterpret_cast<LIBSSH2_SFTP *>(sftp), dst_name);
						retval = libssh2_sftp_rename(reinterpret_cast<LIBSSH2_SFTP *>(sftp), src_name.c_str(), dst_name);
					}

					if (retval == LIBSSH2_ERROR_SFTP_PROTOCOL) {
						goto ERROR_LABEL;
					} else if (retval) {
						if (++retry == RETRIES) {
							GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't rename file {1} to {2} on node {3} - {4}") % src_name % dst_name % sftp_prefix % auto_sftp.get_error()).str(SGLOCALE));
							goto ERROR_LABEL;
						}

						goto SSH_ERROR;
					}

					datalen = sizeof(file_rply_t) - sizeof(long);
					result_msg->set_length(datalen);

					rply = reinterpret_cast<file_rply_t *>(result_msg->data());
					rply->datalen = datalen;
					rply->error_code = 0;
					rply->native_code = 0;
				}
				break;
			case FILE_OPCODE_GET:
				{
					string src_name = data;
					data += src_name.length() + 1;

					const char *dst_name = data;

					isftpstream isftp(sftp_prefix, src_name);
					if (!isftp) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1} on node {2}") % src_name % sftp_prefix).str(SGLOCALE));
						goto SSH_ERROR;
					}

					ofstream ofs(dst_name);
					if (!ofs) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1}") % dst_name).str(SGLOCALE));
						goto ERROR_LABEL;
					}

					ofs << isftp.rdbuf();

					datalen = sizeof(file_rply_t) - sizeof(long);
					result_msg->set_length(datalen);

					rply = reinterpret_cast<file_rply_t *>(result_msg->data());
					rply->datalen = datalen;
					rply->error_code = 0;
					rply->native_code = 0;
				}
				break;
			case FILE_OPCODE_PUT:
				{
					string src_name = data;
					data += src_name.length() + 1;

					const char *dst_name = data;

					ifstream ifs(src_name.c_str());
					if (!ifs) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1}") % src_name).str(SGLOCALE));
						goto ERROR_LABEL;
					}

					osftpstream osftp(sftp_prefix, dst_name);
					if (!osftp) {
						GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1} on node {2}") % dst_name % sftp_prefix).str(SGLOCALE));
						goto SSH_ERROR;
					}

					osftp << ifs.rdbuf();

					datalen = sizeof(file_rply_t) - sizeof(long);
					result_msg->set_length(datalen);

					rply = reinterpret_cast<file_rply_t *>(result_msg->data());
					rply->datalen = datalen;
					rply->error_code = 0;
					rply->native_code = 0;
				}
				break;
			default:
				GPP->write_log(__FILE__, __LINE__, (_("FATAL: internal error")).str(SGLOCALE));
				exit(1);
			}

			return svc_fini(SGSUCCESS, 0, result_msg);
		} catch (bad_ssh& ex) {
			if (++retry == RETRIES) {
				GPP->write_log(__FILE__, __LINE__, ex.what());
				goto ERROR_LABEL;
			}
		} catch (exception& ex) {
			GPP->write_log(__FILE__, __LINE__, ex.what());
			goto ERROR_LABEL;
		}

SSH_ERROR:
		SSHP->erase_session(sftp_prefix);
		data = rqst->data() + sftp_prefix.length() + 1;
	}

ERROR_LABEL:
	datalen = sizeof(file_rply_t) - sizeof(long);
	result_msg->set_length(datalen);

	rply = reinterpret_cast<file_rply_t *>(result_msg->data());
	rply->datalen = datalen;
	rply->error_code = SGESYSTEM;
	rply->native_code = retval;
	result_msg->set_length(rply->datalen);
	return svc_fini(SGSUCCESS, 0, result_msg);
}

}
}

