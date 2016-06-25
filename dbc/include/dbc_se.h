#if !defined(__DBC_SE_H__)
#define __DBC_SE_H__

#include "dbc_struct.h"
#include "dbc_manager.h"

namespace ai
{
namespace sg
{

struct dbc_se_t
{
	char seq_name[SEQUENCE_NAME_SIZE + 1];
	int flags;
	int accword;
	long minval;
	long maxval;
	long currval;
	long increment;
	long cache;
	bool cycle;
	bool order;
	int prev;
	int next;

	void init();
	dbc_se_t& operator=(const dbc_se_t& rhs);
};

class dbc_se : public dbc_manager
{
public:
	static dbc_se& instance(dbct_ctx *DBCT);

	// 切换操作表上下文
	void set_ctx(dbc_se_t *te_);
	// 获取当前的SE结构
	dbc_se_t * get_se();

	long get_currval(const string& seq_name);
	long get_nextval(const string& seq_name);
	long get_nextval();

private:
	dbc_se();
	virtual ~dbc_se();

	dbc_se_t *se;

	friend class dbct_ctx;
};

}
}

#endif

