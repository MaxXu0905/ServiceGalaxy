#if !defined(__SG_UNLOADCF_H__)
#define __SG_UNLOADCF_H__

#include "sg_internal.h"

namespace ai
{
namespace sg
{

using namespace std;

class sg_unloadcf
{
public:
	sg_unloadcf();
	~sg_unloadcf();

	void init(int argc, char **argv);
	void run();

private:
	gpp_ctx *GPP;
	sgp_ctx *SGP;

	bool prompt;
	bool suppress;
	string xml_file;
	bool enable_hidden;
	string config_file;
};

}
}

#endif

