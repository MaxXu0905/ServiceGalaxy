#if !defined(__TEST_MANAGER_H__)
#define __TEST_MANAGER_H__

#include "test_action.h"

namespace ai
{
namespace sg
{

class test_manager : public dbc_manager
{
public:
	test_manager(const string& user_name, const string& password);
	~test_manager();

	bool execute(const dbc_action_t& dbc_action);

protected:
	dbc_api& api_mgr;
};

}
}

#endif

