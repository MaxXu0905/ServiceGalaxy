#include "sql_extern.h"
#include "sql_statement.h"

using namespace ai::scci;
using namespace std;

static void usage(const string& cmd);

int main(int argc, char *argv[])
{
	int ret;
	int c;
	string h_path;
	string cpp_path;
	string file_prefix;
	string class_name;
	int array_size = 0;
	bool require_ind = false;
	dbtype_enum dbtype = DBTYPE_ORACLE; // default
	bool print_only = false;
	bool verbose = false;

	yyin = stdin;		// default
	yyout = stdout;	// default
	errcnt = 0;

	while ((c = ::getopt(argc, argv,"h:H:f:c:s:id:pv")) != EOF) {
		switch (c) {
 		case 'h':
			h_path = optarg;
			break;
		case 'H':
			cpp_path = optarg;
			break;
		case 'f':
			file_prefix = optarg;
			break;
		case 'c':
			class_name = optarg;
			break;
		case 's':
			array_size = atoi(optarg);
			break;
		case 'i':
			require_ind = true;
			break;
		case 'd':
			if (strcasecmp(optarg, "ORACLE") == 0)
				dbtype = DBTYPE_ORACLE;
			else if (strcasecmp(optarg, "ALTIBASE") == 0)
				dbtype = DBTYPE_ALTIBASE;
			else if (strcasecmp(optarg, "DBC") == 0)
				dbtype = DBTYPE_DBC;
			else if (strcasecmp(optarg, "GP") == 0)
							dbtype = DBTYPE_GP;
			else
				usage(argv[0]);
			break;
		case 'p':
			print_only = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (optind < argc) {
		cerr << (_("ERROR: Invalid arguments passed to buildsql\n")).str(SGLOCALE);
		::exit(1);
	}

	if ((h_path.empty() && cpp_path.empty()) || file_prefix.empty() || class_name.empty())
		usage(argv[0]);

	if (h_path.empty())
		h_path = cpp_path;
	else if (cpp_path.empty())
		cpp_path = h_path;

	if (array_size <= 0)
		array_size = 1000;

	// call yyparse() to parse input or temp file
	if ((ret = yyparse()) < 0) {
		cerr << "\n" << (_("ERROR: {1}: Parse failed\n") % argv[0]).str(SGLOCALE);
		::exit(1);
	}
	if(ret == 1) {
		cerr << "\n" << (_("ERROR: {1}: Severe error found. Stop syntax checking\n") % argv[0]).str(SGLOCALE);
		::exit(1);
	}
	if (errcnt > 0) {
		cerr << "\n" << (_("ERROR: {1}: Above errors found during syntax checking\n") % argv[0]).str(SGLOCALE);
		::exit(1);
	}

	print_prefix = "";

	sql_statement::set_h_path(h_path);
	sql_statement::set_cpp_path(cpp_path);
	sql_statement::set_file_prefix(file_prefix);
	sql_statement::set_class_name(class_name);
	sql_statement::set_array_size(array_size);
	sql_statement::set_reqire_ind(require_ind);
	sql_statement::set_dbtype(dbtype);
	if (print_only) {
		sql_statement::print_data();
	} else {
		if (!sql_statement::gen_data_static()) {
			cerr << "\n" << (_("ERROR: {1}: Parse failed, see LOG for more information\n") % argv[0]).str(SGLOCALE);
			sql_statement::clear();
			::exit(1);
		}
	}

	sql_statement::clear();

	cout << (_("Parse finished successfully")).str(SGLOCALE) << "\n\n";
	::exit(0);
}

static void usage(const string& cmd)
{
	cout << (_("usage: ")).str(SGLOCALE) << cmd
		<< " [-v] [-p] [-i] [-h h_path] [-H cpp_path] [-f file_prefix] [-c class_name] [-s array_size] [-d ORACLE|ALTIBASE|SHM|COHERERENCE|GP]\n";
	::exit(1);
}

