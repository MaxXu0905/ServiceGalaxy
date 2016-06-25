#include "bb_dparser.h"
#include "dbc_admin.h"

namespace bi = boost::interprocess;
namespace bio = boost::io;
namespace po = boost::program_options;
using namespace std;

namespace ai
{
namespace sg
{

bbc_dparser::bbc_dparser(int flags_)
	: cmd_dparser(flags_)
{
}

bbc_dparser::~bbc_dparser()
{
}

dparser_result_t bbc_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	boost::shared_ptr<dbc_server> auto_server(new dbc_server());
	dbc_server *server_mgr = auto_server.get();

	server_mgr->clean();
	std::cout << "\n" << (_("BB clean finished.\n")).str(SGLOCALE);

	return DPARSE_SUCCESS;
}

bbp_dparser::bbp_dparser(int flags_)
	: cmd_dparser(flags_)
{
}

bbp_dparser::~bbp_dparser()
{
}

dparser_result_t bbp_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	return show_bbp();
}

dparser_result_t bbp_dparser::show_bbp() const
{
	int i;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_bbmap_t& bbmap = bbp->bbmap;

	cout << (_("Bulletin Board Parameters:\n")).str(SGLOCALE);
	cout << (_("             MAGIC: 0x")).str(SGLOCALE) << setbase(16) << bbparms.magic << setbase(10) << "\n";
	cout << (_("        BB RELEASE: ")).str(SGLOCALE) << bbparms.bbrelease << "\n";
	cout << (_("        BB VERSION: ")).str(SGLOCALE) << bbparms.bbversion << "\n";
	cout << (_("      INITIAL SIZE: 0x")).str(SGLOCALE) << setbase(16) << bbparms.initial_size << setbase(10) << "\n";
	cout << (_("        TOTAL SIZE: 0x")).str(SGLOCALE) << setbase(16) << bbparms.total_size << setbase(10) << "\n";
	cout << (_("           IPC KEY: ")).str(SGLOCALE) << bbparms.ipckey<< "\n";
	cout << (_("          IPC MODE: 0")).str(SGLOCALE) << setbase(8) << bbparms.perm << setbase(10) << "\n";
	cout << (_("          POLLTIME: ")).str(SGLOCALE) << bbparms.scan_unit << "\n";
	cout << (_("         ROBUSTINT: ")).str(SGLOCALE) << bbparms.sanity_scan << "\n";
	cout << (_("        STACK SIZE: 0x")).str(SGLOCALE) << setbase(16) << bbparms.stack_size << setbase(10) << "\n";
	cout << (_("        STAT LEVEL: ")).str(SGLOCALE) << bbparms.stat_level << "\n";
	cout << (_("           SYS DIR: ")).str(SGLOCALE) << bbparms.sys_dir << "\n";
	cout << (_("          SYNC DIR: ")).str(SGLOCALE) << bbparms.sync_dir << "\n";
	cout << (_("          DATA DIR: ")).str(SGLOCALE) << bbparms.data_dir << "\n";
	cout << (_("         SHM COUNT: ")).str(SGLOCALE) << bbparms.shm_count << "\n";
	cout << (_("     RESERVE COUNT: ")).str(SGLOCALE) << bbparms.reserve_count << "\n";
	cout << (_("        POST SIGNO: ")).str(SGLOCALE) << bbparms.post_signo << "\n";
	cout << (_("           SHM IDS: \n")).str(SGLOCALE);
	for (i = 0; i < bbparms.shm_count; i++)
		cout << "                     " << bbmap.shmids[i] << "\n";
	cout << (_("          SHM SIZE: 0x")).str(SGLOCALE) << setbase(16) << bbparms.shm_size << setbase(10) << "\n";
	cout << (_("         MAX USERS: ")).str(SGLOCALE) << bbparms.maxusers << "\n";
	cout << (_("           MAX TES: ")).str(SGLOCALE) << bbparms.maxtes << "\n";
	cout << (_("           MAX SES: ")).str(SGLOCALE) << bbparms.maxses << "\n";
	cout << (_("          MAX RTES: ")).str(SGLOCALE) << bbparms.maxrtes << "\n";
	cout << (_("           SE BKTS: ")).str(SGLOCALE) << bbparms.sebkts << "\n";
	cout << (_("         SEG REDOS: ")).str(SGLOCALE) << bbparms.segment_redos << "\n";
	cout << (_("    TRANSACTION ID: ")).str(SGLOCALE) << bbmeters.tran_id << "\n";
	cout << (_("           CUR TES: ")).str(SGLOCALE) << bbmeters.curtes << "\n";
	cout << (_("          CUR RTES: ")).str(SGLOCALE) << bbmeters.currtes << "\n";
	cout << (_("        CUR MAX TE: ")).str(SGLOCALE) << bbmeters.curmaxte<< "\n";
	cout << (_("          MEM FREE: 0x")).str(SGLOCALE) << setbase(16) << bbmap.mem_free << setbase(10) << "\n";

	size_t free_size = 0;
	{
		bi::scoped_lock<bi::interprocess_recursive_mutex> lock(bbp->memlock);
		long mem_free = bbmap.mem_free;
		while (mem_free != -1) {
			dbc_mem_block_t *mem_block = bbp->long2block(mem_free);
			free_size += mem_block->size + sizeof(dbc_mem_block_t);
			mem_free = mem_block->next;
		}
	}
	cout << (_("         FREE SIZE: {1}M\n")% (free_size / 1024 / 1024)).str(SGLOCALE);

	size_t redo_mem_used = 0;
	{
		dbc_redo_t *redo = DBCT->_DBCT_redo;
		ipc_sem::tas(&redo->accword, SEM_WAIT);
		BOOST_SCOPE_EXIT((&redo)) {
			ipc_sem::semclear(&redo->accword);
		} BOOST_SCOPE_EXIT_END

		long mem_used = redo->mem_used;
		while (mem_used != -1) {
			dbc_mem_block_t *mem_block = bbp->long2block(mem_used);
			redo_mem_used += mem_block->size + sizeof(dbc_mem_block_t);
			mem_used = mem_block->next;
		}
	}
	cout << (_("         REDO SIZE: {1}B\n")% redo_mem_used).str(SGLOCALE);

	return DPARSE_SUCCESS;
}

dump_dparser::dump_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("table,T", po::value<string>(&table_name)->required(), (_("specify table name to dump, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;
	pdesc.add("table", 1);
}

dump_dparser::~dump_dparser()
{
}

dparser_result_t dump_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t* ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_SELECT)) {
		err_msg = (_("ERROR: permission denied, missing SELECT privilege")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.find_te(table_name);
	if (te == NULL) {
		err_msg = (_("ERROR: table {1} not exist.") % table_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	boost::shared_ptr<dbc_server> auto_server(new dbc_server());
	dbc_server *server_mgr = auto_server.get();

	te_mgr.set_ctx(te);
	if (!server_mgr->save_to_file()) {
		err_msg = (_("ERROR: dump table {1} failed.") % table_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	cout << "\n" << (_("dump table {1} successfully.\n") % table_name).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

dumpdb_dparser::dumpdb_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("version,v", (_("print current dbc_dump version")).str(SGLOCALE).c_str())
		("table_id,t", po::value<int>(&table_id)->default_value(-1), (_("specify table_id to dump")).str(SGLOCALE).c_str())
		("table_name,T", po::value<string>(&table_name)->default_value(""), (_("specify table_name to dump")).str(SGLOCALE).c_str())
		("parallel,P",  po::value<int>(&parallel)->default_value(-1), (_("specify parallel threads to dump")).str(SGLOCALE).c_str())
		("commit_size,c", po::value<long>(&commit_size)->default_value(-1), (_("specify rows to commit")).str(SGLOCALE).c_str())
		("background,b", (_("run dbc_sync in background mode")).str(SGLOCALE).c_str())
	;
}

dumpdb_dparser::~dumpdb_dparser()
{
}

dparser_result_t dumpdb_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	string str = (boost::format("dbc_dump -u %1% -p %2%") % cmd_dparser::get_user_name() % cmd_dparser::get_password()).str();
	if (table_id >= 0)
		str += (boost::format(" -t %1%") % table_id).str();
	if (!table_name.empty())
		str += (boost::format(" -T %1%") % table_name).str();
	if (parallel >= 0)
		str += (boost::format(" -P %1%") % parallel).str();
	if (commit_size >= 0)
		str += (boost::format(" -c %1%") % commit_size).str();
	if (vm->count("version"))
		str += " -v";
	if (vm->count("background"))
		str += " -b";

	sys_func& sys_mgr = sys_func::instance();
	int status = sys_mgr.gp_status(::system(str.c_str()));
	if (status != 0) {
		err_msg = (_("ERROR: Failure return from {1}, status {2}, {3}") % str % status % strerror(errno)).str(SGLOCALE);
		return DPARSE_CERR;
	}

	cout << "\n" << (_("dbc_dump executed successfully.\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

extend_dparser::extend_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("add_count,c", po::value<int>(&add_count)->required(), (_("extend additional block count")).str(SGLOCALE).c_str())
	;
}

extend_dparser::~extend_dparser()
{
}

dparser_result_t extend_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	boost::shared_ptr<dbc_server> auto_server(new dbc_server());
	dbc_server *server_mgr = auto_server.get();

	if (!server_mgr->extend_shm(add_count)) {
		err_msg = "ERROR: Failed to extend shared memory, see log for more detailed information.";
		return DPARSE_CERR;
	}

	cout << "\n" << (_("Extend shared memory successfully.\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

kill_dparser::kill_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("session,S", po::value<int>(&session_id)->required(), (_("session id to be killed")).str(SGLOCALE).c_str())
		("signo,s", po::value<int>(&signo)->default_value(SIGTERM), (_("signo to be sent")).str(SGLOCALE).c_str())
	;
}

kill_dparser::~kill_dparser()
{
}

dparser_result_t kill_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;

	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

	if (session_id < 0 || session_id >= bbparms.maxrtes) {
		err_msg = (_("ERROR: session_id given doesn't exist.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_rte_t *rte = DBCT->_DBCT_rtes + session_id;
	if (kill(rte->pid, 0) == -1) {
		err_msg = (_("ERROR: Session died, please do bbclean to cleanup session.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	{
		ipc_sem::tas(&rte->accword, SEM_WAIT);
	 	BOOST_SCOPE_EXIT((&rte)) {
			ipc_sem::semclear(&rte->accword);
		} BOOST_SCOPE_EXIT_END

		if (!(rte->flags & RTE_IN_TRANS)) {
			err_msg = (_("ERROR: Session not in transaction.")).str(SGLOCALE);
			return DPARSE_CERR;
		}

		rte->flags |= RTE_ABORT_ONLY;
	}

	kill(rte->pid, signo);

	cout << (_("Session is marked as terminated successfully.\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

set_dparser::set_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("key,k", po::value<string>(&key)->required(), (_("specify parameter name to be set, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
		("value,v", po::value<string>(&value)->required(), (_("specify parameter value to be set, 2nd positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;
	pdesc.add("key", 1)
		.add("value", 2);
}

set_dparser::~set_dparser()
{
}

dparser_result_t set_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_api& api_mgr = dbc_api::instance();

	if (strcasecmp(key.c_str(), "timeout") == 0) {
		BOOST_FOREACH(const char& ch, value) {
			if (!isdigit(ch)) {
				err_msg = (_("ERROR: para_value must be numeric\n")).str(SGLOCALE);
				return DPARSE_BADOPT;
			}
		}
		api_mgr.set_timeout(::atol(value.c_str()));
	} else if (strcasecmp(argv[1], "statlevel") == 0) {
		BOOST_FOREACH(const char& ch, value) {
			if (!isdigit(ch)) {
				err_msg = (_("ERROR: para_value must be numeric\n")).str(SGLOCALE);
				return DPARSE_BADOPT;
			}
		}
		bbparms.stat_level = ::atoi(value.c_str());
	} else if (strcasecmp(argv[1], "autocommit") == 0) {
		BOOST_FOREACH(const char& ch, value) {
			if (!isdigit(ch)) {
				err_msg = (_("ERROR: para_value must be numeric\n")).str(SGLOCALE);
				return DPARSE_BADOPT;
			}
		}
		api_mgr.set_autocommit(::atoi(value.c_str()) != 0);
	} else if (strcasecmp(argv[1], "rownum") == 0) {
		BOOST_FOREACH(const char& ch, value) {
			if (!isdigit(ch)) {
				err_msg = (_("ERROR: para_value must be numeric\n")).str(SGLOCALE);
				return DPARSE_BADOPT;
			}
		}
		api_mgr.set_rownum(::atoi(value.c_str()));
	} else {
		err_msg = (_("ERROR: unrecognized parameter {1} given.")% key).str(SGLOCALE);
		return DPARSE_CERR;
	}

	cout << (_("Paremeter set successfully.\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

forget_dparser::forget_dparser(int flags_)
	: cmd_dparser(flags_)
{
}

forget_dparser::~forget_dparser()
{
}

dparser_result_t forget_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	cout << (_("A transaction forgotten successfully.\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

check_dparser::check_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("table,T", po::value<string>(&table_name)->required(), (_("specify table name to check")).str(SGLOCALE).c_str())
		("index,I", po::value<string>(&index_name)->default_value(""), (_("specify index name to check")).str(SGLOCALE).c_str())
		("index_id,i", po::value<int>(&index_id)->default_value(-1), (_("specify index id to check")).str(SGLOCALE).c_str())
	;
}

check_dparser::~check_dparser()
{
}

dparser_result_t check_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	if (index_name.empty() && index_id == -1) {
		err_msg = (_("ERROR: missing option -I or -i")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	int i;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.find_te(table_name);
	if (te == NULL) {
		err_msg = (_("ERROR: table {1} not exist.") % table_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	const dbc_data_t& te_meta = te->te_meta;
	te_mgr.set_ctx(te);

	for (i = 0; i <= te->max_index; i++) {
		dbc_ie_t *ie = te_mgr.get_index(i);
		if (ie == NULL)
			continue;

		if (i == index_id || strcasecmp(ie->ie_meta.index_name, index_name.c_str()) == 0) {
			index_id = i;
			break;
		}
	}

	if (i > te->max_index) {
		if (index_id >= 0)
			err_msg = (_("ERROR: index_id {1} not exist.") % index_id).str(SGLOCALE);
		else
			err_msg = (_("ERROR: index {2} not exist.") % index_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	bi::sharable_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

	boost::shared_array<char> auto_data_buf(new char[te_meta.struct_size]);
	char *data_buf = auto_data_buf.get();
	boost::shared_array<char> auto_result(new char[te_meta.struct_size]);
	char *result = auto_result.get();
	long rows;
	long not_found = 0;
	long many_found = 0;
	long not_match = 0;
	long success = 0;
	long rowid = te->row_used;

	// 设置校验起始点
	boost::timer timer;
	bc::system_clock::time_point start = bc::system_clock::now();
	dbc_api& api_mgr = dbc_api::instance(DBCT);

	while (rowid != -1 && !gotintr) {
		char *data = DBCT->rowid2data(rowid);
		char *src_data;
		if (!te->in_flags(TE_VARIABLE) || DBCT->_DBCT_skip_marshal) {
			src_data = data;
		} else {
			memset(data_buf, '\0', te_meta.struct_size);
			memset(result, '\0', te_meta.struct_size);
			te_mgr.unmarshal(data_buf, data);
			src_data = data_buf;
		}
		rows = api_mgr.find(te->table_id, index_id, src_data, result);
		if (rows < 1) {
			print((_("not found\nkey data:")).str(SGLOCALE), src_data, te_meta);
			not_found++;
		} else if (rows > 1) {
			print((_("many found\nkey data:")).str(SGLOCALE), src_data, te_meta);
			many_found++;
		} else if (memcmp(src_data, result, te_meta.struct_size) != 0) {
			print((_("not match:\nkey data:")).str(SGLOCALE), src_data, te_meta);
			print((_("result data:")).str(SGLOCALE), result, te_meta);
			not_match++;
		} else {
			success++;
		}

		row_link_t *link = DBCT->rowid2link(rowid);
		rowid = link->next;
	}

	if (gotintr)
		cout << (_("check cancelled with signal\n")).str(SGLOCALE);
	else if (not_found == 0 && not_match == 0)
		cout << (_("check finished successfully\n")).str(SGLOCALE);
	else
		cout << (_("check finished with error\n")).str(SGLOCALE);

	cout << (_("rows not found: ")).str(SGLOCALE) << not_found << endl;
	cout << (_("rows many found: ")).str(SGLOCALE) << many_found << endl;
	cout << (_("rows not match: ")).str(SGLOCALE) << not_match << endl;
	cout << (_("rows success: ")).str(SGLOCALE) << success << endl;

	// 设置校验结束点
	bc::system_clock::duration sec = bc::system_clock::now() - start;
	std::cout << (_("execution elapsed = {1}s") % (sec.count() / 1000000000.0)).str(SGLOCALE) << endl;
	return DPARSE_SUCCESS;
}

void check_dparser::print(const string& head, const char *data, const dbc_data_t& te_meta)
{
	cout << head << endl;
	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];

		cout << field.field_name << ": ";
		switch (field.field_type) {
		case SQLTYPE_CHAR:
			cout << *(data + field.field_offset);
			break;
		case SQLTYPE_UCHAR:
			cout << *reinterpret_cast<const unsigned char *>(data + field.field_offset);
			break;
		case SQLTYPE_SHORT:
			cout << *reinterpret_cast<const short *>(data + field.field_offset);
			break;
		case SQLTYPE_USHORT:
			cout << *reinterpret_cast<const unsigned short *>(data + field.field_offset);
			break;
		case SQLTYPE_INT:
			cout << *reinterpret_cast<const int *>(data + field.field_offset);
			break;
		case SQLTYPE_UINT:
			cout << *reinterpret_cast<const unsigned int *>(data + field.field_offset);
			break;
		case SQLTYPE_LONG:
			cout << *reinterpret_cast<const long *>(data + field.field_offset);
			break;
		case SQLTYPE_ULONG:
			cout << *reinterpret_cast<const unsigned long *>(data + field.field_offset);
			break;
		case SQLTYPE_FLOAT:
			cout << *reinterpret_cast<const float *>(data + field.field_offset);
			break;
		case SQLTYPE_DOUBLE:
			cout << *reinterpret_cast<const double *>(data + field.field_offset);
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			cout << (data + field.field_offset);
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			cout << *reinterpret_cast<const time_t *>(data + field.field_offset);
			break;
		default:
			break;
		}
		cout << endl;
	}
	cout << endl;
}

enable_dparser::enable_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("table,T", po::value<string>(&table_name)->required(), (_("specify table name to enable")).str(SGLOCALE).c_str())
		("index,I", po::value<string>(&index_name)->default_value(""), (_("specify index name to enable")).str(SGLOCALE).c_str())
		("index_id,i", po::value<int>(&index_id)->default_value(-1), (_("specify index id to enable")).str(SGLOCALE).c_str())
	;
}

enable_dparser::~enable_dparser()
{
}

dparser_result_t enable_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	if (index_name.empty() && index_id == -1) {
		err_msg = (_("ERROR: missing option -I or -i")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_api& api_mgr = dbc_api::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t* ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_CREATE)) {
		err_msg = (_("ERROR: permission denied, missing CREATE privilege.")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.find_te(table_name);
	if (te == NULL) {
		err_msg = (_("ERROR: table {1} not exist.") % table_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	te_mgr.set_ctx(te);

	for (int i = 0; i <= te->max_index; i++) {
		dbc_ie_t *ie = te_mgr.get_index(i);
		if (ie == NULL)
			continue;

		if (i == index_id || strcasecmp(ie->ie_meta.index_name, index_name.c_str()) == 0) {
			err_msg = (_("ERROR: index {1} already exists.") % ie->ie_meta.index_name).str(SGLOCALE);
			return DPARSE_CERR;
		}
	}

	dbc_config& config_mgr = dbc_config::instance();
	const vector<data_index_t>& meta = config_mgr.get_meta();

	for (vector<data_index_t>::const_iterator iter = meta.begin(); iter != meta.end(); ++iter) {
		if (strcasecmp(iter->data.te_meta.table_name, table_name.c_str()) != 0)
			continue;

		const vector<dbc_ie_t>& index_vector = iter->index_vector;
		for (vector<dbc_ie_t>::const_iterator index_iter = index_vector.begin(); index_iter != index_vector.end(); ++index_iter) {
			if (index_iter->index_id == index_id
				|| strcasecmp(index_iter->ie_meta.index_name, index_name.c_str()) == 0) {
				if (!api_mgr.create_index(*index_iter)) {
					err_msg = (_("ERROR: failed to create index {1}, see log for detailed information.") % index_iter->ie_meta.index_name).str(SGLOCALE);
					return DPARSE_CERR;
				}

				cout << "\n" << (_("index {1} enabled successfully.\n")% index_iter->ie_meta.index_name).str(SGLOCALE);
				return DPARSE_SUCCESS;
			}
		}

		if (index_id >= 0)
			err_msg = (_("ERROR: index_id {1} doesn't exist in database.") % index_id).str(SGLOCALE);
		else
			err_msg = (_("ERROR: index {1} doesn't exist in database.") % index_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	err_msg = (_("ERROR: table {1} not exist.") % table_name).str(SGLOCALE);
	return DPARSE_CERR;
}

disable_dparser::disable_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("table,T", po::value<string>(&table_name)->required(), (_("specify table name to disable")).str(SGLOCALE).c_str())
		("index,I", po::value<string>(&index_name)->default_value(""), (_("specify index name to disable")).str(SGLOCALE).c_str())
		("index_id,i", po::value<int>(&index_id)->default_value(-1), (_("specify index id to disable")).str(SGLOCALE).c_str())
	;
}

disable_dparser::~disable_dparser()
{
}

dparser_result_t disable_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	if (index_name.empty() && index_id == -1) {
		err_msg = (_("ERROR: missing option -I or -i")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_api& api_mgr = dbc_api::instance(DBCT);
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t* ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_DROP)) {
		err_msg = (_("ERROR: permission denied, missing DROP privilege")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.find_te(table_name);
	if (te == NULL) {
		err_msg = (_("ERROR: table {1} not exist.") % table_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	te_mgr.set_ctx(te);

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_ie& ie_mgr = dbc_ie::instance(DBCT);

	for (int i = 0; i <= te->max_index; i++) {
		dbc_ie_t *ie = te_mgr.get_index(i);
		if (ie == NULL)
			continue;

		if (i == index_id || strcasecmp(ie->ie_meta.index_name, index_name.c_str()) == 0) {
			// 删除索引时，需要保证没有进程准备使用该索引
			t_sys_index_lock item;
			item.table_id = te->table_id;
			item.index_id = index_id;
			auto_ptr<dbc_cursor> cursor = api_mgr.find(TE_SYS_INDEX_LOCK, 1, &item);
			while (cursor->next()) {
				const t_sys_index_lock *result = reinterpret_cast<const t_sys_index_lock *>(cursor->data());
				if (kill(result->pid, 0) != -1) {
					err_msg = (_("ERROR: index {1} currently used by process {2}") % ie->ie_meta.index_name % result->pid).str(SGLOCALE);
					return DPARSE_CERR;
				}
			}

			{
				// 删除索引时，需要保证没有会话正在操作该表
				bi::scoped_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

				ie_mgr.set_ctx(ie);
				ie_mgr.free_mem();
				bbp->free(te->index_hash[i] - sizeof(dbc_mem_block_t));
				te->index_hash[i] = -1;
			}

			cout << "\n" << (_("index {1} disabled successfully.\n")% ie->ie_meta.index_name).str(SGLOCALE);
			return DPARSE_SUCCESS;
		}
	}

	if (index_id >= 0)
		err_msg = (_("ERROR: index_id {1} not exist.") % index_id).str(SGLOCALE);
	else
		err_msg = (_("ERROR: index {2} not exist.") % index_name).str(SGLOCALE);
	return DPARSE_CERR;
}

reload_dparser::reload_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("table,T", po::value<vector<string> >(&table_names)->required(), (_("specify table name(s) to reload, positional parameter(s) take same effect.")).str(SGLOCALE).c_str())
		("safe,s", (_("load table in safe mode")).str(SGLOCALE).c_str())
	;

	pdesc.add("table", -1);
}

reload_dparser::~reload_dparser()
{
}

dparser_result_t reload_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	for (int i = 0; i < table_names.size(); i++) {
		for (int j = i + 1; j < table_names.size(); j++) {
			if (strcasecmp(table_names[i].c_str(), table_names[j].c_str()) == 0) {
				err_msg = (_("ERROR: table {1} is specified multiple times") % table_names[i]).str(SGLOCALE);
				return DPARSE_CERR;
			}
		}
	}

	bool safe_mode = vm->count("safe");
	dbc_ue& ue_mgr = dbc_ue::instance(DBCT);
	dbc_ue_t* ue = ue_mgr.get_ue();
	if (ue == NULL || !(ue->perm & DBC_PERM_RELOAD)) {
		err_msg = (_("ERROR: permission denied, missing RELOAD privilege")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = NULL;
	boost::shared_ptr<dbc_server> auto_server(new dbc_server());
	dbc_server *server_mgr = auto_server.get();

	if (DBCP->_DBCP_proc_type == DBC_SLAVE) {
		// 等待文件可用
		server_mgr->wait_ready();
	}

	if (table_names.size() == 1) {
		const string& table_name = table_names[0];

		te = te_mgr.find_te(table_name);
		if (te == NULL) {
			err_msg = (_("ERROR: table {1} not exist.") % table_name).str(SGLOCALE);
			return DPARSE_CERR;
		}

		bool status;
		if (safe_mode)
			status = server_mgr->safe_load(te);
		else
			status = server_mgr->unsafe_load(te);
		if (!status) {
			err_msg = (_("ERROR: reload table {1} failed.") % table_name).str(SGLOCALE);
			return DPARSE_CERR;
		}

		cout << "\n" << (_("reload table {1} successfully.\n")% table_name).str(SGLOCALE);
	} else if (!table_names.empty()) {
		dbc_te& te_mgr = dbc_te::instance(DBCT);
		boost::thread_group thread_group;
		vector<int> retvals;
		vector<dbc_te_t *> tes;
		int i;

		for (i = 0; i < table_names.size(); i++) {
			te = te_mgr.find_te(table_names[i]);
			if (te == NULL) {
				err_msg = (_("ERROR: table {1} not exist.") % table_names[i]).str(SGLOCALE);
				return DPARSE_CERR;
			}
			tes.push_back(te);
		}

		dbc_api& api_mgr = dbc_api::instance(DBCT);
		int64_t user_id = api_mgr.get_user();

		retvals.resize(table_names.size());
		for (i = 0; i < table_names.size(); i++) {
			retvals[i] = -1;
			thread_group.create_thread(boost::bind(&reload_dparser::s_load, safe_mode, user_id, tes[i], boost::ref(retvals[i])));
		}

		thread_group.join_all();
		for (i = 0; i < table_names.size(); i++) {
			if (retvals[i] == -1) {
				err_msg = (_("ERROR: reload table {1} failed.") % table_names[i]).str(SGLOCALE);
				return DPARSE_CERR;
			}
		}
	} else {
		boost::thread_group thread_group;
		vector<int> retvals;
		for (int i = 0; i <= bbmeters.curmaxte; i++)
			retvals.push_back(0);

		dbc_api& api_mgr = dbc_api::instance(DBCT);
		int64_t user_id = api_mgr.get_user();

		int idx = 0;
		for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++, idx++) {
			if (te->in_flags(TE_IN_MEM) || !te->in_active())
				continue;

			retvals[idx] = -1;
			thread_group.create_thread(boost::bind(&reload_dparser::s_load, user_id, safe_mode, te, boost::ref(retvals[idx])));
		}

		thread_group.join_all();
		for (idx = 0; idx <= bbmeters.curmaxte; idx++) {
			if (retvals[idx] == -1) {
				dbc_te_t *error_te = DBCT->_DBCT_tes + idx;
				err_msg = (_("ERROR: reload table {1} failed.") % error_te->te_meta.table_name).str(SGLOCALE);
				return DPARSE_CERR;
			}
		}
	}

	return DPARSE_SUCCESS;
}

void reload_dparser::s_load(bool safe_mode, int64_t user_id, dbc_te_t *te, int& retval)
{
	retval = -1;
	boost::shared_ptr<dbc_server> auto_server(new dbc_server());
	dbc_server *server_mgr = auto_server.get();
	gpp_ctx *GPP = gpp_ctx::instance();
	dbct_ctx *DBCT = dbct_ctx::instance();

	if (!DBCT->attach_shm()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't attach to shared memory")).str(SGLOCALE));
		return;
	}

	DBCT->init_globals();

	dbc_api& api_mgr = dbc_api::instance(DBCT);
	api_mgr.set_user(user_id);

	BOOST_SCOPE_EXIT((&DBCT)) {
		DBCT->detach_shm();
	} BOOST_SCOPE_EXIT_END

	dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
	if (!rte_mgr.get_slot()) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't register to DBC")).str(SGLOCALE));
		return;
	}

	BOOST_SCOPE_EXIT((&rte_mgr)) {
		rte_mgr.put_slot();
	} BOOST_SCOPE_EXIT_END

	try {
		if (safe_mode)
			retval = (server_mgr->safe_load(te) ? 0 : -1);
		else
			retval = (server_mgr->unsafe_load(te) ? 0 : -1);
	} catch (exception& ex) {
		GPP->write_log(__FILE__, __LINE__, ex.what());
	}
}

}
}

