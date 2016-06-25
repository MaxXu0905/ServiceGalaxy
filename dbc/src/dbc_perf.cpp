#include "datastruct_structs.h"
#include "dbc_api.h"
#include "sdc_api.h"
#include <boost/interprocess/sync/named_mutex.hpp>
#include "perf_macro.h"

using namespace ai::sg;
using namespace std;
namespace bf = boost::filesystem;
namespace bi = boost::interprocess;
namespace po = boost::program_options;
namespace bc = boost::chrono;

int const MODE_SELECT = 0x1;
int const MODE_INSERT = 0x2;
int const MODE_UPDATE = 0x4;
int const MODE_DELETE = 0x8;

int main(int argc, char **argv)
{
	PERF_TABLE_TYPE data;
	PERF_TABLE_TYPE result;

	dbct_ctx *DBCT = dbct_ctx::instance();
	bool background = false;
	bool use_sdc = false;
	string mode_str;
	int mode;
	string src_dir;
	string tmp_dir;
	string user_name;
	string password;

	po::options_description desc((_("Allowed options")).str(SGLOCALE));
	desc.add_options()
		("help", (_("produce help message")).str(SGLOCALE).c_str())
		("background,b", (_("run in background mode")).str(SGLOCALE).c_str())
		("mode,m", po::value<string>(&mode_str)->required(), (_("run in select/insert/update/delete mode")).str(SGLOCALE).c_str())
		("src_dir", po::value<string>(&src_dir)->required(), (_("specify source directory")).str(SGLOCALE).c_str())
		("tmp_dir", po::value<string>(&tmp_dir)->required(), (_("specify temparary directory")).str(SGLOCALE).c_str())
		("sdc,s", (_("run in sdc mode")).str(SGLOCALE).c_str())
		("username,u", po::value<string>(&user_name)->default_value("system"), (_("specify DBC/SDC user name")).str(SGLOCALE).c_str())
		("password,p", po::value<string>(&password)->default_value("system"), (_("specify DBC/SDC password")).str(SGLOCALE).c_str())
	;

	po::variables_map vm;

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			::exit(0);
		}

		po::notify(vm);
	} catch (exception& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << desc << std::endl;
		::exit(1);
	}

	if (vm.count("background"))
		background = true;

	if (background) {
		sys_func& sys_mgr = sys_func::instance();
		sys_mgr.background();
	}

	if (vm.count("sdc"))
		use_sdc = true;

	if (strcasecmp(mode_str.c_str(), "select") == 0) {
		mode = MODE_SELECT;
	} else if (strcasecmp(mode_str.c_str(), "insert") == 0) {
		mode = MODE_INSERT;
	} else if (strcasecmp(mode_str.c_str(), "update") == 0) {
		mode = MODE_UPDATE;
	} else if (strcasecmp(mode_str.c_str(), "delete") == 0) {
		mode = MODE_DELETE;
	} else {
		std::cout << (_("ERROR: Invalid mode given.")).str(SGLOCALE) << std::endl;
		std::cout << desc << std::endl;
		::exit(1);
	}

	// 设置可执行文件名称
	gpp_ctx *GPP = gpp_ctx::instance();
	GPP->set_procname(argv[0]);

	if (*src_dir.rbegin() != '/')
		src_dir += '/';
	if (*tmp_dir.rbegin() != '/')
		tmp_dir += '/';

	boost::timer timer;
	bc::system_clock::time_point start = bc::system_clock::now();
	long rows = 0;

	try {
		dbc_api& dbc_mgr = dbc_api::instance(DBCT);
		sdc_api& sdc_mgr = sdc_api::instance();

		if (!dbc_mgr.login()) {
			std::cout << (_("ERROR: Can't login on DBC.\n")).str(SGLOCALE);
			return 1;
		}

		if (!dbc_mgr.connect(user_name, password)) {
			std::cout << (_("ERROR: Can't connect to DBC.\n")).str(SGLOCALE);
			return 1;
		}

		if (use_sdc) {
			if (!sdc_mgr.login()) {
				std::cout << (_("ERROR: Can't login on SDC.\n")).str(SGLOCALE);
				return 1;
			}

			if (!sdc_mgr.connect(user_name, password)) {
				std::cout << (_("ERROR: Can't connect to SDC.\n")).str(SGLOCALE);
				return 1;
			}

			if (!sdc_mgr.get_meta()) {
				std::cout << (_("ERROR: Can't get meta of SDC - ")).str(SGLOCALE) << DBCT->strerror() << std::endl;
				return 1;
			}
		}

		dbc_mgr.set_autocommit(true);

		BOOST_SCOPE_EXIT((&use_sdc)(&dbc_mgr)(&sdc_mgr)) {
			if (use_sdc) {
				sdc_mgr.disconnect();
				sdc_mgr.logout();
			}

			dbc_mgr.disconnect();
			dbc_mgr.logout();
		} BOOST_SCOPE_EXIT_END

		dbc_te& te_mgr = dbc_te::instance(DBCT);
		dbc_te_t *te = te_mgr.find_te(PERF_TABLE_NAME);
		if (te == NULL) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't find table {1}") % PERF_TABLE_NAME).str(SGLOCALE));
			return -1;
		}
		const dbc_data_t& te_meta = te->te_meta;

		int record_size = 1;
		for (int i = 0; i < te_meta.field_size; i++) {
			record_size += te_meta.fields[i].load_size;
			if (te_meta.delimiter != '\0') // 有分隔符
				record_size++;
		}
		boost::shared_array<char> auto_buf(new char[record_size]);
		char *buf = auto_buf.get();
		bool fixed = (te_meta.delimiter == '\0');

		string mutex_name;
		bf::path path = bf::canonical(bf::path(src_dir));
		for (bf::path::iterator iter = path.begin(); iter != path.end(); ++iter) {
			string item = iter->native();
			if (item == "/")
				continue;

			if (!mutex_name.empty())
				mutex_name += '.';
			mutex_name += item;
		}

		bi::named_mutex file_mutex(bi::open_or_create, mutex_name.c_str());
		scan_file<> fscan(src_dir, "^.*$");
		vector<string> files;
		fscan.get_files(files);

		BOOST_FOREACH(const string& filename, files) {
			string src_full_name;
			string tmp_full_name;

			{
				bi::scoped_lock<bi::named_mutex> lock(file_mutex);

				src_full_name = src_dir + filename;
				if (access(src_full_name.c_str(), R_OK) == -1)
					continue;

				tmp_full_name = tmp_dir + filename;
				if (::rename(src_full_name.c_str(), tmp_full_name.c_str()) == -1) {
					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't rename {1} to {2}") % src_full_name % tmp_full_name).str(SGLOCALE));
					return -1;
				}
			}

			FILE *fp = fopen(tmp_full_name.c_str(), "r");
			if (fp == NULL) {
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't open file {1} - {2}") % tmp_full_name % ::strerror(errno)).str(SGLOCALE));
				return -1;
			}

			BOOST_SCOPE_EXIT((&fp)) {
				fclose(fp);
			} BOOST_SCOPE_EXIT_END

			while (!usignal::get_pending()) {
				string buf_str;
				int status;

				if (fixed)
					status = (fread(buf, record_size, 1, fp) == 1);
				else
					status = (fgets(buf, record_size - 1, fp) != NULL);

				if (!status) {
					if (feof(fp))
						break;

					GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't read record from file {1} - {2}") % tmp_full_name % ::strerror(errno)).str(SGLOCALE));
					return -1;
				}

				if (fixed) {
					DBCT->fixed2record(buf, reinterpret_cast<char *>(&data), te_meta);
				} else {
					buf_str = buf;
					if (!buf_str.empty() && *buf_str.rbegin() == '\n')
						buf_str.resize(buf_str.length() - 1);

					DBCT->variable2record(buf_str, reinterpret_cast<char *>(&data), te_meta);
				}

				if (use_sdc) {
					switch (mode) {
					case MODE_SELECT:
						sdc_mgr.find(te->table_id, 0, &data, &result);
						break;
					case MODE_INSERT:
						sdc_mgr.insert(te->table_id, &data);
						break;
					case MODE_UPDATE:
						sdc_mgr.update(te->table_id, 0, &data, &data);
						break;
					case MODE_DELETE:
						sdc_mgr.erase(te->table_id, 0, &data);
						break;
					default:
						break;
					}
				} else {
					switch (mode) {
					case MODE_SELECT:
						dbc_mgr.find(te->table_id, 0, &data, &result);
						break;
					case MODE_INSERT:
						dbc_mgr.insert(te->table_id, &data);
						break;
					case MODE_UPDATE:
						dbc_mgr.update(te->table_id, 0, &data, &data);
						break;
					case MODE_DELETE:
						dbc_mgr.erase(te->table_id, 0, &data);
						break;
					default:
						break;
					}
				}

				rows++;
				if (rows % 10000 == 0)
					std::cout << (_("rows = ")).str(SGLOCALE) << rows << std::endl;
			}

			if (usignal::get_pending())
				return -1;
		}
	} catch (exception& ex) {
		cout << ex.what();
		return -1;
	}

	bc::system_clock::duration sec = bc::system_clock::now() - start;
	std::cout << (_("rows = {1}, clocks = {2}, elapsed = {3}")% rows % timer.elapsed() %(sec.count() / 1000000000.0)).str(SGLOCALE) << std::endl;
	std::cout << (_("rows per clocks = {1}, rows per elapsed = {2}") % (rows / timer.elapsed()) %(rows * 1000000000.0 / sec.count())).str(SGLOCALE) << std::endl;

	return 0;
}

