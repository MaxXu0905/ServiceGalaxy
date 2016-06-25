#if !defined(__MRP_CTX_H__)
#define __MRP_CTX_H__

#include <unistd.h>
#include <string>
#include <map>
#include "log_file.h"
#include "uunix.h"
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread/once.hpp>
#include <boost/unordered_map.hpp>

namespace ai
{
namespace sg
{

class mrp_ctx
{
public:
	static mrp_ctx * instance();
	~mrp_ctx();

	// 元数据配置文件
	std::string _MRP_config_file;
	// 元数据配置文件加载后的映射表
	boost::unordered_map<std::string, std::string> _MRP_key_map;

private:
	mrp_ctx();

	static void init_once();

	static boost::once_flag once_flag;
	static mrp_ctx *_instance;
};

}
}

#endif

