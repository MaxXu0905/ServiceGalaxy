#include "listener.h"

namespace bf = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::posix_time;
namespace bc = boost::chrono;

namespace ai
{
namespace sg
{

bool listener::started = false;

listener::listener()
	: acceptor(io_svc)
{
	GPP = gpp_ctx::instance();
}

listener::~listener()
{
}

int listener::run(int argc, char **argv)
{
	string nlsaddr;
	string user;
	bool forground = false;
	struct passwd *pwd = NULL;
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, (_("argc={1}") % argc).str(SGLOCALE), &retval);
#endif

	sg_api& api_mgr = sg_api::instance();
	api_mgr.set_procname(argv[0]);

	umask(0);		// make sure log writable by all
	setpgid(0, 0);  // make immune to signals sent to caller

	// Make process impervious to spurious signal activity.
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);
	sigaction(SIGUSR2, &act, NULL);

	act.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &act, NULL);

#if defined(SIGPOLL)
	sigaction(SIGPOLL, &act, NULL);
#endif

	act.sa_handler = sigterm;
	act.sa_flags = 0;
	sigaction(SIGTERM, &act, NULL);

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("lnsaddr,l", po::value<string>(&nlsaddr)->required(), (_("network address")).str(SGLOCALE).c_str())
		("user,u", po::value<string>(&user), (_("name or number of user to run as")).str(SGLOCALE).c_str())
		("forground,F", (_("Run in foreground rather than background")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			retval = 0;
			return retval;
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		return retval;
	}

	if (vm.count("forground"))
		forground = true;

	// Change to the appropriate user id for main loop.
	if (!user.empty()) {
		if (isdigit(user[0]))
			pwd = getpwuid(atoi(user.c_str()));
		else
			pwd = getpwnam(user.c_str());

		if (pwd == NULL) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: sgagent could not find entry for user {1}\n") % user).str(SGLOCALE));
			sigterm(SIGTERM);
			return retval;
		}
	}

	// If we're root, then pwd is required (and must be non-root)
	if (geteuid() == 0) {
		if ((pwd == NULL) || pwd->pw_uid == 0) {
			cerr << (_("ERROR: sgagent cannot run as root\n")).str(SGLOCALE);
			sigterm(SIGTERM);
			return retval;
		}
		if (setgid(pwd->pw_gid) == -1) {
			cerr << (_("ERROR: sgagent could not set group id to ")).str(SGLOCALE) << pwd->pw_name << "\n";
			sigterm(SIGTERM);
			return retval;
		}
		if (setuid(pwd->pw_uid) == -1) {
			cerr << (_("ERROR: sgagent could not set user id to ")).str(SGLOCALE) << pwd->pw_name << "\n";
			sigterm(SIGTERM);
			return retval;
		}
	} else {
		// If we're not root, can't change uid or must be same
		if (pwd != NULL && geteuid() != pwd->pw_uid) {
			cerr << (_("ERROR: sgagent invoked by non-root, can't change user id.\n")).str(SGLOCALE);
			sigterm(SIGTERM);
			return retval;
		}
	}

	string ipaddr;
	string port;
	if (!get_address(nlsaddr, ipaddr, port)) {
		sigterm(SIGTERM);
		return retval;
	}

	// 创建连接
	basio::ip::tcp::resolver resolver(io_svc);
	basio::ip::tcp::resolver::query query(ipaddr, boost::lexical_cast<string>(port));
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
	acceptor.open(endpoint.protocol());
	acceptor.set_option(basio::socket_base::reuse_address(true));
	acceptor.bind(endpoint);
	acceptor.listen();

	start_accept();

	if (!forground && sys_func::instance().background() != 0) {
		cerr << (_("ERROR: Could not become a background process")).str(SGLOCALE);
		sigterm(SIGTERM);
		return retval;
	}

	started = true;

	GPP->write_log((_("INFO: sgagent started successfully")).str(SGLOCALE));

	io_svc.run();
	sigterm(0);
	retval = 0;
	return retval;
}

void listener::start_accept()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, "", NULL);
#endif
	listener_connection::pointer new_connection = listener_connection::create(acceptor.get_io_service());

	acceptor.async_accept(new_connection->socket(),
		boost::bind(boost::mem_fn(&listener::handle_accept), this, new_connection,
		basio::placeholders::error));
}

void listener::handle_accept(listener_connection::pointer new_connection, const boost::system::error_code& error)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(10, __PRETTY_FUNCTION__, (_("error={1}") % error).str(SGLOCALE), NULL);
#endif

	if (!error)
		new_connection->start();

	start_accept();
}

bool listener::get_address(const string& nlsaddr, string& ipaddr, string& port)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(1000, __PRETTY_FUNCTION__, (_("nlsaddr={1}") % nlsaddr).str(SGLOCALE), &retval);
#endif

	if (nlsaddr.size() < 5) {
		cerr << (_("ERROR: Invalid agtaddr parameter given, at least 5 characters long\n")).str(SGLOCALE);
		return retval;
	}

	if (memcmp(nlsaddr.c_str(), "//", 2) != 0) {
		cerr << (_("ERROR: Invalid agtaddr parameter given, should start with \"//\"\n")).str(SGLOCALE);
		return retval;
	}

	string::size_type pos = nlsaddr.find(":");
	if (pos == string::npos) {
		cerr << (_("ERROR: Invalid agtaddr parameter given, missing ':'\n")).str(SGLOCALE);
		return retval;
	}

	string hostname = nlsaddr.substr(2, pos - 2);
	port = nlsaddr.substr(pos + 1, nlsaddr.length() - pos - 1);

	if (!isdigit(hostname[0])) {
		hostent *ent = gethostbyname(hostname.c_str());
		if (ent == NULL) {
			cerr << (_("ERROR: Invalid hostname of agtaddr parameter given\n")).str(SGLOCALE);
			sigterm(SIGTERM);
			return 1;
		}
		ipaddr = boost::lexical_cast<string>(ent->h_addr_list[0][0] & 0xff);
		for (int i = 1; i < ent->h_length; i++) {
			ipaddr += '.';
			ipaddr += boost::lexical_cast<string>(ent->h_addr_list[0][i] & 0xff);
		}
	} else {
		ipaddr = hostname;
	}

	retval = true;
	return retval;
}

void listener::sigterm(int signo)
{
	gpp_ctx *GPP = gpp_ctx::instance();

	if (started)
		GPP->write_log((_("INFO: Terminating sgagent process\n")).str(SGLOCALE));
	else
		GPP->write_log((_("INFO: Terminating sgagent process\n")).str(SGLOCALE));

	if (signo != 0)
		exit(1);
	else
		exit(0);
}

}
}

