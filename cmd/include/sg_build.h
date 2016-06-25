#if !defined(__SG_BUILD_H__)
#define __SG_BUILD_H__

#include "sg_internal.h"

namespace ai
{
namespace sg
{

class sg_build
{
public:
	sg_build();
	~sg_build();

	int run(int argc, char **argv);

private:
	bool init_svr(int argc, char **argv);
	bool init_clt(int argc, char **argv);
	bool gen_svr();
	bool build_svr();
	bool build_clt();

	int build_flag;
	std::vector<std::string> first_files;
	std::vector<std::string> last_files;
	std::vector<std::string> include_files;
	std::string output_file;
	std::vector<std::string> namespaces;
	std::string server_class;
	std::map<std::string, std::string> service_map;
	bool keep_temp;
	bool system_used;
	bool verbose;

	std::string srcf;
	std::string objf;
};

}
}

#endif

