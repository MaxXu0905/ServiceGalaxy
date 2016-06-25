#include "prt_dparser.h"

namespace bi = boost::interprocess;
namespace bio = boost::io;
namespace po = boost::program_options;
using namespace std;

namespace ai
{
namespace sg
{

option_dparser::option_dparser(const string& option_name_, int flags_, bool& value_)
	: cmd_dparser(flags_),
	  option_name(option_name_),
	  value(value_)
{
	string desc_msg = (_("turn {1} on/off, 1st positional parameter takes same effect.") % option_name).str(SGLOCALE);
	desc.add_options()
		("option,o", po::value<string>(&option)->default_value(""), desc_msg.c_str())
	;
	pdesc.add("option", 1);
}

option_dparser::~option_dparser()
{
}

dparser_result_t option_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	if (option.empty()) {
		value = !value;
	} else if (strcasecmp(option.c_str(), "on") == 0) {
		value = true;
	} else if (strcasecmp(option.c_str(), "off") == 0) {
		value = false;
	} else {
		err_msg = (_("ERROR: only on/off is allowed for option 'option'")).str(SGLOCALE);
		return DPARSE_CERR;
	}

	if (value)
		cout << option_name << (_(" now on\n")).str(SGLOCALE);
	else
		cout << option_name << (_(" now off\n")).str(SGLOCALE);
	return DPARSE_SUCCESS;
}

tep_dparser::tep_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("table,T", po::value<string>(&table_name)->required(), (_("specify table name to describe, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;
	pdesc.add("table", 1);
}

tep_dparser::~tep_dparser()
{
}

dparser_result_t tep_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	dbc_te& te_mgr = dbc_te::instance(DBCT);
	dbc_te_t *te = te_mgr.find_te(table_name);
	if (te == NULL) {
		err_msg = (_("ERROR: table {1} not exist.") % table_name).str(SGLOCALE);
		return DPARSE_CERR;
	}

	te_mgr.set_ctx(te);

	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;

	long mem_total = 0;
	long used_rows = 0;
	{
		bi::sharable_lock<bi::interprocess_upgradable_mutex> lock(te->rwlock);

		long mem_used = te->mem_used;
		while (mem_used != -1) {
			dbc_mem_block_t *mem_block = bbp->long2block(mem_used);
			mem_total += mem_block->size + sizeof(dbc_mem_block_t);
			mem_used = mem_block->next;
		}

		long row_used = te->row_used;
		while (row_used != -1) {
			row_link_t *link = DBCT->rowid2link(row_used);
			used_rows++;
			row_used = link->next;
		}
	}

	long free_rows = 0;
	{
		int free_slots;
		if (!te->in_flags(TE_VARIABLE))
			free_slots = 1;
		else
			free_slots = MAX_VSLOTS;

		for (int i = 0; i < free_slots; i++) {
			bi::scoped_lock<bi::interprocess_mutex> lock(te->mutex_free[i]);

			long row_free = te->row_free[i];
			while (row_free != -1) {
				row_link_t *link = DBCT->rowid2link(row_free);
				free_rows++;
				row_free = link->next;
			}
		}
	}

	long ref_rows = 0;
	{
		bi::scoped_lock<bi::interprocess_mutex> lock(te->mutex_ref);

		long row_ref = te->row_ref;
		while (row_ref != -1) {
			row_link_t *link = DBCT->rowid2link(row_ref);
			ref_rows++;
			row_ref = link->next;
		}
	}

	cout << (_("MEM TOTAL  USED ROWS  FREE ROWS  REF ROWS   CUR COUNT\n")).str(SGLOCALE);
	cout << "---------- ---------- ---------- ---------- ----------\n";
	cout << std::left << std::setw(10) << mem_total << " "
		<< std::left << std::setw(10) << used_rows << " "
		<< std::left << std::setw(10) << free_rows << " "
		<< std::left << std::setw(10) << ref_rows << " "
		<< std::left << std::setw(10) << te->cur_count << "\n";

	if (verbose) {
		cout << "\n\n" << (_("INDEX NAME                       MEM TOTAL  FREE NODES\n")).str(SGLOCALE);
		cout << "-------------------------------- ---------- ----------\n";

		for (int i = 0; i <= te->max_index; i++) {
			dbc_ie_t *ie = te_mgr.get_index(i);
			if (ie == NULL)
				continue;

			ipc_sem::tas(&ie->node_accword, SEM_WAIT);
			BOOST_SCOPE_EXIT((&ie)) {
				ipc_sem::semclear(&ie->node_accword);
			} BOOST_SCOPE_EXIT_END

			long ie_mem_total = 0;
			long ie_mem_used = ie->mem_used;
			while (ie_mem_used != -1) {
				dbc_mem_block_t *mem_block = bbp->long2block(ie_mem_used);
				ie_mem_total += mem_block->size + sizeof(dbc_mem_block_t);
				ie_mem_used = mem_block->next;
			}

			long free_nodes = 0;
			long node_free = ie->node_free;
			while (node_free != -1) {
				dbc_inode_t *inode = DBCT->long2inode(node_free);
				free_nodes++;
				node_free = inode->right;
			}

			cout << std::left << std::setw(32) << ie->ie_meta.index_name << " "
				<< std::left << std::setw(10) << ie_mem_total << " "
				<< std::left << std::setw(10) << free_nodes << "\n";
		}
	}

	return DPARSE_SUCCESS;
}

show_dparser::show_dparser(int flags_)
	: cmd_dparser(flags_)
{
	desc.add_options()
		("topic,t", po::value<string>(&topic)->required(), (_("specify topic to show, session, parameter, table, sequence, or stat, 1st positional parameter takes same effect.")).str(SGLOCALE).c_str())
		("pattern,p", po::value<string>(&pattern)->default_value(""), (_("specify pattern to match topic, 2nd positional parameter takes same effect.")).str(SGLOCALE).c_str())
	;
	pdesc.add("topic", 1)
		.add("pattern", 2);
}

show_dparser::~show_dparser()
{
}

dparser_result_t show_dparser::parse(int argc, char **argv)
{
	dparser_result_t retval;

	if ((retval = parse_command_line(argc, argv)) != DPARSE_SUCCESS)
		return retval;

	int i;
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	dbc_bbmeters_t& bbmeters = bbp->bbmeters;
	dbc_api& api_mgr = dbc_api::instance(DBCT);

	if (strcasecmp(topic.c_str(), "session") == 0) {
		cout << (_("SESSION_ID PID        ELAPSED_TIME REMAIN_TIME STATUS\n")).str(SGLOCALE);
		cout << "---------- ---------- ------------ ----------- ----------\n";

		bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

		for (dbc_rte_t *rte = DBCT->_DBCT_rtes; rte < DBCT->_DBCT_rtes + bbparms.maxrtes; rte++) {
			if (!rte->in_flags(RTE_IN_USE) || rte->in_flags(RTE_MARK_REMOVED))
				continue;

			int flags = rte->flags;

			cout << std::left << std::setw(10) << rte->rte_id << " "
				<< std::left << std::setw(10) << rte->pid << " ";
			if (rte->timeout == 0 || !(flags & RTE_IN_TRANS)) {
				cout << std::left << std::setw(12) << "-" << " "
					<< std::left << std::setw(11) << "-" << " ";
			} else {
				cout << std::left << std::setw(12) << (time(0) - rte->rtimestamp) << " "
					<< std::left << std::setw(11) << (rte->rtimestamp + rte->timeout - time(0)) << " ";
			}

			int has_data = false;
			if (flags & RTE_IN_TRANS) {
				cout << (_("TRAN")).str(SGLOCALE);
				has_data = true;
			}
			if (flags & RTE_IN_SUSPEND) {
				if (has_data)
					cout << " | ";
				else
					has_data = true;
				cout << (_("SUSPEND")).str(SGLOCALE);
			}
			if (flags & RTE_ABORT_ONLY) {
				if (has_data)
					cout << " | ";
				else
					has_data = true;
				cout << "ABORT";
			}
			if (flags & RTE_IN_PRECOMMIT) {
				if (has_data)
					cout << " | ";
				else
					has_data = true;
				cout << (_("PRECOMMIT")).str(SGLOCALE);
			}
			if (!has_data)
				cout << "-";
			cout << "\n";
		}

		return DPARSE_SUCCESS;
	} else if (strcasecmp(topic.c_str(), "parameter") == 0) {
		cout << (_("NAME             TYPE       VALUE\n")).str(SGLOCALE)
			<< "---------------- ---------- ----------\n";

		if (pattern_match(pattern, "timeout")) {
			cout << std::left << std::setw(16) << "timeout" << ' '
				<< std::left << std::setw(10) << "long" << ' '
				<< std::left << api_mgr.get_timeout() << endl;
		}

		if (pattern_match(pattern, "statlevel")) {
			cout << std::left << std::setw(16) << "statlevel" << ' '
				<< std::left << std::setw(10) << "int" << ' '
				<< std::left << bbparms.stat_level << endl;
		}

		if (pattern_match(pattern, "autocommit")) {
			cout << std::left << std::setw(16) << "autocommit" << ' '
				<< std::left << std::setw(10) << "bool" << ' '
				<< std::left << (api_mgr.get_autocommit() ? "true" : "false") << endl;
		}

		if (pattern_match(pattern, "rownum")) {
			cout << std::left << std::setw(16) << "rownum" << ' '
				<< std::left << std::setw(10) << "int" << ' '
				<< std::left << api_mgr.get_rownum() << endl;
		}

		return DPARSE_SUCCESS;
	} else if (strcasecmp(topic.c_str(), "user") == 0) {
		cout << (_("USER ID USER NAME\n")).str(SGLOCALE)
			<< "------- ----------------\n";

		bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

		dbc_ue_t *ue;
		for (ue = DBCT->_DBCT_ues; ue <= DBCT->_DBCT_ues + bbparms.maxusers; ue++) {
			if (!ue->in_use())
				continue;

			if (pattern_match(pattern, ue->user_name))
				cout << std::left << std::setw(7) << ue->get_user_idx() << ' '
					<< std::left << ue->user_name << endl;
		}

		return DPARSE_SUCCESS;
	} else if (strcasecmp(topic.c_str(), "table") == 0) {
		cout << (_("TABLE NAME\n")).str(SGLOCALE)
			<< "----------------\n";

		bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

		dbc_te_t *te;
		for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
			if (!te->in_active())
				continue;

			if (te->user_id != DBCT->_DBCT_user_id)
				continue;

			if (pattern_match(pattern, te->te_meta.table_name))
				cout << te->te_meta.table_name << endl;
		}

		return DPARSE_SUCCESS;
	} else if (strcasecmp(topic.c_str(), "sequence") == 0) {
		cout << (_("SEQUENCE NAME    MINVALUE            MAXVALUE            CURRVAL             INCREMENT  CACHE CYCLE ORDER\n")).str(SGLOCALE)
			<< "---------------- ------------------- ------------------- ------------------- ---------- ----- ----- -----\n";

		bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

		dbc_se_t *se;
		for (se = DBCT->_DBCT_ses; se < DBCT->_DBCT_ses + bbparms.maxses; se++) {
			if (!(se->flags & TE_IN_USE))
				continue;

			if (pattern_match(pattern, se->seq_name)) {
				cout << std::left << std::setw(16) << se->seq_name << ' '
					<< std::left << std::setw(19) << se->minval << ' '
					<< std::left << std::setw(19) << se->maxval << ' '
					<< std::left << std::setw(19) << se->currval << ' '
					<< std::left << std::setw(10) << se->increment << ' '
					<< std::left << std::setw(5) << se->cache << ' '
					<< std::left << std::setw(5) << (se->cycle ? "YES" : "NO") << ' '
					<< std::left << std::setw(5) << (se->order ? "YES" : "NO") << endl;
			}
		}

		return DPARSE_SUCCESS;
	} else if (strcasecmp(topic.c_str(), "stat") == 0) {
		cout << (_("USER     CATEGORY                         TYPE TIMES      CLOCKS(S)  ELAPSED_TIME    TPS        MIN      MAX\n")).str(SGLOCALE)
			<< "-------- -------------------------------- ---- ---------- ---------- --------------- ---------- -------- --------\n";

		// Output global stats
		if (pattern_match(pattern, "GLOBAL")) {
			for (i = 0; i < TOTAL_STAT; i++) {
				dbc_stat_t& stat_item = bbmeters.stat_array[i];
				cout << std::left << std::setw(8) << (_("SYSTEM")).str(SGLOCALE) << ' '
					<< std::left << std::setw(32) << (_("[GLOBAL]")).str(SGLOCALE) << ' '
					<< std::left << std::setw(4) << STAT_NAMES[i] << ' '
					<< std::left << std::setw(10) << stat_item.times << ' '
					<< std::left << std::setw(10) << std::fixed << (static_cast<double>(stat_item.clocks) / CLOCKS_PER_SEC) << ' '
					<< std::left << std::setw(15) << stat_item.elapsed << ' ';

				if (stat_item.elapsed > 0)
					cout << std::left << std::setw(10) << (stat_item.times / stat_item.elapsed) << ' ';
				else
					cout << std::left << std::setw(10) << "-" << ' ';

				if (stat_item.min_time == numeric_limits<double>::max())
					cout << std::left << std::setw(8) << "-" << ' ';
				else
					cout << std::left << std::setw(8) << stat_item.min_time << ' ';

				if (stat_item.max_time == numeric_limits<double>::min())
					cout << std::left << std::setw(8) << "-" << endl;
				else
					cout << std::left << std::setw(8) << stat_item.max_time << endl;
			}
		}

		bi::scoped_lock<bi::interprocess_recursive_mutex> slock(bbp->syslock);

		dbc_te_t *te;
		dbc_te& te_mgr = dbc_te::instance(DBCT);
		for (te = DBCT->_DBCT_tes; te <= DBCT->_DBCT_tes + bbmeters.curmaxte; te++) {
			if (!te->in_active())
				continue;

			// Output table stats
			if (pattern_match(pattern, te->te_meta.table_name)) {
				for (i = 0; i < TE_TOTAL_STAT; i++) {
					dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
					dbc_stat_t& stat_item = te->stat_array[i];
					cout << std::left << std::setw(8) << ue->user_name << ' '
						<< std::left << std::setw(32) << te->te_meta.table_name << ' '
						<< std::left << std::setw(4) << TE_STAT_NAMES[i] << ' '
						<< std::left << std::setw(10) << stat_item.times << ' '
						<< std::left << std::setw(10) << std::fixed << (static_cast<double>(stat_item.clocks) / CLOCKS_PER_SEC) << ' '
						<< std::left << std::setw(15) << stat_item.elapsed << ' ';

					if (stat_item.elapsed > 0)
						cout << std::left << std::setw(10) << (stat_item.times / stat_item.elapsed) << ' ';
					else
						cout << std::left << std::setw(10) << "-" << ' ';

					if (stat_item.min_time == numeric_limits<double>::max())
						cout << std::left << std::setw(8) << "-" << ' ';
					else
						cout << std::left << std::setw(8) << stat_item.min_time << ' ';

					if (stat_item.max_time == numeric_limits<double>::min())
						cout << std::left << std::setw(8) << "-" << endl;
					else
						cout << std::left << std::setw(8) << stat_item.max_time << endl;
				}
			}

			// Output index stats
			te_mgr.set_ctx(te);
			for (int j = 0; j <= te->max_index; j++) {
				dbc_ie_t *ie = te_mgr.get_index(j);
				if (ie == NULL)
					continue;

				if (pattern_match(pattern, ie->ie_meta.index_name)) {
					for (i = 0; i < IE_TOTAL_STAT; i++) {
						dbc_ue_t *ue = DBCT->_DBCT_ues + (te->user_id & UE_ID_MASK);
						dbc_stat_t& stat_item = ie->stat_array[i];
						cout << std::left << std::setw(8) << ue->user_name << ' '
							<< std::left << "	 " << std::setw(28) << ie->ie_meta.index_name << ' '
							<< std::left << std::setw(4) << IE_STAT_NAMES[i] << ' '
							<< std::left << std::setw(10) << stat_item.times << ' '
							<< std::left << std::setw(10) << std::fixed << (static_cast<double>(stat_item.clocks) / CLOCKS_PER_SEC) << ' '
							<< std::left << std::setw(15) << stat_item.elapsed << ' ';

						if (stat_item.elapsed > 0)
							cout << std::left << std::setw(10) << (stat_item.times / stat_item.elapsed) << ' ';
						else
							cout << std::left << std::setw(10) << "-" << ' ';

						if (stat_item.min_time == numeric_limits<double>::max())
							cout << std::left << std::setw(8) << "-" << ' ';
						else
							cout << std::left << std::setw(8) << stat_item.min_time << ' ';

						if (stat_item.max_time == numeric_limits<double>::min())
							cout << std::left << std::setw(8) << "-" << endl;
						else
							cout << std::left << std::setw(8) << stat_item.max_time << endl;
					}
				}
			}
		}
	}

	return DPARSE_SUCCESS;
}

bool show_dparser::pattern_match(const string& pattern, const string& str)
{
	if (pattern.empty())
		return true;

	string lower_pattern;
	lower_pattern.resize(pattern.length());
	std::transform(pattern.begin(), pattern.end(), lower_pattern.begin(), tolower_predicate());

	string lower_str;
	lower_str.resize(str.length());
	std::transform(str.begin(), str.end(), lower_str.begin(), tolower_predicate());

	return (lower_str.find(lower_pattern) != string::npos);
}

}
}

