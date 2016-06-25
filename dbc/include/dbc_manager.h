#if !defined(__DBC_MANAGER_H__)
#define __DBC_MANAGER_H__

#include "dbc_struct.h"

namespace ai
{
namespace sg
{

enum dbc_manager_t {
	DBC_UE_MANAGER,
	DBC_TE_MANAGER,
	DBC_IE_MANAGER,
	DBC_SE_MANAGER,
	DBC_RTE_MANAGER,
	DBC_REDO_MANAGER,
	DBC_REDO_TREE_MANAGER,
	DBC_INODE_TREE_MANAGER,
	DBC_INODE_LIST_MANAGER,
	DBC_SQLITE_MANAGER,
	DBC_API_MANAGER,
	SDC_CONFIG_MANAGER,
	DBC_TOTAL_MANAGER
};

class dbc_manager : public sg_manager
{
public:
	dbc_manager();
	virtual ~dbc_manager();

protected:
	dbcp_ctx *DBCP;
	dbct_ctx *DBCT;
};

}
}

#endif

