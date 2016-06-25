#include "sg_build.h"

namespace po = boost::program_options;
namespace bio = boost::io;

namespace ai
{
namespace sg
{

int const BUILD_FLAG_CLIENT = 0x1;
int const BUILD_FLAG_SERVER = 0x2;

sg_build::sg_build()
{
}

sg_build::~sg_build()
{
	if (!keep_temp)
		::unlink(srcf.c_str());

	::unlink(objf.c_str());
}

int sg_build::run(int argc, char **argv)
{
	int retval = -1;
#if defined(DEBUG)
	scoped_debug<int> debug(100, __PRETTY_FUNCTION__, (_("argc={1}, argv={2}") % argc % argv).str(SGLOCALE), &retval);
#endif

	build_flag = 0;
	keep_temp = false;
	system_used = false;
	verbose = false;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--client") == 0) {
			build_flag |= BUILD_FLAG_CLIENT;
			break;
		} else if (strcmp(argv[i], "--server") == 0) {
			build_flag |= BUILD_FLAG_SERVER;
			break;
		}
	}

	if (build_flag & BUILD_FLAG_CLIENT) {
		if (!init_clt(argc, argv))
			return retval;
		if (!build_clt())
			return retval;
	} else {
		if (!init_svr(argc, argv))
			return retval;
		if (!gen_svr())
			return retval;
		if (!build_svr())
			return retval;
	}

	retval = 0;
	return retval;
}

bool sg_build::init_svr(int argc, char **argv)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("argc={1}, argv={2}") % argc % argv).str(SGLOCALE), &retval);
#endif

	vector<string> services;

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("first,f", po::value<vector<string> >(&first_files), (_("files added before libraries")).str(SGLOCALE).c_str())
		("last,l", po::value<vector<string> >(&last_files), (_("files added after libraries")).str(SGLOCALE).c_str())
		("include,I", po::value<vector<string> >(&include_files), (_("header files which operation class defined")).str(SGLOCALE).c_str())
		("output,o", po::value<string>(&output_file)->required(), (_("output filename")).str(SGLOCALE).c_str())
		("namespace,n", po::value<vector<string> >(&namespaces), (_("namespaces where operation class resides")).str(SGLOCALE).c_str())
		("server-class,S", po::value<string>(&server_class)->default_value("sg_svr"), (_("server class name")).str(SGLOCALE).c_str())
		("operation,s", po::value<vector<string> >(&services), (_("operation name and its implementation, OPERATION:CLASS")).str(SGLOCALE).c_str())
		("keep,k", (_("keep temp files")).str(SGLOCALE).c_str())
		("system", (_("system used only")).str(SGLOCALE).c_str())
		("server", (_("specify to build server")).str(SGLOCALE).c_str())
		("verbose,v", (_("verbose mode")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		exit(1);
	}

	if (vm.count("keep"))
		keep_temp = true;
	if (vm.count("system"))
		system_used = true;
	if (vm.count("verbose"))
		verbose = true;

	bool empty_include = include_files.empty();
	for (vector<string>::const_iterator iter = services.begin(); iter != services.end(); ++iter) {
		bool found = false;
		for (string::const_iterator str_iter = iter->begin(); str_iter != iter->end(); ++str_iter) {
			if (*str_iter == ':') {
				string service_name(iter->begin(), str_iter);
				string class_name(str_iter + 1, iter->end());
				map<string, string>::const_iterator map_iter = service_map.find(service_name);
				if (map_iter != service_map.end()) {
					std::cout << (_("ERROR: duplicate operation name {1} given")% service_name).str(SGLOCALE) << std::endl;
					return retval;
				}
				service_map[service_name] = class_name;
				if (empty_include)
					include_files.push_back(class_name + ".h");
				found = true;
				break;
			}
		}

		if (!found) {
			std::cout << (_("ERROR: wrong operation parameter given.")).str(SGLOCALE) << std::endl;
			return retval;
		}
	}

	retval = true;
	return retval;
}

bool sg_build::init_clt(int argc, char **argv)
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, (_("argc={1}, argv={2}") % argc % argv).str(SGLOCALE), &retval);
#endif

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("first,f", po::value<vector<string> >(&first_files), (_("files added before libraries")).str(SGLOCALE).c_str())
		("last,l", po::value<vector<string> >(&last_files), (_("files added after libraries")).str(SGLOCALE).c_str())
		("output,o", po::value<string>(&output_file)->required(), (_("output filename")).str(SGLOCALE).c_str())
		("client", (_("specify to build client")).str(SGLOCALE).c_str())
		("verbose,v", (_("verbose mode")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		exit(1);
	}

	if (vm.count("verbose"))
		verbose = true;

	retval = true;
	return retval;
}

bool sg_build::gen_svr()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	int index;
	map<string, string>::const_iterator iter;

	srcf = (boost::format("BS-%+x.cpp") % getpid()).str();
	objf = (boost::format("BS-%+x.o") % getpid()).str();

	ofstream ofs(srcf.c_str());
	if (!ofs) {
		std::cout << (_("ERROR: sgcompile cannot open the temporary output file ")).str(SGLOCALE) << srcf << std::endl;
		return retval;
	}

	ofs << "#include \"" << server_class << ".h\"\n"
		<< "#include \"sgp_ctx.h\"\n";

	if (!system_used)
		ofs << "#include <boost/thread.hpp>\n";

	for (vector<string>::const_iterator iter = include_files.begin(); iter != include_files.end(); ++iter)
		ofs << "#include \"" << *iter << "\"\n";

	ofs << "\nusing namespace ai::sg;\n";
	for (vector<string>::const_iterator iter = namespaces.begin(); iter != namespaces.end(); ++iter)
		ofs << "using namespace " << *iter << ";\n";

	// 创建SG_SVR
	ofs << "\nstatic sg_svr * create_svr()\n"
		<< "{\n"
		<< "\treturn new " << server_class << "();\n"
		<< "}\n\n";

	// 创建SG_SVC
	index = 0;
	for (iter = service_map.begin(); iter != service_map.end(); ++iter) {
		ofs << "static sg_svc * create_svc" << index << "()\n"
			<< "{\n"
			<< "\treturn new " << iter->second << "();\n"
			<< "}\n\n";
		index++;
	}

	ofs << "static sg_svcdsp_t _svcdsp_table[] = {\n";
	index = 0;
	for (iter = service_map.begin(); iter != service_map.end(); ++iter) {
		ofs << "\t{ " << index << ", \"" << iter->first << "\", \""
			<< iter->second << "\", &create_svc" << index << " },\n";
		index++;
	}
	ofs << "\t{ -1, \"\", \"\", NULL }\n"
		<< "};\n\n";

	ofs << "int main(int argc, char **argv)\n"
		<< "{\n"
		<< "\tgpp_ctx *GPP = gpp_ctx::instance();\n"
		<< "\tsgp_ctx *SGP = sgp_ctx::instance();\n";
	if (system_used)
		ofs << "\tSGP->_SGP_let_svc = true;\n";
	ofs << "\tSGP->_SGP_svcdsp = _svcdsp_table;\n\n";

	ofs << "\ttry {\n"
		<< "\t\t" << server_class << " svr_mgr;\n";

	ofs << "\n\t\treturn svr_mgr._main(argc, argv, &create_svr);\n"
		<< "\t} catch (exception& ex) {\n"
		<< "\t\tGPP->write_log(__FILE__, __LINE__, ex.what());\n"
		<< "\t\treturn 1;\n"
		<< "\t}\n"
		<< "}\n";

	retval = true;
	return retval;
}

bool sg_build::build_svr()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	char *ptr;
	string cc;
	string cflags;
	ostringstream fmt;

	ptr = ::getenv("CC");
	if (ptr == NULL || ptr[0] == '\0')
		cc = _SGCOMPILER;
	else
		cc = ptr;

	ptr = ::getenv("CFLAGS");
	if (ptr == NULL) {
		cflags = "";
	} else {
		cflags = ptr;
		cflags += ' ';
	}

	fmt.str("");
	fmt << cc << " ";

	/* Special compile options that:
	 * 1) we are built with, and
	 * 2) that users must use when they use us to build their apps.
	 */
#if defined(sun)
#ifdef __sparcv9
	fmt << "-xarch=v9 -w ";
#elif defined(__amd64)
	fmt << "-xarch=amd64 -fnonstd  -w ";
#else
	fmt << "-w ";
#endif /* __sparcv9 */
#elif defined(_IBMR2)
#if defined(__64BIT__)
	fmt << "-q64 ";
#else
	fmt << "-brtl -qstaticinline -qrtti=all -bhalt:5 ";
#endif /* __64BIT__ */
#elif defined(__hpux)
#if defined(__LP64__) && defined(__BIGMSGQUEUE_ENABLED)
#ifdef __ia64
	fmt << "+DD64 +Olit=all -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#else
	fmt << "+DA2.0W -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#endif /* __ia64 */
#elif defined(__LP64__)
#ifdef __ia64
	fmt << "+DD64 +Olit=all -Wl,+s ";
#else
	fmt << "+DA2.0W -Wl,+s ";
#endif /* __ia64 */
#elif defined(__BIGMSGQUEUE_ENABLED)
#ifdef __ia64
	fmt << "+DD32 +Olit=all -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#else
	fmt << "+DA1.1 -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#endif /* __ia64 */
#else
#ifdef __ia64
	fmt << "+DD32 +Olit=all -Wl,+s ";
#else
	fmt << "+DA1.1 -Wl,+s ";
#endif /* __ia64 */
#endif /* __LP64__, __BIGMSGQUEUE_ENABLED */
#if defined(_HP_NAMESPACE_STD)
	fmt << "-AA ";
#else
	fmt << "-AP -D__HPACC_USING_MULTIPLIES_IN_FUNCTIONAL ";
#endif
#elif defined(__linux__)
#if defined(__x86_64__)
	fmt << "-m64 ";
#else
	fmt << "-m32 ";
#endif
#endif

#if defined(sun) || (defined(__hpux) && defined(__ia64))
	// -R/usr/lib/lwp is already included
	fmt << "-mt ";
#endif

	for (vector<string>::const_iterator iter = first_files.begin(); iter != first_files.end(); ++iter)
		fmt << *iter << " ";

	fmt << cflags << "-I$SGROOT/include -o " << output_file << " " << srcf;

#ifdef DYNAMIC
	fmt << " -Bdynamic";
#endif

	fmt << " -L$SGROOT/lib";

#if defined(_AIX)
	fmt << " -brtl -qstaticinline";
#endif

	fmt << " -lpublic -lsg";
	for (vector<string>::const_iterator iter = last_files.begin(); iter != last_files.end(); ++iter)
		fmt << " " << *iter;

	string cmd = fmt.str();

	if (verbose)
		std::cout << cmd << std::endl;

	sys_func& func_mgr = sys_func::instance();
	int ret = func_mgr.gp_status(::system(cmd.c_str()));
	if (ret != 0) {
		gpp_ctx *GPP = gpp_ctx::instance();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't execute {1} - {2}") % cmd % strerror(errno)).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

bool sg_build::build_clt()
{
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(100, __PRETTY_FUNCTION__, "", &retval);
#endif

	char *ptr;
	string cc;
	string cflags;
	ostringstream fmt;

	ptr = ::getenv("CC");
	if (ptr == NULL || ptr[0] == '\0')
		cc = _SGCOMPILER;
	else
		cc = ptr;

	ptr = ::getenv("CFLAGS");
	if (ptr == NULL) {
		cflags = "";
	} else {
		cflags = ptr;
		cflags += ' ';
	}

	fmt.str("");
	fmt << cc << " ";

	/* Special compile options that:
	 * 1) we are built with, and
	 * 2) that users must use when they use us to build their apps.
	 */
#if defined(sun)
#ifdef __sparcv9
	fmt << "-xarch=v9 -w ";
#elif defined(__amd64)
	fmt << "-xarch=amd64 -fnonstd  -w ";
#else
	fmt << "-w ";
#endif /* __sparcv9 */
#elif defined(_IBMR2)
#if defined(__64BIT__)
	fmt << "-q64 ";
#else
	fmt << "-brtl -qstaticinline -qrtti=all -bhalt:5 ";
#endif /* __64BIT__ */
#elif defined(__hpux)
#if defined(__LP64__) && defined(__BIGMSGQUEUE_ENABLED)
#ifdef __ia64
	fmt << "+DD64 +Olit=all -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#else
	fmt << "+DA2.0W -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#endif /* __ia64 */
#elif defined(__LP64__)
#ifdef __ia64
	fmt << "+DD64 +Olit=all -Wl,+s ";
#else
	fmt << "+DA2.0W -Wl,+s ";
#endif /* __ia64 */
#elif defined(__BIGMSGQUEUE_ENABLED)
#ifdef __ia64
	fmt << "+DD32 +Olit=all -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#else
	fmt << "+DA1.1 -Wl,+s -D__BIGMSGQUEUE_ENABLED ";
#endif /* __ia64 */
#else
#ifdef __ia64
	fmt << "+DD32 +Olit=all -Wl,+s ";
#else
	fmt << "+DA1.1 -Wl,+s ";
#endif /* __ia64 */
#endif /* __LP64__, __BIGMSGQUEUE_ENABLED */
#if defined(_HP_NAMESPACE_STD)
	fmt << "-AA ";
#else
	fmt << "-AP -D__HPACC_USING_MULTIPLIES_IN_FUNCTIONAL ";
#endif
#elif defined(__linux__)
#if defined(__x86_64__)
	fmt << "-m64 ";
#else
	fmt << "-m32 ";
#endif
#endif

#if defined(sun) || (defined(__hpux) && defined(__ia64))
	// -R/usr/lib/lwp is already included
	fmt << "-mt ";
#endif

	for (vector<string>::const_iterator iter = first_files.begin(); iter != first_files.end(); ++iter)
		fmt << *iter << " ";

	fmt << cflags << "-I$SGROOT/include -o " << output_file << " " << srcf;

#ifdef DYNAMIC
	fmt << " -Bdynamic";
#endif

	fmt << " -L$SGROOT/lib";

#if defined(_AIX)
	fmt << " -brtl -qstaticinline";
#endif

	fmt << " -lpublic -lsg";
	for (vector<string>::const_iterator iter = last_files.begin(); iter != last_files.end(); ++iter)
		fmt << " " << *iter;

	string cmd = fmt.str();

	if (verbose)
		std::cout << cmd << std::endl;

	sys_func& func_mgr = sys_func::instance();
	int ret = func_mgr.gp_status(::system(cmd.c_str()));
	if (ret != 0) {
		gpp_ctx *GPP = gpp_ctx::instance();
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't execute {1} - {2}") % cmd % strerror(errno)).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

}
}

