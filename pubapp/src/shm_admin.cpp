#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include "shm_admin.h"
#include "shm_pair.h"
#include "usignal.h"
#include "sys_func.h"

using namespace ai::sg;
using namespace std;

int main(int argc, char **argv)
{
	try {
		// set program name prior to making the first userlog
		gpp_ctx *GPP = gpp_ctx::instance();
		GPP->set_procname(argv[0]);
		shm_admin instance;
		return instance.run(argc, argv);
	} catch (exception &ex) {
		cout << ex.what() << std::endl;
	}
}

namespace ai
{
namespace sg
{

/*	The following lines fill in the cmds array.
 *	The fields are:
 *		1. Descriptive help message (msg number)
 *		2. Name of the command.
 *		3. Abbreviation of the command name.
 *		4. Options which the command takes, if any (msg number)
 *		5. Flags, as defined in tmadmin.h.
 *		6. Routine name.
 */
cmds_t shm_admin::cmds[] = {
	{ "Echo input command lines. The setting is toggled if no option is given.", "echo", "e", "[{off | on}]", 0, &shm_admin::shmae },
	{ "Print the abbreviation, arguments, and description for the specified\n command, or for all commands.", "help", "h", "[{command | all}]", PAGE, &shm_admin::shmah },
	{ "Produce output in verbose mode. The setting is toggled if no option\n is given.", "verbose", "v", "[{off | on}]", 0, &shm_admin::shmav },
	{ "Print data in hexadecimal mode.", "hex", "hex", "[{off | on}]", 0, &shm_admin::shmahex },
	{ "Terminate this session.", "quit", "q", "", 0, &shm_admin::shmaq },
	{ "Malloc memory for given size from shared memory.", "malloc", "m", "-s size",  0, &shm_admin::shmam },
	{ "Free memory for give address.", "free", "f", "{-a address |-i block_index -O block_offset}", 0, &shm_admin::shmaf },
	{ "Realloc memory for given address and size from shared memory.", "realloc", "ra", "{-a address |-i block_index -O block_offset} -s size",  0, &shm_admin::shmara },
	{ "Create shared memory for given component key.", "createcom", "cc", "-c com_key -h hash_size -d data_size -l data_len", PAGE, &shm_admin::shmacc },
	{ "Destroy shared memory for given component key.", "destroycom", "dc", "-c com_key", PAGE, &shm_admin::shmadc },
	{ "Print header information of shared memory.", "printheader", "ph", "", PAGE, &shm_admin::shmaph },
	{ "Print used shared memory link.", "printused", "pu", "[-i block_index]", PAGE, &shm_admin::shmapu },
	{ "Print free shared memory link.", "printfree", "pf", "[-i block_index]", PAGE, &shm_admin::shmapf },
	{ "Print header information of shared memory for given component key.", "printcom", "pc", "[-c com_key]", PAGE, &shm_admin::shmapc },
	{ "Print service_key for given component key.", "printkey", "pk", "-c com_key", PAGE, &shm_admin::shmapk },
	{ "Print service data information for given service key.", "printdata", "pd", "[-c com_key] -S svc_key [-s size]", PAGE, &shm_admin::shmapd },
	{ "Invalidate service data information for given service key.", "invalidatedata", "id", "-c com_key -S svc_key", PAGE, &shm_admin::shmaid },
	{ "Lock system spinlock.", "lock", "l", "-T {sys|free|used} [-i block_index]", 0, &shm_admin::shmal },
	{ "Unlock system spinlock.", "unlock", "ul", "-T {sys|free|used} [-i block_index]", 0, &shm_admin::shmaul },
	{ "dump data from given address.", "dump", "d", "{-a address | -i block_index -O block_offset} -s size [-o offset]", PAGE, &shm_admin::shmad },
	{ "do sanity check and free memory for which process has died.", "clean", "c", "", 0, &shm_admin::shmac },
	{ "Remove shared memory.", "remove", "rm", "", 0, &shm_admin::shmarm }
};

int shm_admin::maxcmds = sizeof(shm_admin::cmds) / sizeof(shm_admin::cmds[0]);
int shm_admin::gotintr = 0;

shm_admin::shm_admin()
{
	echo = false;
	private_flag = false;
	verbose = false;
	paginate = false;

	GPP = gpp_ctx::instance();
	GPT = gpt_ctx::instance();
}

shm_admin::~shm_admin()
{
}

int shm_admin::run(int argc, char *argv[])
{
	int c;
	key_t key = -1;
	size_t block_size = 0;
	int block_count = 0;
	int shmflg = IPC_CREAT | 0660;
	int monitor_seconds = 0;

	opterr = 0; // don't want getopt to print error messages
	while ((c = ::getopt(argc, argv, "k:b:c:p:m:v")) != EOF) {
		switch (c) {
		case 'k':
			key = ::atoi(optarg);
			break;
		case 'b':
			block_size = ::atol(optarg);
			break;
		case 'c':
			block_count = ::atoi(optarg);
			break;
		case 'p':
			private_flag = true;
			break;
		case 'm':
			monitor_seconds = ::atoi(optarg);
			break;
		case 'v':
			cout << (_("Usage: {1} -k key [-b block_size] [-c block_count] [-m monitor_seconds] [-v]\n") % argv[0]).str(SGLOCALE);
			::exit(0);
		default:
			// argv[optind-1] contains current option, e.g., -Z
			cerr << (_("ERROR: Invalid option: {1}") % argv[optind - 1]).str(SGLOCALE)<< "\n";
			::exit(1);
		}
	}

	// The following tests whether any arguments such as "garbage" exist
	if (optind < argc) {
		cerr << (_("ERROR: Invalid argument: {1}") % argv[optind] ).str(SGLOCALE)<< "\n";
		::exit(1);
	}

	if (key <= 0) {
		cout << (_("Usage: {1} -k key [-b block_size] [-c block_count] [-m monitor_seconds] [-v]\n") % argv[0]).str(SGLOCALE);
		::exit(1);
	}

	// go to routine intr on interrupt
	if (usignal::signal(SIGINT, SIG_IGN) != SIG_IGN)
		usignal::signal(SIGINT, intr);
	if (usignal::signal(SIGPIPE, SIG_IGN) != SIG_IGN)
		usignal::signal(SIGPIPE, intr);

	if (mgr.attach(key, shmflg, block_size, block_count) == -1) {
		cout << (_("Can't attach to shared memory, key = {1}") % key).str(SGLOCALE) <<std::endl;
		::exit(1);
	}

	if (monitor_seconds > 0) {
		sys_func::instance().background();
		while (1) {
			shmac();
			::sleep(monitor_seconds);
		}
	}

	for (;;) {
		if (gotintr)
			::exit(0);

		switch (process()) {
		case SHMQUIT: // leave tmadmin
			if (private_flag)
				shmarm();
			::exit(0);
		}
	}

	return 0;
}

int shm_admin::process()
{
	/*
	 * This is the main loop of tmadmin. The prompt is printed,
	 * command line is read and parsed, and the appropriate routine
	 * is called. Error messages are also handled here.
	 */
	cmds_t *ct;
	int err, cur;

	while (1) {
		if (gotintr) {
			gotintr = 0;
			cerr << "\n";
		}

    cout << "\n>";

		// flush the output streams to clear out the buffer
		cout << std::flush;
		cerr << std::flush;

		// Read in the command line here
		std::getline(cin, cmdline);
		if (cmdline.empty()) {
			if (cin.eof()) {
				// actually end of file; look like "quit"
				cmdline = "quit\n";
				cout << "\n";
			} else {
				// just an interrupt, so give prompt again
				continue;
			}
		}

		// Parse the command line here. Note that SHMCERR is never returned from "scan".
		if ((err = scan(cmdline)) > 0) {
			if (echo)
				cout << cmdline << std::endl;
			cerr << shm_emsgs(err) << "\n";
			continue;
		}

		/*
		 * numargs == 0 means that nothing but white space was entered
		 * on the command line, so repeat the previous command
		 */
		if (numargs == 0) {
			/*
			 * Set the variables from the previous command. Note
			 * that this assumes that the cmdargs array hasn't been
			 * changed since the last command line was parsed.
			 * Therefore, numargs simply indicates the number of
			 * valid elements in the cmdargs array.
			 */
			numargs = oldargs;
			cmdline = prevcmd;
			cout << prevcmd << std::endl;
		} else {
			/*
			 * Set the variables in case this command line has
			 * to be called again
			 */
			oldargs = numargs;
			prevcmd = cmdline;
			if (echo)
				cout << cmdline << std::endl;
		}

		if (cmdline[0] == '!') { // shell command entered
			if (cmdline[1] == '!') {
				/*
				 * Repeat previous shell command, but first
				 * print what the previous command was.
				 */
				cout << prevsys << std::endl;
			} else {
				/*
				 * Save for next !! command. (cmdline+1) is
				 * used to remove the leading '!'
				 */
				prevsys = cmdline.substr(1);
			}
			if (sys_func::instance().gp_status(::system(prevsys.c_str())) != 0)
				cerr << (_("ERROR: Error executing {1}") % prevsys).str(SGLOCALE)<< "\n";
			continue;
		}

		if (cmdline[0] == '#') // comment line
			continue;

		ct = cmds;		/* array of legal commands */
		cur = maxcmds;		/* number of commands */

		while (cur--) {
			if (::strcmp(cmdargs[0], ct->name) == 0 || ::strcmp(cmdargs[0], ct->alias) == 0) {
				// check if a valid command was entered
				if (!validcmd(ct))
					break;
				// set current values from defaults
				if ((err = determ()) > 0) {
					cur = -1;
					break;
				}
				// call the proper routine
				if ((ct->flags & PAGE) && paginate) {
					usignal::signal(SIGINT, SIG_IGN);
					paginate = sys_func::instance().page_start(&pHandle);
				}
				err = (this->*ct->proc)();
				if ((ct->flags & PAGE) && paginate) {
					sys_func::instance().page_finish(&pHandle);
					usignal::signal(SIGINT, shm_admin::intr);
				}
				break;
			}
			ct++;
		}

		/*
		 * Special conditions: we want to break out of here and
		 * let the main routine take control.
		 */
		if (err == SHMQUIT)
			break;

		if (cur == -1)	// cur == -1 means no such command
			err = SHMBADCMD;

		if (err > 0) {
			/*
			 * if err == SHMCERR, then we want to print out
			 * the custom crafted error message stored in "shmcerr"
			 */
			cerr << (err == SHMCERR ? shmcerr : shm_emsgs(err)) << "\n";
			if (err == SHMSYNERR)
				cerr << ct->name << "(" << ct->alias << ") " << ct->prms_msg;
		}
	}

	return err;
}

int shm_admin::shmae()
{
	return off_on("Echo", echo);
}

int shm_admin::shmahex()
{
	return off_on("hex", hex);
}

int shm_admin::shmah()
{
	cmds_t *ct; // command table
	int len, cur;

	if (numargs != 1) {
		if (numargs != 2 || !c_vopt)
			return SHMSYNERR;
	}

	ct = cmds; // ct points to the begin of the cmds array
	cur = maxcmds; // Number of commands to consider

	if (c_vopt && c_value != "all") {
		// we do this only if a particular command, such as "psr" was given
		while (cur--) {
			// Now go through the array and look for it
			if (c_value == ct->name || c_value == ct->alias) {
				// If an invalid command is given, return "No such command."
				if (!validcmd(ct))
					cur = -1;
				else
					cur = 1; // Only want to print out
				break; // 1 entry
			}
			ct++; // Go on to next entry
		}
		if (cur == -1) // Couldn't find the desired command
			return SHMBADCMD;
	}

	for (; cur-- && !gotintr; ct++) {
		// determine if this is a valid command
		if (!validcmd(ct))
			continue;

		cout << std::endl;
		cout << ct->name <<" (" << ct->alias << ")," << ct->prms_msg ;


		if (c_vopt) {
			// If an argument given, then print out a description also.
			cout << std::endl;
			// 4 is added to take into account 2 spaces and "()"
			len = 4 + ::strlen(ct->name) + ::strlen(ct->alias) + ::strlen(ct->prms_msg);
			// Remove an extra underline on the end if no parms.
			if (!::strcmp(ct->prms_msg, ""))
				len--;
			if (len > 75)
				len = 75;

			while (len--) // Underline
				cout << '-';

			cout << "\n" << ct->desc_msg << "\n";
		}
	}

	if (!c_vopt) // Need to end the last line properly
		cout << '\n';

	return 0;
}

int shm_admin::shmav()
{
	return off_on("Verbose", d_verbose);
}

int shm_admin::shmaq()
{
	return SHMQUIT;
}

int shm_admin::shmam()
{
	if (numargs != 2)
		return SHMARGCNT;

	if (!c_sopt)
		return SHMSYNERR;

	void *addr = mgr.malloc(c_size);
	if (addr == NULL) {
		cerr << (_("ERROR: malloc() failed, error_code = {1}") % GPT->_GPT_error_code).str(SGLOCALE);
		return SHMCERR;
	}

	cout <<(_("INFO: Memory allocated, address = 0x")).str(SGLOCALE);
	cout << std::setbase(16) << reinterpret_cast<unsigned long>(addr)
		<< std::setbase(10) << std::endl;
	return 0;
}

int shm_admin::shmaf()
{
	shm_link_t shm_link;
	shm_data_t *shm_data;
	if (c_aopt) {
		shm_data = (shm_data_t *)(c_address - sizeof(shm_data_t) + sizeof(void *));
		if (mgr.get_shm_link(shm_data) == shm_link_t::SHM_LINK_NULL) {
			cerr << (_("ERROR: Address provided is not correct.")).str(SGLOCALE);
			return SHMCERR;
		}
	} else if (c_iopt && c_Oopt) {
		shm_link.block_index = c_block_index;
		shm_link.block_offset = c_block_offset;
		shm_data = mgr.get_shm_data(shm_link);
		if (shm_data == NULL) {
			cerr << (_("ERROR: Address provided is not correct.")).str(SGLOCALE);
			return SHMCERR;
		}
	} else {
		return SHMSYNERR;
	}

	GPT->_GPT_error_code = 0;
	mgr.free(&shm_data->data);
	if (GPT->_GPT_error_code) {
		cerr << (_("ERROR: free() failed, error_code = {1}") % GPT->_GPT_error_code).str(SGLOCALE);
		return SHMCERR;
	}

	cout <<(_("INFO: Memory freed successfully")).str(SGLOCALE) << std::endl;
	return 0;
}

int shm_admin::shmara()
{
	shm_link_t shm_link;
	shm_data_t *shm_data;
	if (c_aopt) {
		shm_data = (shm_data_t *)(c_address - sizeof(shm_data_t) + sizeof(void *));
		if (mgr.get_shm_link(shm_data) == shm_link_t::SHM_LINK_NULL) {
			cerr << _("ERROR: Address provided is not correct.").str(SGLOCALE);
			return SHMCERR;
		}
	} else if (c_iopt && c_Oopt) {
		shm_link.block_index = c_block_index;
		shm_link.block_offset = c_block_offset;
		shm_data = mgr.get_shm_data(shm_link);
		if (shm_data == NULL) {
			cerr << _("ERROR: Address provided is not correct.").str(SGLOCALE);
			return SHMCERR;
		}
	} else {
		return SHMSYNERR;
	}

	if (!c_sopt)
		return SHMSYNERR;

	void *addr = mgr.realloc(&shm_data->data, c_size);
	if (addr == NULL) {
		cerr << (_("ERROR: malloc() failed, error_code = {1}") %GPT->_GPT_error_code ).str(SGLOCALE);
		return SHMCERR;
	}

	cout << (_("INFO: Memory reallocated, address = 0x")).str(SGLOCALE);
	cout << std::setbase(16) << reinterpret_cast<unsigned long>(addr) << std::setbase(10) << std::endl;
	return 0;
}

int shm_admin::shmacc()
{
	if (!c_copt || !c_hopt || !c_dopt)
		return SHMSYNERR;

	try {
		shm_pair shp(mgr, c_com_key, c_hash_size, c_data_size, c_data_len);

		cout <<(_("INFO: Component key created successfully.")).str(SGLOCALE) << std::endl;

	} catch (exception& ex) {
		if (GPT->_GPT_error_code == GPEINVAL)
			cerr << (_("ERROR: Invalid parameter provided.")).str(SGLOCALE);
		else
			cerr << ex.what();
		return SHMCERR;
	}

	return 0;
}

int shm_admin::shmadc()
{
	if (!c_copt)
		return SHMSYNERR;

	try {
		shm_pair shp(mgr, c_com_key);

		GPT->_GPT_error_code = 0;
		mgr.free(shp.shp_hdr);
		if (GPT->_GPT_error_code) {
			cerr << (_("ERROR: Component key destroy failed, error_code = {1}") % GPT->_GPT_error_code).str(SGLOCALE);
			return SHMCERR;
		}

		cout <<(_("INFO: Component key destroyed successfully.")).str(SGLOCALE) << std::endl;
	} catch (exception& ex) {
		if (GPT->_GPT_error_code == GPEINVAL)
			cerr << (_("ERROR: Invalid parameter provided.")).str(SGLOCALE);
		else
			cerr << ex.what();
		return SHMCERR;
	}

	return 0;
}

int shm_admin::shmaph()
{
	int i;

	if (numargs > 1)
 		return SHMARGCNT;

	shm_malloc_t *shm_hdr = mgr.shm_hdr;

	cout <<(_("Shared Memory Parameters:")).str(SGLOCALE) << std::endl;
	cout <<(_("          SHM COUNT:  {1}") % shm_hdr->shm_count).str(SGLOCALE) << std::endl;
	cout <<(_("        BLOCK COUNT:  {1}") % shm_hdr->block_count).str(SGLOCALE) << std::endl;
	cout <<(_("         BLOCK SIZE:  {1}") % shm_hdr->block_size).str(SGLOCALE) << std::endl;
	cout <<(_("       BUCKET INDEX:  {1}") % shm_hdr->bucket_index).str(SGLOCALE) << std::endl;
	for (i = 0; i < shm_hdr->shm_count; i++) {
		cout <<(_("          IPCKEY[{1}]:  ") % i % shm_hdr->keys[i]).str(SGLOCALE) << std::endl;
		cout <<(_("Shared Memory ID[{1}]:  ") % i % shm_hdr->shm_ids[i]).str(SGLOCALE) << std::endl;
		cout <<(_("     SHM Address[{1}]:  ") % i).str(SGLOCALE) << std::setbase(16) << (unsigned long)shm_hdr->shm_addrs[i] << std::setbase(10) << "\n";
	}
	cout <<(_("          SYSLOCK(spin, pid, time): ({1}, {2}, {3})")
				% shm_hdr->syslock.spinlock
				% shm_hdr->syslock.lock_pid
				% shm_hdr->syslock.lock_time).str(SGLOCALE) << "\n";

	for (i = 0; i < SHM_BUCKETS; i++) {
		cout <<(_("    FREE SPINLOCK(spin, pid, time): ({1}, {2}, {3})")
				% shm_hdr->free_lock[i].spinlock
				% shm_hdr->free_lock[i].lock_pid
				% shm_hdr->free_lock[i].lock_time).str(SGLOCALE) << "\n";
		cout <<(_("    USED SPINLOCK(spin, pid, time): ({1}, {2}, {3})")
				% shm_hdr->used_lock[i].spinlock
				% shm_hdr->used_lock[i].lock_pid
				% shm_hdr->used_lock[i].lock_time).str(SGLOCALE) << "\n";
		cout <<(_("                         FREE LINK: (")).str(SGLOCALE);
		cout << std::setbase(16);
		cout << shm_hdr->free_link[i].block_index << ", "<< shm_hdr->free_link[i].block_offset << ")\n";
		cout <<(_("                         USED LINK: (")).str(SGLOCALE);
		cout << shm_hdr->used_link[i].block_index << ", "<< shm_hdr->used_link[i].block_offset << ")\n";
		cout << std::setbase(10);
	}

 	return 0;
}

int shm_admin::shmapu()
{
	int i;
	int count;
	shm_link_t shm_link;
	shm_data_t *shm_data;

	if (numargs > 2)
		return SHMARGCNT;

	shm_malloc_t *shm_hdr = mgr.shm_hdr;

	for (i = 0; i < SHM_BUCKETS; i++) {
		if (c_iopt && c_block_index != i)
			continue;

		shm_hdr->used_lock[i].lock();
		if (shm_hdr->used_link[i] == shm_link_t::SHM_LINK_NULL) {
			shm_hdr->used_lock[i].unlock();
			continue;
		}

		cout <<(_("BUCKET {1}") % i).str(SGLOCALE) << ":\n";
		count = 0;
		shm_link = shm_hdr->used_link[i];
		for (; shm_link != shm_link_t::SHM_LINK_NULL; shm_link = shm_data->next) {
			cout <<(_("        USED LINK[{1}]:     (") % count++).str(SGLOCALE);
			cout << std::setbase(16);
			cout << shm_link.block_index << ", " << shm_link.block_offset << ")\n";
			cout << std::setbase(10);
			shm_data = mgr.get_shm_data(shm_link);
			if (shm_data == NULL)
				break;
		}
		cout << "\n";
		shm_hdr->used_lock[i].unlock();
	}

	return 0;
}

int shm_admin::shmapf()
{
	int i;
	int count;
	shm_link_t shm_link;
	shm_data_t *shm_data;

	if (numargs > 2)
		return SHMARGCNT;

	shm_malloc_t *shm_hdr = mgr.shm_hdr;

	for (i = 0; i < SHM_BUCKETS; i++) {
		if (c_iopt && c_block_index != i)
			continue;

		shm_hdr->free_lock[i].lock();
		if (shm_hdr->free_link[i] == shm_link_t::SHM_LINK_NULL) {
			shm_hdr->free_lock[i].unlock();
			continue;
		}

		cout <<(_("BUCKET {1}") % i).str(SGLOCALE) << ":\n";
		count = 0;
		shm_link = shm_hdr->free_link[i];
		for (; shm_link != shm_link_t::SHM_LINK_NULL; shm_link = shm_data->next) {
			cout <<(_("        FREE LINK[{1}]:     (") % count++).str(SGLOCALE);
			cout << std::setbase(16);
			cout << shm_link.block_index << ", " << shm_link.block_offset << ")\n";
			cout << std::setbase(10);
			shm_data = mgr.get_shm_data(shm_link);
			if (shm_data == NULL)
				break;
		}
		cout << "\n";
		shm_hdr->free_lock[i].unlock();
	}

	return 0;
}

int shm_admin::shmapc()
{
	if (!c_copt) {
		shm_link_t used_link;
		shm_data_t *used_data;
		shm_malloc_t *shm_hdr = mgr.shm_hdr;

		cout <<(_("Shared Memory Pair Parameters:")).str(SGLOCALE) << std::endl;
		cout <<(_("-----------------------------------:")).str(SGLOCALE) << std::endl;

		for (int i = 0; i <= shm_hdr->bucket_index; i++) {
			shm_hdr->used_lock[i].lock();

			// First, go through used_list to find an entry
			used_link = shm_hdr->used_link[i];
			for (; used_link != shm_link_t::SHM_LINK_NULL; used_link = used_data->next) {
				used_data = mgr.get_shm_data(used_link);
				if (used_data == NULL) {
					shm_hdr->used_lock[i].unlock();
					return 0;
				}

				if (!(used_data->flags & SHM_KEY_INUSE)) {
					shm_hdr->used_lock[i].unlock();
					continue;
				}

				shp_hdr_t *shp_hdr = reinterpret_cast<shp_hdr_t *>(&used_data->data);
				cout <<(_("                  KEY:  {1}") % shp_hdr->key).str(SGLOCALE) << std::endl;
				cout <<(_("LOCK(spin, pid, time):  ({1}, {2}, {3})")
							% shp_hdr->lock.spinlock
							% shp_hdr->lock.lock_time
							 % shp_hdr->lock.lock_time).str(SGLOCALE) << "\n";
				cout <<(_("                MAGIC:  0x")).str(SGLOCALE);
				cout << std::setbase(16) << shp_hdr->magic << std::setbase(10) <<"\n";
				cout <<(_("            REF COUNT:  {1}") % shp_hdr->ref_count).str(SGLOCALE) << "\n";
				cout <<(_("           TOTAL SIZE:  {1}") % shp_hdr->total_size).str(SGLOCALE) << "\n";
				cout <<(_("            HASH SIZE:  {1}") % shp_hdr->hash_size).str(SGLOCALE) << "\n";
				cout <<(_("            DATA SIZE:  {1}") % shp_hdr->data_size).str(SGLOCALE) << "\n";
				cout <<(_("             DATA LEN:  {1}") % shp_hdr->data_len).str(SGLOCALE) << "\n";
				cout <<(_("            FREE LINK:  {1}") % shp_hdr->free_link).str(SGLOCALE) << "\n";
				cout << "-----------------------------------\n";

				shm_hdr->used_lock[i].unlock();
			}

			shm_hdr->used_lock[i].unlock();
		}

		return 0;
	}

	try {
		shm_pair shp(mgr, c_com_key);
		shp_hdr_t *shp_hdr = shp.shp_hdr;

		if (shp_hdr == NULL)
			return SHMBADCMD;

		cout <<(_("Shared Memory Pair Parameters:")).str(SGLOCALE) << std::endl;
		cout <<(_("                  KEY:  {1}") % shp_hdr->key).str(SGLOCALE) << std::endl;
		cout <<(_("LOCK(spin, pid, time):  ({1}, {2}, {3})")
					% shp_hdr->lock.spinlock
					% shp_hdr->lock.lock_time
					% shp_hdr->lock.lock_time).str(SGLOCALE) << "\n";
		cout <<(_("                MAGIC:  0x")).str(SGLOCALE);
		cout << std::setbase(16) << shp_hdr->magic << std::setbase(10) <<"\n";
		cout <<(_("            REF COUNT:  {1}") % shp_hdr->ref_count).str(SGLOCALE) << "\n";
		cout <<(_("           TOTAL SIZE:  {1}") % shp_hdr->total_size).str(SGLOCALE) << "\n";
		cout <<(_("            HASH SIZE:  {1}") % shp_hdr->hash_size).str(SGLOCALE) << "\n";
		cout <<(_("            DATA SIZE:  {1}") % shp_hdr->data_size).str(SGLOCALE) << "\n";
		cout <<(_("             DATA LEN:  {1}") % shp_hdr->data_len).str(SGLOCALE) << "\n";
		cout <<(_("            FREE LINK:  {1}") % shp_hdr->free_link).str(SGLOCALE) << "\n";

	} catch (exception& ex) {
		if (GPT->_GPT_error_code == GPEINVAL)
			cerr << (_("ERROR: Component key provided doesn't exist.")).str(SGLOCALE);
		else
			cerr << ex.what();
		return SHMCERR;
	}

	return 0;
}

int shm_admin::shmapk()
{
	if (c_com_key.empty()) {
		cerr << (_("ERROR: Componenet key is null.")).str(SGLOCALE);
		return SHMCERR;
	}

	try {
		shm_pair shp(mgr, c_com_key);
		shp_hdr_t *shp_hdr = shp.shp_hdr;
		int *hash_table = shp.hash_table;

	cout <<(_("Shared Memory Operation Data Information:")).str(SGLOCALE) << std::endl;
	cout << "-----------------------------------\n";

		// From now on, no exception will be throw, so we don't need to unlock() on exception.
		shp.lock();

		for (int i = 0; i < shp_hdr->hash_size; i++) {
			int shp_index = hash_table[i];
			if (shp_index == -1)
				continue;

			shp_data_t *shp_data = shp.at(shp_index);
			while (1) {
				cout <<(_("                FLAGS:  0x")).str(SGLOCALE);
				cout << std::setbase(16) << shp_data->flags << std::setbase(10) << "\n";
				cout <<(_("            REF COUNT:  {1}") % shp_data->ref_count).str(SGLOCALE) << "\n";
				cout <<(_("                 PREV:  {1}") % shp_data->prev).str(SGLOCALE) << "\n";
				cout <<(_("                 NEXT:  {1}") % shp_data->next).str(SGLOCALE) << "\n";
				cout <<(_("             KEY NAME:  {1}") % shp_data->key_name).str(SGLOCALE) << "\n";
				cout << "-----------------------------------\n";

				if (shp_data->next == -1)
					break;

				shp_data = shp.at(shp_data->next);
			}
		}

		shp.unlock();
		return 0;
	} catch (exception& ex) {
		if (GPT->_GPT_error_code == GPEINVAL)
			cerr << (_("ERROR: COM_KEY provided doesn't exist.")).str(SGLOCALE);
		else
			cerr << ex.what();
		return SHMCERR;
	}

	return 0;
}

int shm_admin::shmapd()
{
	if (c_com_key.empty() || !c_Sopt)
		return SHMSYNERR;

	try {
		shm_pair shp(mgr, c_com_key);
		shp_data_t *shp_data = shp.find(c_svc_key);

		if (shp_data == NULL) {
			cerr << (_("INFO: Can't find entry for Operation Key.")).str(SGLOCALE);
			return SHMCERR;
		}

		cout <<(_("Shared Memory Operation Data Information:")).str(SGLOCALE) << std::endl;
		cout <<(_("-----------------------------------")).str(SGLOCALE) << std::endl;
		cout <<(_("                FLAGS:  0x")).str(SGLOCALE);
		cout << std::setbase(16) << shp_data->flags << std::setbase(10) << "\n";
		cout <<(_("            REF COUNT:  {1}") % shp_data->ref_count).str(SGLOCALE) << "\n";
		cout <<(_("                 PREV:  {1}") % shp_data->prev).str(SGLOCALE) << "\n";
		cout <<(_("                 NEXT:  {1}") % shp_data->next).str(SGLOCALE) << "\n";
		cout <<(_("             KEY NAME:  {1}") % shp_data->key_name).str(SGLOCALE) << "\n";
		cout <<(_("                 DATA:  ")).str(SGLOCALE) << std::endl;

		unsigned char *start_addr = reinterpret_cast<unsigned char *>(&shp_data->data);
		int size;
		if (c_sopt)
			size = c_size;
		else
			size = shp.shp_hdr->data_size;

		for (int i = 0; i < size; i++) {
			if (i % 16 == 0)
				cout << "\n0x" << std::setbase(16) << (unsigned long)(start_addr + i) << ' ';

			if (hex) {
				cout << std::setfill('0') << std::setw(2) << std::setbase(16)
					<< static_cast<int>(start_addr[i]) << ' ';
			} else {
				cout << std::setbase(10) << static_cast<int>(start_addr[i]) << ' ';
			}
		}

		cout << '\n' << std::setbase(10);
		shp.erase(shp_data);
	} catch (exception& ex) {
		if (GPT->_GPT_error_code == GPEINVAL)
			cerr << (_("ERROR: Component key provided doesn't exist.")).str(SGLOCALE);
		else
			cerr << ex.what();
		return SHMCERR;
	}

	return 0;
}

int shm_admin::shmaid()
{
	if (!c_copt || !c_Sopt)
		return SHMSYNERR;

	try {
		shm_pair shp(mgr, c_com_key);
		shp_data_t *shp_data = shp.find(c_svc_key);
		if (shp_data == NULL) {
			cerr << (_("ERROR: Operation key provided doesn't exist.")).str(SGLOCALE);
			return SHMCERR;
		}
		shp_data->invalidate(&shp);

		cout <<(_("INFO: Operation key invalidated successfully.")).str(SGLOCALE) << std::endl;
	} catch (exception& ex) {
		if (GPT->_GPT_error_code == GPEINVAL)
			cerr << (_("ERROR: Invalid parameter provided.")).str(SGLOCALE);
		else
			cerr << ex.what();
		return SHMCERR;
	}

	return 0;
}

int shm_admin::shmal()
{
	if (!c_topt)
		return SHMSYNERR;

	shm_malloc_t *shm_hdr = mgr.shm_hdr;

	switch (c_type) {
	case SHM_LOCKTYPE_SYS:
		shm_hdr->syslock.lock();
		return 0;
	case SHM_LOCKTYPE_FREE:
		if (!c_iopt)
			return SHMSYNERR;
		if (c_block_index < 0 || c_block_index > SHM_BUCKETS) {
			cerr << (_("ERROR: Block index out of range")).str(SGLOCALE);
			return SHMCERR;
		}
		shm_hdr->free_lock[c_block_index].lock();
		return 0;
	case SHM_LOCKTYPE_USED:
		if (!c_iopt)
			return SHMSYNERR;
		shm_hdr->used_lock[c_block_index].lock();
		return 0;
	default:
		cerr << (_("ERROR: Lock type not corrrect")).str(SGLOCALE);
		return SHMCERR;
	}
}

int shm_admin::shmaul()
{
	if (!c_topt)
		return SHMSYNERR;

	shm_malloc_t *shm_hdr = mgr.shm_hdr;

	switch (c_type) {
	case SHM_LOCKTYPE_SYS:
		shm_hdr->syslock.unlock();
		return 0;
	case SHM_LOCKTYPE_FREE:
		if (!c_iopt)
			return SHMSYNERR;
		if (c_block_index < 0 || c_block_index > SHM_BUCKETS) {
			cerr << (_("ERROR: Block index out of range")).str(SGLOCALE);
			return SHMCERR;
		}
		shm_hdr->free_lock[c_block_index].unlock();
		return 0;
	case SHM_LOCKTYPE_USED:
		if (!c_iopt)
			return SHMSYNERR;
		shm_hdr->used_lock[c_block_index].unlock();
		return 0;
	default:
		cerr << (_("ERROR: Lock type not corrrect")).str(SGLOCALE);
		return SHMCERR;
	}

}

int shm_admin::shmad()
{
	if (!c_sopt)
		return SHMSYNERR;

	int i;
	shm_link_t shm_link;
	shm_data_t *shm_data;
	if (c_aopt) {
		shm_data = (shm_data_t *)(c_address - sizeof(shm_data_t) + sizeof(void *));
		if (mgr.get_shm_link(shm_data) == shm_link_t::SHM_LINK_NULL) {
			cerr << (_("ERROR: Address provided is not correct.")).str(SGLOCALE);
			return SHMCERR;
		}
	} else if (c_iopt && c_Oopt) {
		shm_link.block_index = c_block_index;
		shm_link.block_offset = c_block_offset;
		shm_data = mgr.get_shm_data(shm_link);
		if (shm_data == NULL) {
			cerr << (_("ERROR: Address provided is not correct.")).str(SGLOCALE);
			return SHMCERR;
		}
	} else {
		return SHMSYNERR;
	}

	unsigned char *start_addr = reinterpret_cast<unsigned char *>(shm_data);

	if (c_oopt) {
		size_t data_size = (SHM_MIN_SIZE << shm_data->bucket_index);
		if (data_size < c_offset + c_size) {
			cerr << (_("ERROR: offset out of bound")).str(SGLOCALE);
			return SHMCERR;
		}
		start_addr += c_offset;
	}

	for (i = 0; i < c_size; i++) {
		if (i % 16 == 0)
			cout << "\n0x" << std::setbase(16) << (unsigned long)(start_addr + i) << ' ';

		if (hex) {
			cout << std::setfill('0') << std::setw(2) << std::setbase(16)
				<< static_cast<int>(start_addr[i]) << ' ';
		} else {
			cout << std::setbase(10) << static_cast<int>(start_addr[i]) << ' ';
		}
	}

	cout << '\n' << std::setbase(10);
	return 0;
}

int shm_admin::shmac()
{
	shm_malloc_t *shm_hdr = mgr.shm_hdr;

	// check syslock
	if (shm_hdr->syslock.spinlock)
		clean_lock(shm_hdr->syslock);

	for (int i = 0; i < SHM_BUCKETS; i++) {
		if (shm_hdr->free_lock[i].spinlock)
			clean_lock(shm_hdr->free_lock[i]);
		if (shm_hdr->used_lock[i].spinlock)
			clean_lock(shm_hdr->used_lock[i]);
	}
	return 0;
}

int shm_admin::shmarm()
{
	shm_malloc_t *shm_hdr = mgr.shm_hdr;
	shm_hdr->syslock.lock();
	shm_hdr->flags = 0;
	for (int i = 1; i < shm_hdr->shm_count; i++)
		::shmctl(shm_hdr->shm_ids[i], IPC_RMID, NULL);
	shm_hdr->syslock.unlock();
	::shmctl(shm_hdr->shm_ids[0], IPC_RMID, NULL);
	return SHMQUIT;
}

int shm_admin::scan(const string& s)
{
	/*
	 * This routine parses the command line (s) and forms the array cmdargs.
	 * It also removes leading AND trailing white space, checks length
	 * of arguments, etc. Each element returned in the cmdargs array is of
	 * the form "-oarg", where o is the option (such as g) and
	 * arg is the argument (such as 400). For those arguments which
	 * don't have option letters, such as "printqueue 3487", only
	 * the argument itself is returned.
	 */
	int c = 0, i, pos;

	numargs = pos = 0;

	for (i = 0; i < s.length() && isspace(s[i]); ++i)
		;

	// don't test this if no leading white space (since i-1 = -1 !!)
	if (i > 0 && s[i - 1] == '\n')
		return 0;

	/*
	 * If a shell command or a comment is entered, we don't want to
	 * parse it, so return immediately.
	 */
	if (s[i] == '!' || s[i] == '#') {
		numargs++;
		cmdline = s.substr(i);
		return -1;
	}

	/*
	 * Since the boot, shutdown, and config commands are passed off to
	 * tmboot, tmshutdown, and tmconfig respectively via system, we don't
	 * want to parse the command line.
	 */
	int optflag = 0;

	if (optflag) {
		numargs++;
		cmdline = s.substr(i);
		return 0;
	}

	optflag = 0;

	while (1) {
		// remove leading white space
		while (isspace(s[i]) && s[i] != '\n')
			i++;

		bool quoteflag = false; // not in a quoted string
		while (i < s.length()) {
			c = s[i++];
			if (isspace(c) && !quoteflag) //indicates end of current element
				break;

			/*
			 * optflag == 0 means that we haven't encountered
			 * a "-" yet.
			 * optflag == 1 means that we have encountered a "-".
			 * optflag == 2 means that we have an option for "-",
			 * such as "-g".
			 * optflag > 2 means that we have an option and
			 * argument, such as "-g 400".
			 */
			if (optflag != 0 && c != '-' && !quoteflag)
				optflag++;

			if (!quoteflag && c == '"')
				quoteflag = true;
			else if (quoteflag && c == '"')
				break;

			if (c == '-' && !quoteflag) {
				if (optflag == 0) {
					optflag++; // leading "-"
				} else if (optflag < 3) { // just "-" or "-g"
					/*
					 * Note that for an embedded "-", such as
					 * "-s enzo-greco", optflag is = 2
					 */
					return SHMSYNERR;
				}
			}

			if (pos >= ARGLEN)
				return SHMLONGARG; // argument too long

			if (numargs >= MAXARGS)
				return SHMARGCNT; // too many arguments

			if (c != '"' || !quoteflag) // eliminate quotes in string
				cmdargs[numargs][pos++] = c; // assign the char
		}

		/*
		 * At this point, if optflag == 2, we just have the option,
		 * "-g", for example, so we need to go through and get the
		 * argument now.
		 */

		// if just "-" or "-g", for example
		if (optflag == 2 && (cmdargs[numargs][1] == 'v' || cmdargs[numargs][1] == 't')) {
			// -v or -t option - no argument
			optflag = 3;
		} else if (optflag == 1 || ((optflag == 2 || quoteflag) && c == '\n')) {
			return SHMSYNERR;
		}

		if (optflag == 0 && pos == 0)
			break; // for white space at end of line

		// a valid element, so assign it and prepare for the next one
		if (optflag == 0 || optflag > 2) {
			cmdargs[numargs] [pos] = '\0';
			optflag = 0;
			numargs++;
			pos = 0;
		}

		if (c == '\n')
			break;
	}

	return 0;
}

int shm_admin::off_on(const char *name, bool& flag)
{
	/* Set the option "name" (eg, verbose) */
	if (numargs > 2) {
		return SHMSYNERR;
	}

	if (c_value == "off") // Turn off
		flag = FALSEOPT;
	else if (c_value =="on") // Turn on
		flag = TRUEOPT;
	else if (numargs == 1) // Toggle
		flag = !flag;
	else
		return SHMSYNERR;

	if (flag == TRUEOPT)
		cout <<(_("{1} now on.") % name).str(SGLOCALE) << std::endl;
	else
		cout <<(_("{1} now off.") % name).str(SGLOCALE) << std::endl;

	return 0;
}

bool shm_admin::validcmd(const cmds_t *ct)
{
	return true;
}

int shm_admin::determ()
{
	string temp;
	int cur; // current element in the cmdargs array
	int i;

	c_sopt = c_aopt = c_iopt = c_Oopt = c_oopt = c_topt = c_yopt = c_vopt
		= c_hopt = c_dopt = c_lopt = c_copt = c_Sopt = 0;

	// Note that cmdargs[0] is the command, so we ignore that element
	for (cur = 1; cur < numargs; cur++) {
		if (cmdargs[cur][0] == '-' && !isdigit(cmdargs[cur][1])) {
			// an option element
			temp = cmdargs[cur] + 2; // store the argument
			switch (cmdargs[cur] [1]) { // now switch on option
			case 's': // size
				if (c_sopt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_sopt = OFFOPT;
				} else {
					for (i = 0; i < temp.length(); i++) {
						if (!isdigit(temp[i]))
							break;
					}
					if (i == 0 || i != temp.length()) {
						cerr << (_("ERROR: -s option: argument must be numeric\n")).str(SGLOCALE);
						return SHMBADOPT;
					}
					c_size = ::atoi(temp.c_str());
					c_sopt = TRUEOPT;
				}
				break;
			case 'a': // address
				if (c_aopt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_aopt = OFFOPT;
				} else {
					if (temp.length() > 2) {
						if (temp[0] != '0' || (temp[1] != 'x' && temp[1] != 'X')) {
							cerr << (_("ERROR: -a option: argument must be hex numeric\n")).str(SGLOCALE);
							return SHMBADOPT;
						}
					}

					for (i = 2; i < temp.length(); i++) {
						if (!isxdigit(temp[i]))
							break;
					}
					if (i == 2 || i != temp.length()) {
						cerr << (_("ERROR: -a option: argument must be hex numeric\n")).str(SGLOCALE);
						return SHMBADOPT;
					}
					c_address = ::strtol(temp.c_str(), NULL, 16);
					c_aopt = TRUEOPT;
				}
				break;
			case 'i': // block_index
				if (c_iopt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_iopt = OFFOPT;
				} else {
					for (i = 0; i < temp.length(); i++) {
						if (!isdigit(temp[i]))
							break;
					}
					if (i == 0 || i != temp.length()) {
						cerr << (_("ERROR: -i option: argument must be numeric\n")).str(SGLOCALE);
						return SHMBADOPT;
					}
					c_block_index = ::atoi(temp.c_str());
					c_iopt = TRUEOPT;
				}
				break;
			case 'O': // block_offset
				if (c_Oopt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_Oopt = OFFOPT;
				} else {
					for (i = 0; i < temp.length(); i++) {
						if (!isdigit(temp[i]))
							break;
					}
					if (i == 0 || i != temp.length()) {
						cerr << (_("ERROR: -O option: argument must be numeric\n")).str(SGLOCALE);
						return SHMBADOPT;
					}
					c_block_offset = ::atoi(temp.c_str());
					c_Oopt = TRUEOPT;
				}
				break;
			case 'o': // offset
				if (c_oopt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_oopt = OFFOPT;
				} else {
					for (i = 0; i < temp.length(); i++) {
						if (!isdigit(temp[i]))
							break;
					}
					if (i == 0 || i != temp.length()) {
						cerr << (_("ERROR: -o option: argument must be numeric\n")).str(SGLOCALE);
						return SHMBADOPT;
					}
					c_offset = ::atoi(temp.c_str());
					c_oopt = TRUEOPT;
				}
				break;
			case 'T': // type
				if (c_topt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_topt = OFFOPT;
				} else {
					if (temp == "sys") {
						c_type = SHM_LOCKTYPE_SYS;
					} else if (temp == "free") {
						c_type = SHM_LOCKTYPE_FREE;
					} else if (temp == "used") {
						c_type = SHM_LOCKTYPE_USED;
					} else {
						cerr << (_("ERROR: -T option: argument must be sys|free|used\n")).str(SGLOCALE);
						return SHMBADOPT;
					}
					c_topt = TRUEOPT;
				}
			case 'h': // hash size
				if (c_hopt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_hopt = OFFOPT;
				} else {
					for (i = 0; i < temp.length(); i++) {
						if (!isdigit(temp[i]))
							break;
					}
					if (i == 0 || i != temp.length()) {
						cerr << (_("ERROR: -h option: argument must be numeric\n")).str(SGLOCALE);
						return SHMBADOPT;
					}
					c_hash_size = ::atoi(temp.c_str());
					c_hopt = TRUEOPT;
				}
				break;
			case 'd': // data size
				if (c_dopt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_dopt = OFFOPT;
				} else {
					for (i = 0; i < temp.length(); i++) {
						if (!isdigit(temp[i]))
							break;
					}
					if (i == 0 || i != temp.length()) {
						cerr << (_("ERROR: -d option: argument must be numeric\n")).str(SGLOCALE);
						return SHMBADOPT;
					}
					c_data_size = ::atoi(temp.c_str());
					c_dopt = TRUEOPT;
				}
				break;
			case 'l': // data len
				if (c_lopt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_lopt = OFFOPT;
				} else {
					for (i = 0; i < temp.length(); i++) {
						if (!isdigit(temp[i]))
							break;
					}
					if (i == 0 || i != temp.length()) {
						cerr << (_("ERROR: -d option: argument must be numeric\n")).str(SGLOCALE);
						return SHMBADOPT;
					}
					c_data_len = ::atoi(temp.c_str());
					c_lopt = TRUEOPT;
				}
				break;
			case 'c': // com key
				if (c_copt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_com_key = "";
					c_copt = OFFOPT;
				} else {
					c_com_key = temp;
					c_copt = TRUEOPT;
				}
				break;
			case 'S': // service key
				if (c_Sopt)
					return SHMDUPARGS;

				if (temp == "*") {
					c_svc_key = "";
					c_Sopt = OFFOPT;
				} else {
					c_svc_key = temp;
					c_Sopt = TRUEOPT;
				}
				break;
			case 'y':
				if (c_yopt)
					return SHMDUPARGS;
				if (temp == "*")
					c_yopt = OFFOPT;
				else
					c_yopt = TRUEOPT;
				break;
			case 'v':
				verbose = TRUEOPT;
				break;
			case 't':
				verbose = FALSEOPT;
				break;
			default:
				return SHMBADOPT; // unknown option
			}
		} else { // value argument (no option)
			if (strcmp(cmdargs[cur], "*") == 0) {
				c_vopt = OFFOPT;
			} else {
				c_value = cmdargs[cur];
				c_vopt = TRUEOPT;
			}
		}
	}

	return 0;
}

void shm_admin::clean_lock(spinlock_t& lock)
{
	pid_t pid = lock.lock_pid;
	if (pid == 0) { // This can happen when just locked.
		if (lock.lock_time + 60 > ::time(0)) {
			lock.unlock();
			GPP->write_log(__FILE__, __LINE__,(_("ERROR: Process got spinlock but died, clean lock")).str(SGLOCALE) );
		}
	} else if (::kill(pid, 0) == -1) { // died, so clean it
		// Make sure pid is not changed, so we can clear spinlock safely.
		if (pid == lock.lock_pid) {
			lock.lock_pid = 0;
			lock.unlock();
			GPP->write_log(__FILE__, __LINE__,(_("ERROR: Process got spinlock but died, clean lock")).str(SGLOCALE) );
		}
	}
}

void shm_admin::intr(int sig)
{
	gotintr++;
}

const char *shm_admin::shm_emsgs(int err_num)
{
	static const char *emsgs[] = {
		"ERROR: Custom Error Message.",
		"ERROR: Argument too long.",
		"ERROR: No such command.",
		"ERROR: Invalid option.",
		"ERROR: Too many arguments.",
		"ERROR: Duplicate arguments.",
		"ERROR: Syntax error on command line.",
	};

	return emsgs[err_num - SHMCERR];
}

}
}

