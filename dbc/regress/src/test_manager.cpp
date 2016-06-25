#include "test_manager.h"

namespace ai
{
namespace sg
{

test_manager::test_manager(const string& user_name, const string& password)
	: api_mgr(dbc_api::instance(DBCT))
{
	if (!api_mgr.login())
		exit(1);

	if (!api_mgr.connect(user_name, password))
		exit(1);
}

test_manager::~test_manager()
{
	api_mgr.disconnect();
	api_mgr.logout();
}

bool test_manager::execute(const dbc_action_t& dbc_action)
{
	test_ctx *TSTC = test_ctx::self();

	// 执行特定函数
	switch (dbc_action.action_type) {
	case ACTION_TYPE_SELECT:
		{
			long *retval = TSTC->get_long(dbc_action.arg1);
			void *key = TSTC->get_void(dbc_action.arg2);
			void *result = TSTC->get_void(dbc_action.arg3);

			if (dbc_action.index_id == -1)
				*retval = api_mgr.find(dbc_action.table_id, key, result);
			else
				*retval = api_mgr.find(dbc_action.table_id, dbc_action.index_id, key, result);

			if (dbc_action.expect_type == EXPECT_TYPE_SUCCESS && *retval <= 0
				|| dbc_action.expect_type == EXPECT_TYPE_FAIL && *retval > 0)
				return false;

			if (dbc_action.expect_type == EXPECT_TYPE_SUCCESS) {
				t_bds_area_code *left = TSTC->get_struct(dbc_action.arg3);
				t_bds_area_code *right = TSTC->get_struct(dbc_action.arg4);
				if (!TSTC->match(*left, *right))
					return false;
			}
			break;
		}
	case ACTION_TYPE_INSERT:
		{
			bool *retval = TSTC->get_bool(dbc_action.arg1);
			void *data = TSTC->get_void(dbc_action.arg2);
			*retval = api_mgr.insert(dbc_action.table_id, data);
			if (dbc_action.expect_type == EXPECT_TYPE_SUCCESS && !(*retval)
				|| dbc_action.expect_type == EXPECT_TYPE_FAIL && *retval)
				return false;
			break;
		}
	case ACTION_TYPE_UPDATE:
		{
			long *retval = TSTC->get_long(dbc_action.arg1);
			void *old_data = TSTC->get_void(dbc_action.arg2);
			void *new_data = TSTC->get_void(dbc_action.arg3);

			if (dbc_action.index_id == -1)
				*retval = api_mgr.update(dbc_action.table_id, old_data, new_data);
			else
				*retval = api_mgr.update(dbc_action.table_id, dbc_action.index_id, old_data, new_data);

			if (dbc_action.expect_type == EXPECT_TYPE_SUCCESS && *retval <= 0
				|| dbc_action.expect_type == EXPECT_TYPE_FAIL && *retval > 0)
				return false;
			break;
		}
	case ACTION_TYPE_DELETE:
		{
			long *retval = TSTC->get_long(dbc_action.arg1);
			void *data = TSTC->get_void(dbc_action.arg2);
			if (dbc_action.index_id == -1)
				*retval = api_mgr.erase(dbc_action.table_id, data);
			else
				*retval = api_mgr.erase(dbc_action.table_id, dbc_action.index_id, data);

			if (dbc_action.expect_type == EXPECT_TYPE_SUCCESS && *retval <= 0
				|| dbc_action.expect_type == EXPECT_TYPE_FAIL && *retval > 0)
				return false;
			break;
		}
	case ACTION_TYPE_DELETE_ROWID:
		{
			bool *retval = TSTC->get_bool(dbc_action.arg1);
			long *rowid = TSTC->get_long(dbc_action.arg2);
			*retval = api_mgr.erase_by_rowid(dbc_action.table_id, *rowid);
			if (dbc_action.expect_type == EXPECT_TYPE_SUCCESS && !(*retval)
				|| dbc_action.expect_type == EXPECT_TYPE_FAIL && *retval)
				return false;
			break;
		}
	case ACTION_TYPE_COMMIT:
		{
			bool *retval = TSTC->get_bool(dbc_action.arg1);
			*retval = api_mgr.commit();
			if (dbc_action.expect_type == EXPECT_TYPE_SUCCESS && !(*retval)
				|| dbc_action.expect_type == EXPECT_TYPE_FAIL && *retval)
				return false;
			break;
		}
	case ACTION_TYPE_ROLLBACK:
		{
			bool *retval = TSTC->get_bool(dbc_action.arg1);
			*retval = api_mgr.rollback();
			if (dbc_action.expect_type == EXPECT_TYPE_SUCCESS && !(*retval)
				|| dbc_action.expect_type == EXPECT_TYPE_FAIL && *retval)
				return false;
			break;
		}
	case ACTION_TYPE_GET_ROWID:
		{
			long *rowid = TSTC->get_long(dbc_action.arg1);
			*rowid = api_mgr.get_rowid(dbc_action.arg2);
			break;
		}
	case ACTION_TYPE_GET_ROWIDS:
		{
			long *rowids = TSTC->get_long(dbc_action.arg1);
			long *rows = TSTC->get_long(dbc_action.arg2);
			for (long i = 0; i < *rows; i++)
				rowids[i] = api_mgr.get_rowid(i);
			break;
		}
	case ACTION_TYPE_GET_STATUS:
	case ACTION_TYPE_END:
		return true;
	}

	return true;
}

}
}

