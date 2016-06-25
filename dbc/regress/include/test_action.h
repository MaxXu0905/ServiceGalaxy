#if !defined(__TEST_ACTION_H__)
#define __TEST_ACTION_H__

#include "dbc_api.h"
#include "oracle_control.h"
#include "sql_control.h"
#include "datastruct_structs.h"
#include "search_h.inl"
#include "test_ctx.h"

namespace ai
{
namespace sg
{

enum dbc_action_type
{
	ACTION_TYPE_SELECT,
	ACTION_TYPE_INSERT,
	ACTION_TYPE_UPDATE,
	ACTION_TYPE_DELETE,
	ACTION_TYPE_DELETE_ROWID,
	ACTION_TYPE_COMMIT,
	ACTION_TYPE_ROLLBACK,
	ACTION_TYPE_GET_ROWID,
	ACTION_TYPE_GET_ROWIDS,
	ACTION_TYPE_GET_STATUS,
	ACTION_TYPE_END
};

enum dbc_expect_type
{
	EXPECT_TYPE_SUCCESS,
	EXPECT_TYPE_FAIL,
};

int const ACTION_FLAG_TIMEDOUT = 0x1;

struct dbc_action_t
{
	dbc_action_type action_type;
	int thread_id;
	int table_id;
	int index_id;
	int flags;
	dbc_expect_type expect_type;
	long arg1;
	long arg2;
	long arg3;
	long arg4;
};

// 测试单会话查询功能
const dbc_action_t dbc_actions_0[] = {
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试单会话插入功能
const dbc_action_t dbc_actions_1[] = {
	{ ACTION_TYPE_INSERT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_bool), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_INSERT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), offsetof(test_ctx, bds_area_code[2]), -1, -1 },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[2]) },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试单会话更新功能
const dbc_action_t dbc_actions_2[] = {
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_UPDATE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[2]) },
	{ ACTION_TYPE_ROLLBACK, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[2]) },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试单会话删除功能
const dbc_action_t dbc_actions_3[] = {
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_ROLLBACK, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_ROLLBACK, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_GET_ROWID, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, rowid), 0, -1, -1 },
	{ ACTION_TYPE_DELETE_ROWID, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), offsetof(test_ctx, rowid), -1, -1 },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_ROLLBACK, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试双会话插入功能
const dbc_action_t dbc_actions_4[] = {
	{ ACTION_TYPE_INSERT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), offsetof(test_ctx, bds_area_code[2]), -1, -1 },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[2]) },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[2]) },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试双会话更新功能
const dbc_action_t dbc_actions_5[] = {
	{ ACTION_TYPE_UPDATE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[2]) },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[2]) },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试双会话删除功能
const dbc_action_t dbc_actions_6[] = {
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试插入等待
const dbc_action_t dbc_actions_7[] = {
	{ ACTION_TYPE_INSERT, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), offsetof(test_ctx, bds_area_code[2]), -1, -1 },
	{ ACTION_TYPE_INSERT, 0, T_BDS_AREA_CODE, -1, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_bool), offsetof(test_ctx, bds_area_code[2]), -1, -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试更新等待1
const dbc_action_t dbc_actions_8[] = {
	{ ACTION_TYPE_UPDATE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_INSERT, 0, T_BDS_AREA_CODE, -1, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_SELECT, 0, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[2]) },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试更新等待2
const dbc_action_t dbc_actions_9[] = {
	{ ACTION_TYPE_UPDATE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_INSERT, 0, T_BDS_AREA_CODE, -1, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_bool), offsetof(test_ctx, bds_area_code[2]), -1, -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试更新等待3
const dbc_action_t dbc_actions_10[] = {
	{ ACTION_TYPE_UPDATE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_DELETE, 0, T_BDS_AREA_CODE, -1, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试更新等待4
const dbc_action_t dbc_actions_11[] = {
	{ ACTION_TYPE_UPDATE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_DELETE, 0, T_BDS_AREA_CODE, -1, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), -1, -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试更新等待5
const dbc_action_t dbc_actions_12[] = {
	{ ACTION_TYPE_UPDATE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_UPDATE, 0, T_BDS_AREA_CODE, 0, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试更新等待6
const dbc_action_t dbc_actions_13[] = {
	{ ACTION_TYPE_UPDATE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_UPDATE, 0, T_BDS_AREA_CODE, 0, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, bds_area_code[0]), -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试删除等待1
const dbc_action_t dbc_actions_14[] = {
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_DELETE, 0, T_BDS_AREA_CODE, -1, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试删除等待2
const dbc_action_t dbc_actions_15[] = {
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_UPDATE, 0, T_BDS_AREA_CODE, 0, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试删除等待3
const dbc_action_t dbc_actions_16[] = {
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_INSERT, 0, T_BDS_AREA_CODE, -1, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试删除等待4
const dbc_action_t dbc_actions_17[] = {
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_DELETE, 0, T_BDS_AREA_CODE, -1, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_ROLLBACK,-1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试删除等待5
const dbc_action_t dbc_actions_18[] = {
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_UPDATE, 0, T_BDS_AREA_CODE, 0, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, bds_area_code[2]), -1 },
	{ ACTION_TYPE_ROLLBACK, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 测试删除等待6
const dbc_action_t dbc_actions_19[] = {
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_INSERT, 0, T_BDS_AREA_CODE, -1, ACTION_FLAG_TIMEDOUT, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_bool), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_ROLLBACK, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_GET_STATUS, 0, -1, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

// 删除多条记录
const dbc_action_t dbc_actions_20[] = {
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[1]), -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[3]), -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[4]), -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[5]), -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[6]), -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[7]), -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[8]), -1, -1 },
	{ ACTION_TYPE_DELETE, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[9]), -1, -1 },
	{ ACTION_TYPE_COMMIT, -1, -1, -1, 0, EXPECT_TYPE_SUCCESS, offsetof(test_ctx, ret_bool), -1, -1, -1 },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[0]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[0]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[1]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[1]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[2]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[2]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[3]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[3]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[4]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[4]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[5]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[5]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[6]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[6]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[7]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[7]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[8]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[8]) },
	{ ACTION_TYPE_SELECT, -1, T_BDS_AREA_CODE, 0, 0, EXPECT_TYPE_FAIL, offsetof(test_ctx, ret_long), offsetof(test_ctx, bds_area_code[9]), offsetof(test_ctx, find_result), offsetof(test_ctx, bds_area_code[9]) },
	{ ACTION_TYPE_END, -1, T_BDS_AREA_CODE, -1, 0, EXPECT_TYPE_SUCCESS, -1, -1, -1, -1 }
};

const dbc_action_t * const test_plan[] = {
	dbc_actions_0,
	dbc_actions_1,
	dbc_actions_2,
	dbc_actions_3,
	dbc_actions_4,
	dbc_actions_5,
	dbc_actions_6,
	dbc_actions_7,
	dbc_actions_8,
	dbc_actions_9,
	dbc_actions_10,
	dbc_actions_11,
	dbc_actions_12,
	dbc_actions_13,
	dbc_actions_14,
	dbc_actions_15,
	dbc_actions_16,
	dbc_actions_17,
	dbc_actions_18,
	dbc_actions_19,
	dbc_actions_20
};

}
}

#endif

