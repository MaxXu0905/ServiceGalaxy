#include "clt_test.h"
#include "FileUtils.h"
#include "sg_public.h"
#include "sys.h"
using namespace ai::sg;
namespace ai {
namespace test{
namespace sg {
namespace {
using namespace std;
class callWraper {
public:
	callWraper(string service, string in, int flags) {
		this->service = service;
		this->in = in;
		this->flags = flags;
	}
	void run() {
		message_pointer send;
		message_pointer recv;
		send = sg_message::create();
		recv = sg_message::create();
		sg_api &api_mgr = sg_api::instance();
		sg_init_t init_info;
		init_info.flags |= SGMULTICONTEXTS;
		if (!api_mgr.init(&init_info)) {
			result = -100;
			return;
		}
		(*send) = in;
		if (!api_mgr.call(service.c_str(), send, recv, flags)) {
			urcode = api_mgr.get_ur_code();
			result = api_mgr.get_error_code();
		}else{
			urcode = api_mgr.get_ur_code();
			result=0;
			out.assign(recv->data(), recv->data() + recv->length());
		}
		api_mgr.fini();
	}
	string getOut() {
		return out;
	}
	int getResult() {
		return result;
	}
	int getUrcode() {
		return urcode;
	}
private:
	string service;
	string in;
	string out;
	int flags;
	int result;
	int urcode;
};
}

class Impl:public SGClient4Test {
public:
	Impl(char * procname) {
		api_mgr = &sg_api::instance();
		api_mgr->set_procname(procname);
		send = sg_message::create();
		recv = sg_message::create();
	}
	~Impl() {
	}
	 bool init() {
		return api_mgr->init(0);
	}
	 void fini() {
		api_mgr->fini();
	}
	int startServer(int32_t groupId, int32_t serverId) {
		if (groupId == -1 && serverId == -1) {
			return Sys::noutsys("sgboot -S");
		}
		string s =
				(boost::format("sgboot -g%1%  -i%2%") % groupId % serverId).str();
		return Sys::noutsys(s.c_str());
	}
	int stopServer(int32_t groupId, int32_t serverId) {
		if (groupId == -1 && serverId == -1) {
			return Sys::noutsys("sgshut -S");
		}
		string s =
				(boost::format("sgshut -g%1%  -i%2%") % groupId % serverId).str();
		return Sys::noutsys(s.c_str());
	}

	 int sgpack(const string filename,bool istemp,const string configfile) {
		if(istemp){
			map<string,string> empty;
			return sgpack(filename,empty,configfile);
		}
		string s = (boost::format("sgpack -y  %1%") % filename).str();
		return Sys::noutsys(s.c_str());

	}
	 int sgpack(const string filename,map<string,string> &context,const string configfile){
		char temp[]="/tmp/sgtemplate-XXXXXX";
		mktemp(temp);
		if(!FileUtils::genfile(filename,temp,context,configfile)){
			FileUtils::del(temp);
			return -1;
		}
		int re=sgpack(temp,false,"");
		FileUtils::del(temp);
		return re;

	}
	 int startsg() {
		int result = Sys::noutsys("sgboot -y");
		init();
		return result;
	}
	 int stopsg() {
		api_mgr->fini();
		return Sys::noutsys("sgshut -y");
	}
	 int startadmin() {
		return Sys::noutsys("sgboot -A");
	}
	 int stopadmin() {
		return Sys::noutsys("sgshut -A");
	}
	int acall(const char *service, const string in, int flags) {
		(*send) = in;
		return api_mgr->acall(service, send, flags);
	}
	int acallnoreply(const char *service, const string in) {
		return acall(service, in, SGNOREPLY);
	}
	int cancel(int handle) {
		if (!api_mgr->cancel(handle)) {
//			std::cout << "error_code = " << api_mgr->get_error_code()
//					<< std::endl;
			return api_mgr->get_error_code();
		}
		return 0;
	}
	int getrply(int handle, string& out, int flags) {
		if (!api_mgr->getrply(handle, recv, flags)) {
//			std::cout << "error_code = " << api_mgr->get_error_code()
//					<< std::endl;
			return api_mgr->get_error_code();
		}
		out.assign(recv->data(), recv->data() + recv->length());
		return 0;
	}

	int acallAndRplay(const char *service, const string in, string& out,
			int flags) {
		(*send) = in;
		int handle = api_mgr->acall(service, send, 0);
		if (!api_mgr->getrply(handle, recv, 0)) {
//			std::cout << "error_code = " << api_mgr->get_error_code()
//					<< std::endl;
			return api_mgr->get_error_code();
		}
		out.assign(recv->data(), recv->data() + recv->length());
		return 0;
	}
	int call(const char *service, const string in, string& out, int flags) {
		int urcode;
		return callurcode(service, in, out, urcode, flags);
	}

	vector<int> callnthread(const char *service, const vector<string> &ins,
			vector<string>& outs, vector<int> &urcodes, int flags) {

		boost::thread_group _thread_group;
		vector<callWraper*> wrapers;
		for (int i = 0; i < ins.size(); i++) {
			callWraper *wraper = new callWraper(service, ins[i], flags);
			_thread_group.create_thread(boost::bind(&callWraper::run, wraper));
			wrapers.push_back(wraper);
		}
		_thread_group.join_all();
		vector<int> results;
		for (int i = 0; i < wrapers.size(); i++) {
			results.push_back(wrapers[i]->getResult());
			outs.push_back(wrapers[i]->getOut());
			urcodes.push_back(wrapers[i]->getUrcode());
			delete wrapers[i];
		}
		return results;
	}
	int callurcode(const char *service, const string in, string& out,
			int &urcode, int flags = 0) {
		(*send) = in;

		if (!api_mgr->call(service, send, recv, 0)) {
			urcode = api_mgr->get_ur_code();
			return api_mgr->get_error_code();
		}
		urcode = api_mgr->get_ur_code();
		out.assign(recv->data(), recv->data() + recv->length());
		return 0;
	}
	bool set_blktime(int blktime, int flags) {
		return api_mgr->set_blktime(blktime, flags);
	}
	int get_blktime(int flags) {
		return api_mgr->get_blktime(flags);
	}
	int getprio() {
		return api_mgr->getprio();
	}
	bool setprio(int prio, int flags) {
		return api_mgr->setprio(prio, flags);
	}
private:
	message_pointer send;
	message_pointer recv;
	sg_api * api_mgr;
};

SGClient4Test::SGClient4Test() {
}
SGClient4Test &SGClient4Test::instance() {
	static Impl impl("SGClient4Test");
	return impl;
}
SGClient4Test::~SGClient4Test() {
}
}
}
}

