#if !defined(__SG_LOADCF_H__)
#define __SG_LOADCF_H__

#include "sg_internal.h"

namespace ai
{
namespace sg
{

using namespace std;

class sg_loadcf
{
public:
	sg_loadcf();
	~sg_loadcf();

	void init(int argc, char **argv);
	void run();

private:
	gpp_ctx *GPP;
	sgp_ctx *SGP;
	sgt_ctx *SGT;

	bool prompt;
	bool suppress;
	std::string xml_file;
	std::string passwd;
	bool enable_hidden;
	std::string sgconfig;
};

}
}

#endif

