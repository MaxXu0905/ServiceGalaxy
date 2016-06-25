#include "dbc_internal.h"

namespace ai
{
namespace sg
{

void dbc_ue_t::set_flags(int flags_)
{
	flags |= flags_;
}

void dbc_ue_t::clear_flags(int flags_)
{
	flags &= ~flags_;
}

bool dbc_ue_t::in_flags(int flags_) const
{
	return (flags & flags_);
}

bool dbc_ue_t::in_use() const
{
	return (flags & UE_IN_USE);
}

int dbc_ue_t::get_user_idx() const
{
	return static_cast<int>(user_id & UE_ID_MASK);
}

dbc_ue& dbc_ue::instance(dbct_ctx *DBCT)
{
	return *reinterpret_cast<dbc_ue *>(DBCT->_DBCT_mgr_array[DBC_UE_MANAGER]);
}

dbc_ue_t * dbc_ue::get_ue()
{
	return ue;
}

void dbc_ue::set_ctx(int user_idx)
{
	ue = DBCT->_DBCT_ues + user_idx;
	DBCT->_DBCT_user_id = ue->user_id;
}

void dbc_ue::set_ctx(dbc_ue_t *ue_)
{
	ue = ue_;
	DBCT->_DBCT_user_id = ue->user_id;
}

bool dbc_ue::connect(const std::string& user_name, const std::string& password)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;

	if (!DBCT->_DBCT_login) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: login() required before connect()")).str(SGLOCALE));
		DBCT->_DBCT_error_code = DBCEPROTO;
		return false;
	}

	if (ue != NULL)
		disconnect();

	string encrypted_password;
	if (!SGP->encrypt(password, encrypted_password)) {
		GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't encrypt password")).str(SGLOCALE));
		DBCT->_DBCT_error_code = DBCESYSTEM;
		return false;
	}

	dbc_ue_t *cue = DBCT->_DBCT_ues;
	for (int i = 0; i < bbparms.maxusers; i++, cue++) {
		if (!cue->in_use())
			continue;

		if (user_name == cue->user_name && encrypted_password == cue->password) {
			ue = cue;
			DBCT->_DBCT_user_id = ue->user_id;
			return true;
		}
	}

	return false;
}

void dbc_ue::disconnect()
{
	if (ue == NULL)
		return;

	ue = NULL;
	DBCT->_DBCT_user_id = -1;
}

bool dbc_ue::connected() const
{
	return (ue != NULL);
}

dbc_te_t * dbc_ue::get_table(int table_id)
{
	thash_t offset = ue->thash[table_id];
	if (offset < 0)
		throw bad_msg(__FILE__, __LINE__, 0, (_("ERROR: Can't find table entry in table list")).str(SGLOCALE));

	return reinterpret_cast<dbc_te_t *>(reinterpret_cast<char *>(DBCT->_DBCT_tes) + offset);
}

dbc_se_t * dbc_ue::get_seq(const string& seq_name)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;
	int hash_value = DBCT->get_hash(seq_name.c_str(), bbparms.sebkts);

	int offset = ue->shash[hash_value];
	while (offset != -1) {
		dbc_se_t *se = DBCT->_DBCT_ses + offset;
		if (strcasecmp(se->seq_name, seq_name.c_str()) == 0)
			return se;
		offset = se->next;
	}

	return NULL;
}

int64_t dbc_ue::create_user(const dbc_user_t& user)
{
	dbc_bboard_t *bbp = DBCT->_DBCT_bbp;
	dbc_bbparms_t& bbparms = bbp->bbparms;

	size_t seed = boost::hash_value(user.user_name);
	boost::hash_combine(seed, bbparms.ipckey & ((1 << MCHIDSHIFT) - 1));
	boost::random::rand48 rand48(seed);
	int64_t key = (static_cast<int64_t>(rand48()) << 32) | (static_cast<int64_t>(rand48()) << UE_ID_BITS);

	// 如果有读权限，则增加执行权限
	int perm = bbparms.perm;
	if (perm & 0400)
		perm |= 0100;
	if (perm & 0040)
		perm |= 0010;
	if (perm & 0004)
		perm |= 0001;

	dbc_ue_t *cue = DBCT->_DBCT_ues;
	for (int i = 0; i < bbparms.maxusers; i++, cue++) {
		if (cue->in_use())
			continue;

		strcpy(cue->user_name, user.user_name.c_str());
		strcpy(cue->password, user.password.c_str());
		strcpy(cue->dbname, user.dbname.c_str());
		strcpy(cue->openinfo, user.openinfo.c_str());
		cue->user_id = key | i;
		cue->perm = user.perm;
		cue->flags = UE_IN_USE;
		cue->ctime = cue->mtime = time(0);

		for (int j = 0; j < MAX_DBC_ENTITIES; j++) {
			cue->thash[j] = -1;
			cue->shash[j] = -1;
		}

		string data_dir = string(bbparms.data_dir) + cue->user_name + "/";
		if (mkdir(data_dir.c_str(), perm) == -1 && errno != EEXIST) {
			DBCT->_DBCT_error_code = DBCESYSTEM;
			GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create data_dir {1} for user {2}") % data_dir % cue->user_name).str(SGLOCALE));
			return -1;
		}

		return cue->user_id;
	}


	DBCT->_DBCT_error_code = DBCELIMIT;
	GPP->write_log(__FILE__, __LINE__, (_("ERROR: Can't create user {1}, too many users") % user.user_name).str(SGLOCALE));
	return -1;
}

dbc_ue::dbc_ue()
{
	// 指向系统用户
	ue = NULL;
}

dbc_ue::~dbc_ue()
{
}

}
}

