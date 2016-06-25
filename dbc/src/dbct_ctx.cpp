#include "dbc_internal.h"

namespace ai
{
namespace sg
{

boost::thread_specific_ptr<dbct_ctx> dbct_ctx::instance_;

dbct_ctx::dbct_ctx()
{
	_DBCT_bbp = NULL;
	_DBCT_ues = NULL;
	_DBCT_tes = NULL;
	_DBCT_ses = NULL;
	_DBCT_rtes = NULL;
	_DBCT_redo = NULL;

	_DBCT_login = false;
	_DBCT_user_id = -1;
	_DBCT_rowids = &_DBCT_rowids_buf;
	_DBCT_rownum = std::numeric_limits<long>::max();
	_DBCT_find_flags = 0;

	_DBCT_data_start = NULL;
	_DBCT_dbc_switch = NULL;

	_DBCT_data_buf = NULL;
	_DBCT_data_buf_size = 0;
	_DBCT_row_buf = NULL;
	_DBCT_row_buf_size = 0;

	_DBCT_update_func = NULL;
	_DBCT_update_func_internal = NULL;
	_DBCT_where_func = NULL;
	_DBCT_set_binds = NULL;
	_DBCT_set_bind_count = 0;
	_DBCT_input_binds = NULL;
	_DBCT_input_bind_count = 0;
	_DBCT_redo_sequence = 0;
	_DBCT_seq_fd = -2;
	_DBCT_atexit_registered = false;

	_DBCT_group_func = NULL;
	_DBCT_internal_format = false;
	_DBCT_internal_size = 0;

	_DBCT_skip_marshal = false;
	_DBCT_insert_size = 0;
}

dbct_ctx::~dbct_ctx()
{
	if (_DBCT_seq_fd >= 0)
		close(_DBCT_seq_fd);

	delete []_DBCT_data_buf;
	delete []_DBCT_row_buf;
	delete []_DBCT_input_binds;
	delete []_DBCT_set_binds;
}

dbct_ctx * dbct_ctx::instance()
{
	dbct_ctx *DBCT = instance_.get();
	if (DBCT == NULL) {
		DBCT = new dbct_ctx();
		instance_.reset(DBCT);

		// 创建管理对象数组
		DBCT->_DBCT_mgr_array[DBC_UE_MANAGER] = new dbc_ue();
		DBCT->_DBCT_mgr_array[DBC_TE_MANAGER] = new dbc_te();
		DBCT->_DBCT_mgr_array[DBC_IE_MANAGER] = new dbc_ie();
		DBCT->_DBCT_mgr_array[DBC_SE_MANAGER] = new dbc_se();
		DBCT->_DBCT_mgr_array[DBC_RTE_MANAGER] = new dbc_rte();
		DBCT->_DBCT_mgr_array[DBC_REDO_MANAGER] = new dbc_redo();
		DBCT->_DBCT_mgr_array[DBC_REDO_TREE_MANAGER] = new redo_rbtree();
		DBCT->_DBCT_mgr_array[DBC_INODE_TREE_MANAGER] = new inode_rbtree();
		DBCT->_DBCT_mgr_array[DBC_INODE_LIST_MANAGER] = new inode_list();
		DBCT->_DBCT_mgr_array[DBC_SQLITE_MANAGER] = new dbc_sqlite();
		DBCT->_DBCT_mgr_array[DBC_API_MANAGER] = new dbc_api();
		DBCT->_DBCT_mgr_array[SDC_CONFIG_MANAGER] = new sdc_config();
	}
	return DBCT;
}

bool dbct_ctx::create_shm()
{
	gpp_ctx *GPP = gpp_ctx::instance();
	dbcp_ctx *DBCP = dbcp_ctx::instance();
	dbc_config& config_mgr = dbc_config::instance();
	dbc_bbparms_t& config_bbparms = config_mgr.get_bbparms();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

	if (DBCP->_DBCP_attached_clients > 0)
		detach_shm(false);

	int i;
	int shmid = ::shmget(config_bbparms.ipckey, 0, config_bbparms.perm);
	if (shmid != -1) {
		dbc_bboard_t *bbp = reinterpret_cast<dbc_bboard_t *>(::shmat(shmid, NULL, config_bbparms.perm));
		if (bbp) {
			// 如果可能，删除共享内存
			for (i = 1; i < bbp->bbparms.shm_count; i++)
				::shmctl(bbp->bbmap.shmids[i], IPC_RMID, 0);

			::shmdt(bbp);
		}

		if (::shmctl(shmid, IPC_RMID, 0) == -1) {
			_DBCT_error_code = DBCEOS;
			_DBCT_native_code = USHMCTL;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmctl failed - {1}") % ::strerror(errno)).str(SGLOCALE));
			return retval;
		}
	}

	vector<int> shmids;
	key_t ipckey = config_bbparms.ipckey;
	size_t total_size = config_bbparms.total_size;

	while (1) {
		size_t shm_size = std::min(total_size, static_cast<size_t>(config_bbparms.shm_size));
		shmid = ::shmget(ipckey, shm_size, IPC_CREAT | config_bbparms.perm);
		if (shmid == -1) {
			_DBCT_error_code = DBCEOS;
			_DBCT_native_code = USHMGET;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmget failed, shm_size = {1}, error message - {2}") % shm_size % ::strerror(errno)).str(SGLOCALE));
			return retval;
		}

		shmids.push_back(shmid);

		if (total_size <= config_bbparms.shm_size)
			break;

		total_size -= config_bbparms.shm_size;
		ipckey = IPC_PRIVATE;
	}

	if (!reserve_addr(config_bbparms))
		return retval;

	_DBCT_bbp = reinterpret_cast<dbc_bboard_t *>(DBCP->_DBCP_attach_addr);

	void *shm_addr = DBCP->_DBCP_attach_addr;
	for (i = 0; i < shmids.size(); i++) {
		if (::shmat(shmids[i], shm_addr, config_bbparms.perm) != shm_addr) {
			_DBCT_error_code = DBCEOS;
			_DBCT_native_code = USHMAT;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmat failed - {1} ") % ::strerror(errno)).str(SGLOCALE));
			return retval;
		}

		shm_addr = reinterpret_cast<void *>(reinterpret_cast<char *>(shm_addr) + config_bbparms.shm_size);
		_DBCT_bbp->bbmap.shmids[i] = shmids[i];
	}

	// 初始化其它全局字段
	_DBCT_bbp->init(config_bbparms);
	DBCP->_DBCP_attached_count = _DBCT_bbp->bbparms.shm_count;
	DBCP->_DBCP_attached_clients++;
	retval = true;
	return retval;
}

bool dbct_ctx::extend_shm(int add_count)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	dbcp_ctx *DBCP = dbcp_ctx::instance();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("add_count={1}") % add_count).str(SGLOCALE), &retval);
#endif

	if (!DBCP->_DBCP_is_server && !DBCP->_DBCP_is_admin) {
		_DBCT_error_code = DBCEPERM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Function can only be called by dbc_server or dbc_admin.")).str(SGLOCALE));
		return retval;
	}

	dbc_bbparms_t& bbparms = _DBCT_bbp->bbparms;
	dbc_bbmeters_t& bbmeters = _DBCT_bbp->bbmeters;
	dbc_bbmap_t& bbmap = _DBCT_bbp->bbmap;
	bi::scoped_lock<bi::interprocess_recursive_mutex> slock(_DBCT_bbp->syslock);
	scoped_usignal sig;

	if (add_count <= 0) {
		_DBCT_error_code = DBCEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: invalid argument given")).str(SGLOCALE));
		return retval;
	}

	if (bbparms.shm_count + add_count > bbparms.reserve_count) {
		_DBCT_error_code = DBCEINVAL;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: shm count exceeds {1}") % bbparms.reserve_count).str(SGLOCALE));
		return retval;
	}

	void *shm_addr = reinterpret_cast<void *>(reinterpret_cast<char *>(DBCP->_DBCP_attach_addr) + bbparms.total_size);
	long total_size = bbparms.shm_size * add_count;
	int i;
	int j;

	for (i = bbparms.shm_count; i < bbparms.shm_count + add_count; i++) {
		bbmap.shmids[i] = ::shmget(IPC_PRIVATE, bbparms.shm_size, IPC_CREAT | bbparms.perm);
		if (bbmap.shmids[i] == -1) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmget failed - {1}") % ::strerror(errno)).str(SGLOCALE));

			for (j = bbparms.shm_count; j < i; j++)
				::shmctl(bbmap.shmids[j], IPC_RMID, 0);

			_DBCT_error_code = DBCEOS;
			_DBCT_native_code = USHMGET;
			return retval;
		}
	}

	if (munmap(shm_addr, total_size) == -1) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: munmap failed - {1}") % ::strerror(errno)).str(SGLOCALE));
		_DBCT_error_code = DBCEOS;
		return retval;
	}

	for (i = bbparms.shm_count; i < bbparms.shm_count + add_count; i++) {
		if (::shmat(bbmap.shmids[i], shm_addr, bbparms.perm) != shm_addr) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmat failed - {1}") % ::strerror(errno)).str(SGLOCALE));

			for (j = bbparms.shm_count; j < bbparms.shm_count + add_count; j++)
				::shmctl(bbmap.shmids[j], IPC_RMID, 0);

			_DBCT_error_code = DBCEOS;
			_DBCT_native_code = USHMAT;
			return retval;
		}

		shm_addr = reinterpret_cast<void *>(reinterpret_cast<char *>(shm_addr) + bbparms.shm_size);
	}

	bbmeters.additional_count = add_count;

	// Mark all RTEs and wait for all clients to reattach.
	dbc_rte& rte_mgr = dbc_rte::instance(this);
	dbc_rte_t *currte = rte_mgr.get_rte();
	dbc_rte_t *rte;
	for (rte = _DBCT_rtes; rte < _DBCT_rtes + bbparms.maxrtes; rte++) {
		if (!rte->in_flags(RTE_IN_USE) || rte->in_flags(RTE_MARK_REMOVED) || rte == currte)
			continue;

		rte->set_flags(RTE_REATTACH);

		if (kill(rte->pid, bbparms.post_signo) == -1)
			rte->clear_flags(RTE_REATTACH);
	}

	while (1) {
		for (rte = _DBCT_rtes; rte < _DBCT_rtes + bbparms.maxrtes; rte++) {
			if (rte->in_flags(RTE_REATTACH)) {
				if (kill(rte->pid, bbparms.post_signo) != -1)
					break;

				rte->clear_flags(RTE_REATTACH);
			}
		}

		if (rte == _DBCT_rtes + bbparms.maxrtes)
			break;

		GPP->write_log((_("INFO: Waiting for all clients to reattach, sleep 5s.")).str(SGLOCALE));
		sleep(5);
	}

	// Add to free list.
	dbc_mem_block_t *mem_block = _DBCT_bbp->long2block(bbparms.total_size);
	mem_block->magic = DBC_MEM_MAGIC;
	mem_block->flags = MEM_IN_USE;
	mem_block->size = total_size - sizeof(dbc_mem_block_t);
	mem_block->prev = -1;
	mem_block->next = -1;
	_DBCT_bbp->free(bbparms.total_size);

	// Adjust shm variables
	bbparms.shm_count += add_count;
	bbparms.total_size += total_size;
	bbmeters.additional_count = 0;
	DBCP->_DBCP_attached_count = bbparms.shm_count;
	retval = true;
	return retval;
}

bool dbct_ctx::attach_shm()
{
	gpp_ctx *GPP = gpp_ctx::instance();
	dbcp_ctx *DBCP = dbcp_ctx::instance();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif
	bi::scoped_lock<boost::mutex> lock(DBCP->_DBCP_shm_mutex);

	if (DBCP->_DBCP_attach_addr != NULL) {
		++DBCP->_DBCP_attached_clients;
		_DBCT_bbp = reinterpret_cast<dbc_bboard_t *>(DBCP->_DBCP_attach_addr);
		retval = true;
		return retval;
	}

	// 打开配置文件
	dbc_config& config_mgr = dbc_config::instance();
	if (!config_mgr.open())
		return retval;

	dbc_bbparms_t& config_bbparms = config_mgr.get_bbparms();
	int shmid = ::shmget(config_bbparms.ipckey, 0, config_bbparms.perm);
	if (shmid == -1) {
		_DBCT_error_code = DBCEOS;
		_DBCT_native_code = USHMGET;
		if (!DBCP->_DBCP_is_server)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmget failed - {1}") % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	_DBCT_bbp = reinterpret_cast<dbc_bboard_t *>(::shmat(shmid, NULL, config_bbparms.perm));
	if (_DBCT_bbp == reinterpret_cast<dbc_bboard_t *>(-1)) {
		_DBCT_error_code = DBCEOS;
		_DBCT_native_code = USHMAT;
		if (!DBCP->_DBCP_is_server)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmat failed - {1}") % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	// Wait for server initialization.
	int try_count = 10;
	while (_DBCT_bbp->bbparms.magic != DBC_MAGIC) {
		if (--try_count) {
			sleep(1);
			continue;
		}
		_DBCT_error_code = DBCEBBINSANE;
		if (!DBCP->_DBCP_is_server)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shared memory is not available or ipckey given is not correct")).str(SGLOCALE));
		return retval;
	}

	if (config_bbparms.total_size != _DBCT_bbp->bbparms.total_size
		|| config_bbparms.reserve_count != _DBCT_bbp->bbparms.reserve_count
		|| config_bbparms.shm_size != _DBCT_bbp->bbparms.shm_size) {
		_DBCT_error_code = DBCEBBINSANE;
		if (!DBCP->_DBCP_is_server)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shared memory not sane")).str(SGLOCALE));
		return retval;
	}

	if (::shmdt(_DBCT_bbp) == -1) {
		_DBCT_error_code = DBCEOS;
		_DBCT_native_code = USHMDT;
		if (!DBCP->_DBCP_is_server)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmdt failed - {1}") % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	if (!reserve_addr(config_bbparms))
		return retval;

	void *shm_addr = DBCP->_DBCP_attach_addr;
	if (::shmat(shmid, shm_addr, config_bbparms.perm) != shm_addr) {
		_DBCT_error_code = DBCEOS;
		_DBCT_native_code = USHMAT;
		if (!DBCP->_DBCP_is_server)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmat failed - {1}") % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	_DBCT_bbp = reinterpret_cast<dbc_bboard_t *>(shm_addr);
	dbc_bbparms_t& bbparms = _DBCT_bbp->bbparms;
	dbc_bbmap_t& bbmap = _DBCT_bbp->bbmap;

	for (int i = 1; i < bbparms.shm_count; i++) {
		shm_addr = reinterpret_cast<void *>(reinterpret_cast<char *>(shm_addr) + bbparms.shm_size);
		if (::shmat(bbmap.shmids[i], shm_addr, bbparms.perm) != shm_addr) {
			_DBCT_error_code = DBCEOS;
			_DBCT_native_code = USHMAT;
			if (!DBCP->_DBCP_is_server)
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmat failed - {1}") % ::strerror(errno)).str(SGLOCALE));
			return retval;
		}
	}

	DBCP->_DBCP_attached_count = bbparms.shm_count;
	++DBCP->_DBCP_attached_clients;
	retval = true;
	return retval;
}

void dbct_ctx::detach_shm(bool auto_close)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	dbcp_ctx *DBCP = dbcp_ctx::instance();
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, (_("auto_close={1}") % auto_close).str(SGLOCALE), NULL);
#endif
	bi::scoped_lock<boost::mutex> lock(DBCP->_DBCP_shm_mutex);

	if (DBCP->_DBCP_attach_addr == NULL)
		return;

	if (--DBCP->_DBCP_attached_clients > 0)
		return;

	long shm_size = _DBCT_bbp->bbparms.shm_size;
	long reserved_size = shm_size * (_DBCT_bbp->bbparms.reserve_count - DBCP->_DBCP_attached_count);
	void *shm_addr = DBCP->_DBCP_attach_addr;

	for (int i = 0; i < DBCP->_DBCP_attached_count; i++) {
		if (::shmdt(shm_addr) == -1)
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmdt failed - {1}") % ::strerror(errno)).str(SGLOCALE));
		shm_addr = reinterpret_cast<void *>(reinterpret_cast<char *>(shm_addr) + shm_size);
	}

	if (reserved_size > 0 && munmap(shm_addr, reserved_size) == -1) {
		_DBCT_error_code = DBCESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: munmap failed - {1}") % ::strerror(errno)).str(SGLOCALE));
	}

	DBCP->_DBCP_attached_count = 0;
	DBCP->_DBCP_attach_addr = NULL;

	// 关闭配置文件
	if (auto_close) {
		dbc_config& config_mgr = dbc_config::instance();
		config_mgr.close();
	}

	// 重置全局变量
	DBCP->clear();
}

void dbct_ctx::init_globals()
{
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, "", NULL);
#endif

	dbc_bbparms_t& bbparms = _DBCT_bbp->bbparms;

	_DBCT_data_start = reinterpret_cast<char *>(_DBCT_bbp) + SYS_ROW_SIZE;
	_DBCT_ues = reinterpret_cast<dbc_ue_t *>(_DBCT_bbp + 1);
	_DBCT_tes = reinterpret_cast<dbc_te_t *>(_DBCT_ues + bbparms.maxusers);
	_DBCT_ses = reinterpret_cast<dbc_se_t *>(_DBCT_tes + bbparms.maxtes);
	_DBCT_rtes = reinterpret_cast<dbc_rte_t *>(_DBCT_ses + bbparms.maxses);
	_DBCT_redo = reinterpret_cast<dbc_redo_t *>(_DBCT_rtes + bbparms.maxrtes);
}

// 不区分大小写
int dbct_ctx::get_hash(const char *key, int buckets)
{
	int len = strlen(key);
	int value = static_cast<int>(boost::hash_range(key, key + len));

	return value % buckets;
}

dbc_index_switch_t * dbct_ctx::get_index_switches()
{
	return _DBCT_dbc_switch->index_switches;
}

bool dbct_ctx::changed(int table_id, long rowid)
{
	redo_rbtree& redo_tree_mgr = redo_rbtree::instance(this);
	redo_row_t redo_row;
	redo_row.user_idx = (_DBCT_user_id & UE_ID_MASK);
	redo_row.table_id = table_id;
	redo_row.rowid = rowid;

	if (redo_tree_mgr.search(redo_row) == -1)
		return false;
	else
		return true;
}

bool dbct_ctx::change_insert(int table_id, int row_size, int old_row_size, long rowid, long old_rowid)
{
	redo_rbtree& redo_tree_mgr = redo_rbtree::instance(this);
	redo_row_t redo_row;
	redo_row.user_idx = (_DBCT_user_id & UE_ID_MASK);
	redo_row.table_id = table_id;
	redo_row.row_size = row_size;
	redo_row.old_row_size = old_row_size;
	redo_row.rowid = rowid;
	redo_row.old_rowid = old_rowid;
	redo_row.sequence = _DBCT_redo_sequence++;

	if (redo_tree_mgr.insert(redo_row) == -1) // ERROR already set in insert().
		return false;
	else
		return true;
}

bool dbct_ctx::change_erase(int table_id, long rowid, long redo_sequence)
{
	redo_rbtree& redo_tree_mgr = redo_rbtree::instance(this);
	redo_row_t redo_row;
	redo_row.user_idx = (_DBCT_user_id & UE_ID_MASK);
	redo_row.table_id = table_id;
	redo_row.rowid = rowid;
	redo_row.sequence = redo_sequence;

	if (redo_tree_mgr.erase(redo_row) == -1)
		return false;
	else
 		return true;
}

void dbct_ctx::set_for_update()
{
	_DBCT_find_flags |= (FIND_FOR_UPDATE | FIND_FOR_WAIT);
}

void dbct_ctx::clear_for_update()
{
	_DBCT_find_flags &= ~(FIND_FOR_UPDATE | FIND_FOR_WAIT);
}

bool dbct_ctx::rows_enough() const
{
	return (_DBCT_rowids->size() >= _DBCT_rownum) && !(_DBCT_find_flags & FIND_FOR_UPDATE);
}

bool dbct_ctx::row_visible(const row_link_t *link)
{
	dbc_te& te_mgr = dbc_te::instance(this);
	dbc_te_t *te = te_mgr.get_te();
	long rowid = link2rowid(link);

	if (!(link->flags & (ROW_TRAN_INSERT | ROW_TRAN_DELETE))) {
		return true;
	} else {
		switch (link->flags & (ROW_TRAN_INSERT | ROW_TRAN_DELETE)) {
		case ROW_TRAN_INSERT:
			// 对于插入记录，如果需要等待，则对所有会话都可见；如果不需等待，则只对当前会话可见
			if (_DBCT_find_flags & FIND_FOR_WAIT)
				return true;
			else
				return changed(te->table_id, rowid);
			break;
		case ROW_TRAN_DELETE:
			// 对于删除记录，对当前会话不可见，对其他会话可见
			return !changed(te->table_id, rowid);
			break;
		case (ROW_TRAN_INSERT | ROW_TRAN_DELETE):
			// 对于插入后又删除记录，如果需要等待，则对其他会话可见；如果不需等待，则对所有会话都不可见
			if (_DBCT_find_flags & FIND_FOR_WAIT)
				return !changed(te->table_id, rowid);
			else
				return false;
			break;
		default:
			return false;
		}
	}
}

void dbct_ctx::set_update_func(DBCC_UPDATE_FUNC update_func)
{
	_DBCT_update_func = update_func;
}

void dbct_ctx::set_where_func(compiler::func_type where_func)
{
	_DBCT_where_func = where_func;
}

dbc_inode_t * dbct_ctx::long2inode(long offset)
{
	return reinterpret_cast<dbc_inode_t *>(reinterpret_cast<char *>(_DBCT_bbp) + offset);
}

long dbct_ctx::inode2long(const dbc_inode_t *inode)
{
	return reinterpret_cast<const char *>(inode) - reinterpret_cast<const char *>(_DBCT_bbp);
}

dbc_snode_t * dbct_ctx::long2snode(long offset)
{
	return reinterpret_cast<dbc_snode_t *>(reinterpret_cast<char *>(_DBCT_bbp) + offset);
}

long dbct_ctx::snode2long(const dbc_snode_t *snode)
{
	return reinterpret_cast<const char *>(snode) - reinterpret_cast<const char *>(_DBCT_bbp);
}

redo_bucket_t * dbct_ctx::long2redobucket(long offset)
{
	return reinterpret_cast<redo_bucket_t *>(reinterpret_cast<char *>(_DBCT_bbp) + offset);
}

long dbct_ctx::redobucket2long(const redo_bucket_t *redo_bucket)
{
	return reinterpret_cast<const char *>(redo_bucket) - reinterpret_cast<const char *>(_DBCT_bbp);
}

redo_row_t * dbct_ctx::long2redorow(long offset)
{
	return reinterpret_cast<redo_row_t *>(reinterpret_cast<char *>(_DBCT_bbp) + offset);
}

long dbct_ctx::redorow2long(const redo_row_t *redo_row)
{
	return reinterpret_cast<const char *>(redo_row) - reinterpret_cast<const char *>(_DBCT_bbp);
}

row_link_t * dbct_ctx::rowid2link(long rowid)
{
	return reinterpret_cast<row_link_t *>(reinterpret_cast<char *>(_DBCT_bbp) + rowid);
}

long dbct_ctx::link2rowid(const row_link_t *link)
{
	return reinterpret_cast<const char *>(link) - reinterpret_cast<const char *>(_DBCT_bbp);
}

char * dbct_ctx::rowid2data(long rowid)
{
	return reinterpret_cast<char *>(_DBCT_data_start + rowid);
}

long dbct_ctx::data2rowid(const void *data)
{
	return reinterpret_cast<const char *>(data) - _DBCT_data_start;
}

int dbct_ctx::execute_where_func(char **global)
{
	dbc_te& te_mgr = dbc_te::instance(this);
	dbc_te_t *te = te_mgr.get_te();

	_DBCT_internal_format = true;
	_DBCT_internal_size = te->te_meta.internal_size;

	int retval = (*_DBCT_where_func)(NULL, global, const_cast<const char **>(_DBCT_input_binds), NULL);
	_DBCT_internal_format = false;
	return retval;
}

void dbct_ctx::enter_execute()
{
	_DBCT_timestamp = time(0);

	for (boost::unordered_map<string, seq_cache_t>::iterator iter = _DBCT_seq_map.begin(); iter != _DBCT_seq_map.end(); ++iter)
		iter->second.refetch = true;
}

const char * dbct_ctx::strerror() const
{
	return strerror(_DBCT_error_code);
}

const char * dbct_ctx::strnative() const
{
	return uunix::strerror(_DBCT_native_code);
}

const char * dbct_ctx::strerror(int error_code)
{
	static const char *emsgs[] = {
		"",
		"DBCEABORT - transaction can not commit",			/* DBCEABORT */
		"DBCEBADDESC - bad communication descriptor",		/* DBCEBADDESC */
		"DBCEBLOCK - blocking condition found",				/* DBCEBLOCK */
		"DBCEINVAL - invalid arguments given",				/* DBCEINVAL */
		"DBCELIMIT - a system limit has been reached",		/* DBCELIMIT */
		"DBCENOENT - no entry found",						/* DBCENOENT */
		"DBCEOS - operating system error",					/* DBCEOS */
		"DBCEPERM - bad permissions",						/* DBCEPERM */
		"DBCEPROTO - protocol error",						/* DBCEPROTO */
		"DBCESYSTEM - internal system error",				/* DBCESYSTEM */
		"DBCETIME - timeout occured",						/* DBCETIME */
		"DBCETRAN - error starting transaction",			/* DBCETRAN */
		"DBCGOTSIG - signal received and DBCSIGRSTRT not specified",/* DBCGOTSIG */
		"DBCERMERR - resource manager error",				/* DBCERMERR */
		"DBCERELEASE - invalid release",					/* DBCERELEASE */
		"DBCEMATCH - service name cannot be advertised due to matching conflict", /* DBCEMATCH */
		"DBCEDIAGNOSTIC - function failed - check diagnostic value", 	/* DBCEDIAGNOSTIC */
		"DBCESVRSTAT - bad server status",					/* DBCESVRSTAT */
		"DBCEBBINSANE - BB is insane",						/* DBCEBBINSANE */
		"DBCENOBRIDGE - no BRIDGE available",				/* DBCENOBRIDGE */
		"DBCEDUPENT - duplicate table entry",				/* DBCEDUPENT */
		"DBCECHILD - child start problem",					/* DBCECHILD */
		"DBCENOMSG - no message",							/* DBCENOMSG */
		"DBCENOSERVER - no dbc_server",						/* DBCENOSERVER */
		"DBCENOSWITCH - unknown switch function address",	/* DBCENOSWITCH */
		"DBCEIMAGEINSAME - image is insane",				/* DBCEIMAGEINSAME */
		"DBCESEQNOTDEFINE - sequence is not defined",		/* DBCESEQNOTDEFINE */
		"DBCENOSEQ - sequence doesn't exist",				/* DBCENOSEQ */
		"DBCESEQOVERFLOW - sequence overflow",				/* DBCENOSEQ */
		"DBCEREADONLY - table is readonly"					/* DBCEREADONLY */
	};

	if (error_code <= DBCMINVAL || error_code >= DBCMAXVAL)
		return "";

	return emsgs[error_code];
}

void dbct_ctx::fixed2struct(char *src_buf, char *dst_buf, const dbc_data_t& te_meta)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("src_buf={1}, dst_buf={2}, table_name={3}") % src_buf % dst_buf % te_meta.table_name).str(SGLOCALE), NULL);
#endif
	gpp_ctx *GPP = gpp_ctx::instance();

	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];

		char saver = src_buf[field.load_size];
		src_buf[field.load_size] = '\0';
		common::trim(src_buf, ' ');

		char *field_addr = dst_buf + field.field_offset;

		switch (field.field_type) {
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR:
			*field_addr = *src_buf;
			break;
		case SQLTYPE_STRING:
		case SQLTYPE_VSTRING:
			memcpy(field_addr, src_buf, field.field_size - 1);
			field_addr[field.field_size - 1] = '\0';
			break;
		case SQLTYPE_SHORT:
		case SQLTYPE_USHORT:
			*reinterpret_cast<short *>(field_addr) = static_cast<short>(atoi(src_buf));
			break;
		case SQLTYPE_INT:
		case SQLTYPE_UINT:
			*reinterpret_cast<int *>(field_addr) = atoi(src_buf);
			break;
		case SQLTYPE_LONG:
		case SQLTYPE_ULONG:
			*reinterpret_cast<long *>(field_addr) = atol(src_buf);
			break;
		case SQLTYPE_FLOAT:
			*reinterpret_cast<float *>(field_addr) = static_cast<float>(atof(src_buf));
			break;
		case SQLTYPE_DOUBLE:
			*reinterpret_cast<double *>(field_addr) = atof(src_buf);
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			*reinterpret_cast<time_t *>(field_addr) = static_cast<time_t>(atol(src_buf));
			break;
		default:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error")).str(SGLOCALE));
			exit(1);
		}

		src_buf[field.load_size] = saver;
		src_buf += field.load_size;
	}
}

int dbct_ctx::fixed2record(char *src_buf, char *dst_buf, const dbc_data_t& te_meta)
{
	int extra_size = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, (_("src_buf={1}, dst_buf={2}, table_name={3}") % src_buf % dst_buf % te_meta.table_name).str(SGLOCALE), &extra_size);
#endif
	char *extra_buf = dst_buf + te_meta.internal_size;
	int len;
	gpp_ctx *GPP = gpp_ctx::instance();

	for (int i = 0; i < te_meta.field_size; i++) {
		const dbc_data_field_t& field = te_meta.fields[i];

		char saver = src_buf[field.load_size];
		src_buf[field.load_size] = '\0';
		common::trim(src_buf, ' ');

		char *field_addr = dst_buf + field.internal_offset;

		switch (field.field_type) {
		case SQLTYPE_CHAR:
		case SQLTYPE_UCHAR:
			*field_addr = *src_buf;
			break;
		case SQLTYPE_STRING:
			memcpy(field_addr, src_buf, field.field_size - 1);
			field_addr[field.field_size - 1] = '\0';
			break;
		case SQLTYPE_VSTRING:
			*reinterpret_cast<int *>(field_addr) = extra_size;
			len = strlen(src_buf) + 1;
			memcpy(extra_buf + extra_size, src_buf, len);
			extra_size += len;
			break;
		case SQLTYPE_SHORT:
		case SQLTYPE_USHORT:
			*reinterpret_cast<short *>(field_addr) = static_cast<short>(atoi(src_buf));
			break;
		case SQLTYPE_INT:
		case SQLTYPE_UINT:
			*reinterpret_cast<int *>(field_addr) = atoi(src_buf);
			break;
		case SQLTYPE_LONG:
		case SQLTYPE_ULONG:
			*reinterpret_cast<long *>(field_addr) = atol(src_buf);
			break;
		case SQLTYPE_FLOAT:
			*reinterpret_cast<float *>(field_addr) = static_cast<float>(atof(src_buf));
			break;
		case SQLTYPE_DOUBLE:
			*reinterpret_cast<double *>(field_addr) = atof(src_buf);
			break;
		case SQLTYPE_DATE:
		case SQLTYPE_TIME:
		case SQLTYPE_DATETIME:
			*reinterpret_cast<time_t *>(field_addr) = static_cast<time_t>(atol(src_buf));
			break;
		default:
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error")).str(SGLOCALE));
			exit(1);
		}

		src_buf[field.load_size] = saver;
		src_buf += field.load_size;
	}

	return extra_size;
}

void dbct_ctx::variable2struct(const string& src_buf, char *dst_buf, const dbc_data_t& te_meta)
{
#if defined(DEBUG)
	scoped_debug<bool> debug(50, __PRETTY_FUNCTION__, (_("src_buf={1}, dst_buf={2}, table_name={3}") % src_buf % dst_buf % te_meta.table_name).str(SGLOCALE), NULL);
#endif
	int i = 0;
	gpp_ctx *GPP = gpp_ctx::instance();

	if (te_meta.escape == '\0') {
		char sep_str[2];
		sep_str[0] = te_meta.delimiter;
		sep_str[1] = '\0';
		boost::char_separator<char> sep(sep_str, "", boost::keep_empty_tokens);
		boost::tokenizer<boost::char_separator<char> > tok(src_buf, sep);

		for (boost::tokenizer<boost::char_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter, i++) {
			if (i == te_meta.field_size)
				break;

			const dbc_data_field_t& field = te_meta.fields[i];

			string str = *iter;
			char *field_addr = dst_buf + field.field_offset;

			switch (field.field_type) {
			case SQLTYPE_CHAR:
			case SQLTYPE_UCHAR:
				*field_addr = str[0];
				break;
			case SQLTYPE_STRING:
			case SQLTYPE_VSTRING:
				if (str.length() < field.field_size) {
					memcpy(field_addr, str.c_str(), str.length() + 1);
				} else {
					memcpy(field_addr, str.c_str(), field.field_size - 1);
					field_addr[field.field_size - 1] = '\0';
				}
				break;
			case SQLTYPE_SHORT:
			case SQLTYPE_USHORT:
				*reinterpret_cast<short *>(field_addr) = static_cast<short>(atoi(str.c_str()));
				break;
			case SQLTYPE_INT:
			case SQLTYPE_UINT:
				*reinterpret_cast<int *>(field_addr) = atoi(str.c_str());
				break;
			case SQLTYPE_LONG:
			case SQLTYPE_ULONG:
				*reinterpret_cast<long *>(field_addr) = atol(str.c_str());
				break;
			case SQLTYPE_FLOAT:
				*reinterpret_cast<float *>(field_addr) = static_cast<float>(atof(str.c_str()));
				break;
			case SQLTYPE_DOUBLE:
				*reinterpret_cast<double *>(field_addr) = atof(str.c_str());
				break;
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				*reinterpret_cast<time_t *>(field_addr) = static_cast<time_t>(atol(str.c_str()));
				break;
			default:
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error")).str(SGLOCALE));
				exit(1);
			}
		}
	} else {
		boost::escaped_list_separator<char> sep(te_meta.escape, te_meta.delimiter, '\"');
		boost::tokenizer<boost::escaped_list_separator<char> > tok(src_buf, sep);

		for (boost::tokenizer<boost::escaped_list_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter, i++) {
			if (i == te_meta.field_size)
				break;

			const dbc_data_field_t& field = te_meta.fields[i];

			string str = *iter;
			char *field_addr = dst_buf + field.field_offset;

			switch (field.field_type) {
			case SQLTYPE_CHAR:
			case SQLTYPE_UCHAR:
				*field_addr = str[0];
				break;
			case SQLTYPE_STRING:
			case SQLTYPE_VSTRING:
				if (str.length() <= field.field_size) {
					memcpy(field_addr, str.c_str(), str.length() + 1);
				} else {
					memcpy(field_addr, str.c_str(), field.field_size);
					field_addr[field.field_size] = '\0';
				}
				break;
			case SQLTYPE_SHORT:
			case SQLTYPE_USHORT:
				*reinterpret_cast<short *>(field_addr) = static_cast<short>(atoi(str.c_str()));
				break;
			case SQLTYPE_INT:
			case SQLTYPE_UINT:
				*reinterpret_cast<int *>(field_addr) = atoi(str.c_str());
				break;
			case SQLTYPE_LONG:
			case SQLTYPE_ULONG:
				*reinterpret_cast<long *>(field_addr) = atol(str.c_str());
				break;
			case SQLTYPE_FLOAT:
				*reinterpret_cast<float *>(field_addr) = static_cast<float>(atof(str.c_str()));
				break;
			case SQLTYPE_DOUBLE:
				*reinterpret_cast<double *>(field_addr) = atof(str.c_str());
				break;
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				*reinterpret_cast<time_t *>(field_addr) = static_cast<time_t>(atol(str.c_str()));
				break;
			default:
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error")).str(SGLOCALE));
				exit(1);
			}
		}
	}
}

int dbct_ctx::variable2record(const string& src_buf, char *dst_buf, const dbc_data_t& te_meta)
{
	int extra_size = 0;
#if defined(DEBUG)
	scoped_debug<int> debug(50, __PRETTY_FUNCTION__, (_("src_buf={1}, dst_buf={2}, table_name={3}") % src_buf % dst_buf % te_meta.table_name).str(SGLOCALE), &extra_size);
#endif
	char *extra_buf = dst_buf + te_meta.internal_size;
	int len;
	int i = 0;
	gpp_ctx *GPP = gpp_ctx::instance();

	if (te_meta.escape == '\0') {
		char sep_str[2];
		sep_str[0] = te_meta.delimiter;
		sep_str[1] = '\0';
		boost::char_separator<char> sep(sep_str, "", boost::keep_empty_tokens);
		boost::tokenizer<boost::char_separator<char> > tok(src_buf, sep);

		for (boost::tokenizer<boost::char_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter, i++) {
			if (i == te_meta.field_size)
				break;

			const dbc_data_field_t& field = te_meta.fields[i];

			string str = *iter;
			char *field_addr = dst_buf + field.internal_offset;

			switch (field.field_type) {
			case SQLTYPE_CHAR:
			case SQLTYPE_UCHAR:
				*field_addr = str[0];
				break;
			case SQLTYPE_STRING:
				if (str.length() < field.field_size) {
					memcpy(field_addr, str.c_str(), str.length() + 1);
				} else {
					memcpy(field_addr, str.c_str(), field.field_size - 1);
					field_addr[field.field_size - 1] = '\0';
				}
				break;
			case SQLTYPE_VSTRING:
				*reinterpret_cast<int *>(field_addr) = extra_size;
				if (str.length() < field.field_size)
					len = str.length();
				else
					len = field.field_size - 1;
				memcpy(extra_buf + extra_size, str.c_str(), len);
				extra_buf[extra_size + len] = '\0';
				extra_size += len + 1;
				break;
			case SQLTYPE_SHORT:
			case SQLTYPE_USHORT:
				*reinterpret_cast<short *>(field_addr) = static_cast<short>(atoi(str.c_str()));
				break;
			case SQLTYPE_INT:
			case SQLTYPE_UINT:
				*reinterpret_cast<int *>(field_addr) = atoi(str.c_str());
				break;
			case SQLTYPE_LONG:
			case SQLTYPE_ULONG:
				*reinterpret_cast<long *>(field_addr) = atol(str.c_str());
				break;
			case SQLTYPE_FLOAT:
				*reinterpret_cast<float *>(field_addr) = static_cast<float>(atof(str.c_str()));
				break;
			case SQLTYPE_DOUBLE:
				*reinterpret_cast<double *>(field_addr) = atof(str.c_str());
				break;
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				*reinterpret_cast<time_t *>(field_addr) = static_cast<time_t>(atol(str.c_str()));
				break;
			default:
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error")).str(SGLOCALE));
				exit(1);
			}
		}
	} else {
		boost::escaped_list_separator<char> sep(te_meta.escape, te_meta.delimiter, '\"');
		boost::tokenizer<boost::escaped_list_separator<char> > tok(src_buf, sep);

		for (boost::tokenizer<boost::escaped_list_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter, i++) {
			if (i == te_meta.field_size)
				break;

			const dbc_data_field_t& field = te_meta.fields[i];

			string str = *iter;
			char *field_addr = dst_buf + field.internal_offset;

			switch (field.field_type) {
			case SQLTYPE_CHAR:
			case SQLTYPE_UCHAR:
				*field_addr = str[0];
				break;
			case SQLTYPE_STRING:
				if (str.length() <= field.field_size) {
					memcpy(field_addr, str.c_str(), str.length() + 1);
				} else {
					memcpy(field_addr, str.c_str(), field.field_size);
					field_addr[field.field_size] = '\0';
				}
				break;
			case SQLTYPE_VSTRING:
				*reinterpret_cast<int *>(field_addr) = extra_size;
				if (str.length() <= field.field_size)
					len = str.length();
				else
					len = field.field_size;
				memcpy(extra_buf + extra_size, str.c_str(), len);
				extra_buf[extra_size + len] = '\0';
				extra_size += len + 1;
				break;
			case SQLTYPE_SHORT:
			case SQLTYPE_USHORT:
				*reinterpret_cast<short *>(field_addr) = static_cast<short>(atoi(str.c_str()));
				break;
			case SQLTYPE_INT:
			case SQLTYPE_UINT:
				*reinterpret_cast<int *>(field_addr) = atoi(str.c_str());
				break;
			case SQLTYPE_LONG:
			case SQLTYPE_ULONG:
				*reinterpret_cast<long *>(field_addr) = atol(str.c_str());
				break;
			case SQLTYPE_FLOAT:
				*reinterpret_cast<float *>(field_addr) = static_cast<float>(atof(str.c_str()));
				break;
			case SQLTYPE_DOUBLE:
				*reinterpret_cast<double *>(field_addr) = atof(str.c_str());
				break;
			case SQLTYPE_DATE:
			case SQLTYPE_TIME:
			case SQLTYPE_DATETIME:
				*reinterpret_cast<time_t *>(field_addr) = static_cast<time_t>(atol(str.c_str()));
				break;
			default:
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: internal error")).str(SGLOCALE));
				exit(1);
			}
		}
	}

	return extra_size;
}

string dbct_ctx::get_ready_file(const string& lmid)
{
	string retval;
#if defined(DEBUG)
	scoped_debug<string> debug(300, __PRETTY_FUNCTION__, (_("lmid={1}") % lmid).str(SGLOCALE), &retval);
#endif

	dbc_bbparms_t& bbparms = _DBCT_bbp->bbparms;

	retval = string(bbparms.data_dir) + "system/ready." + lmid;
	return retval;
}

void dbct_ctx::bb_change(int signo)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	dbcp_ctx *DBCP = dbcp_ctx::instance();
	dbct_ctx *DBCT = dbct_ctx::instance();
#if defined(DEBUG)
	scoped_debug<bool> debug(200, __PRETTY_FUNCTION__, (_("signo={1}") % signo).str(SGLOCALE), NULL);
#endif

	if (!DBCT->_DBCT_login)
		return;

	try {
		dbc_rte_t *server_rte = DBCT->_DBCT_rtes + RTE_SLOT_SERVER;
		if (!server_rte->in_flags(RTE_IN_USE)
			|| server_rte->in_flags(RTE_MARK_REMOVED)
			|| kill(server_rte->pid, 0) == -1) {
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: dbc_server died, client exit.")).str(SGLOCALE));
			exit(0);
		}

		dbc_rte& rte_mgr = dbc_rte::instance(DBCT);
		dbc_rte_t *rte = rte_mgr.get_rte();
		dbc_bbparms_t& bbparms = DBCT->_DBCT_bbp->bbparms;
		dbc_bbmeters_t& bbmeters = DBCT->_DBCT_bbp->bbmeters;
		dbc_bbmap_t& bbmap = DBCT->_DBCT_bbp->bbmap;

		if (!rte->in_flags(RTE_REATTACH) || bbparms.shm_count + bbmeters.additional_count == DBCP->_DBCP_attached_count)
			return;

		void *shm_addr = reinterpret_cast<void *>(reinterpret_cast<char *>(DBCP->_DBCP_attach_addr) + bbparms.total_size);
		long total_size = bbparms.shm_size * bbmeters.additional_count;

		if (munmap(shm_addr, total_size) == -1) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: munmap failed - {1}") % ::strerror(errno)).str(SGLOCALE));
			return;
		}

		for (int i = bbparms.shm_count; i < bbparms.shm_count + bbmeters.additional_count; i++) {
			if (::shmat(bbmap.shmids[i], shm_addr, bbparms.perm) != shm_addr) {
				DBCT->_DBCT_error_code = DBCEOS;
				DBCT->_DBCT_native_code = USHMAT;
				GPP->write_log(__FILE__, __LINE__, (_("ERROR: shmat failed - {1}") % ::strerror(errno)).str(SGLOCALE));
				return;
			}

			shm_addr = reinterpret_cast<void *>(reinterpret_cast<char *>(shm_addr) + bbparms.shm_size);
			DBCP->_DBCP_attached_count++;
		}

		// In multi-thread mode, clear RTE_REATTACH for all threads' RTEs.
		for (dbc_rte_t *rte = DBCT->_DBCT_rtes; rte < DBCT->_DBCT_rtes + bbparms.maxrtes; rte++) {
			if (!rte->in_flags(RTE_IN_USE) || rte->in_flags(RTE_MARK_REMOVED))
				continue;

			if (rte->pid == DBCP->_DBCP_current_pid)
				rte->clear_flags(RTE_REATTACH);
		}
	} catch (exception& ex) {
		GPP->write_log(__FILE__, __LINE__, ex.what());
	}
}

bool dbct_ctx::reserve_addr(const dbc_bbparms_t& config_bbparms)
{
	gpp_ctx *GPP = gpp_ctx::instance();
	dbcp_ctx *DBCP = dbcp_ctx::instance();
	bool retval = false;
#if defined(DEBUG)
	scoped_debug<bool> debug(300, __PRETTY_FUNCTION__, "", &retval);
#endif

#if defined(LINUX)
	DBCP->_DBCP_attach_addr = mmap(NULL, config_bbparms.reserve_count * config_bbparms.shm_size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#else
	int devzero_fd = open("/dev/zero", O_RDONLY);
	if (devzero_fd == -1) {
		_DBCT_error_code = DBCEOS;
		_DBCT_native_code = UOPEN;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Open /dev/zero failed - {1}") % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	DBCP->_DBCP_attach_addr = mmap(NULL, config_bbparms.reserve_count * config_bbparms.shm_size, PROT_READ, MAP_PRIVATE, devzero_fd, 0);
	close(devzero_fd);
#endif

	if (DBCP->_DBCP_attach_addr == reinterpret_cast<void *>(-1)) {
		_DBCT_error_code = DBCESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: mmap failed - {1}") % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	if (munmap(DBCP->_DBCP_attach_addr, config_bbparms.total_size) == -1) {
		_DBCT_error_code = DBCESYSTEM;
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: munmap failed - {1}") % ::strerror(errno)).str(SGLOCALE));
		return retval;
	}

	retval = true;
	return retval;
}

}
}

