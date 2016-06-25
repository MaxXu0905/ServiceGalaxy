#include "alive_svc.h"
#include "alive_struct.h"

using namespace std;

namespace ai
{
namespace sg
{

int const RETRIES = 3;
typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

alive_svc::alive_svc()
{
	SSHP = sshp_ctx::instance();
	result_msg = sg_message::create(SGT);
}

alive_svc::~alive_svc()
{
}

svc_fini_t alive_svc::svc(message_pointer& svcinfo)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, "", NULL);
#endif

	alive_rqst_t *rqst = reinterpret_cast<alive_rqst_t *>(svcinfo->data());
	result_msg->set_length(sizeof(alive_rply_t));
	alive_rply_t *rply = reinterpret_cast<alive_rply_t *>(result_msg->data());
	set<pid_t> *pids = NULL;

	alive_proc_t key;
	key.usrname = rqst->usrname;
	key.hostname = rqst->hostname;

	set<alive_proc_t>::iterator iter = procs.find(key);
	if (iter == procs.end()) {
		iter = procs.insert(key).first;
		alive_proc_t& proc = const_cast<alive_proc_t&>(*iter);
		proc.expire = time(0);

		if (!get_procs(rqst->usrname, rqst->hostname, proc.pids)) {
			proc.expire = -1;
			goto ERROR_LABEL;
		}

		pids = &proc.pids;
	} else if (iter->expire < rqst->expire) {
		alive_proc_t& proc = const_cast<alive_proc_t&>(*iter);
		proc.expire = time(0);
		proc.pids.clear();

		if (!get_procs(rqst->usrname, rqst->hostname, proc.pids)) {
			proc.expire = -1;
			goto ERROR_LABEL;
		}

		pids = &proc.pids;
	} else {
		alive_proc_t& proc = const_cast<alive_proc_t&>(*iter);

		pids = &proc.pids;
	}

	{
		set<pid_t>::const_iterator pid_iter = pids->find(rqst->pid);
		if (pid_iter != pids->end())
			rply->rtn = true;
		else
			rply->rtn = false;
	}

	return svc_fini(SGSUCCESS, 0, result_msg);

ERROR_LABEL:
	// 假定还活着
	rply->rtn = true;
	return svc_fini(SGSUCCESS, 0, result_msg);
}

bool alive_svc::get_procs(const string& usrname, const string& hostname, set<pid_t>& pids)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("usrname={1}, hostname={2}") % usrname % hostname).str(SGLOCALE), &retval);
#endif

	string ssh_prefix = usrname;
	ssh_prefix += "@";
	ssh_prefix += hostname;

	string real_command = "ps -ef|awk -v LOGNAME=";
	real_command += usrname;
	real_command += " '{if($1==LOGNAME) print $2}'";

	int retry = 0;
	while (1) {
		try {
			channel_shared_ptr auto_channel(ssh_prefix);
			LIBSSH2_CHANNEL *channel = auto_channel.get();

			if (libssh2_channel_exec(channel, real_command.c_str()) != 0) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't execute command {1} on {2}") % real_command % ssh_prefix).str(SGLOCALE));
				return retval;
			}

			string result;
			char buf[4096];
			ssize_t size;
			while ((size = libssh2_channel_read(channel, buf, sizeof(buf))) > 0)
				result += string(buf, buf + size);

			int ret_code = auto_channel.get_exit_status();
			if (ret_code) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Failed to execute command {1} with return code {2}") % real_command % ret_code).str(SGLOCALE));
				return retval;
			}

			boost::char_separator<char> sep("\r\n\t\b ");
			tokenizer tokens(result, sep);

			for (tokenizer::iterator iter = tokens.begin(); iter != tokens.end(); ++iter) {
				string pid_str = *iter;
				if (pid_str.empty())
					continue;

				pids.insert(static_cast<pid_t>(atol(pid_str.c_str())));
			}

			retval = true;
			return retval;
		} catch (bad_ssh& ex) {
			if (++retry == RETRIES) {
				GPP->write_log(__FILE__, __LINE__, ex.what());
				return retval;
			}
		} catch (exception& ex) {
			GPP->write_log(__FILE__, __LINE__, ex.what());
			return retval;
		}
	}
}

}
}

