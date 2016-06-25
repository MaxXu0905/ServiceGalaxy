#include "metarep_svc.h"
#include "metarep_struct.h"

using namespace std;

namespace ai
{
namespace sg
{

metarep_svc::metarep_svc()
{
	MRP = mrp_ctx::instance();
}

metarep_svc::~metarep_svc()
{
}

svc_fini_t metarep_svc::svc(message_pointer& svcinfo)
{
	sg_metarep_t *data = reinterpret_cast<sg_metarep_t *>(svcinfo->data());
	size_t len = svcinfo->length() - sizeof(int);

	if (len <= 0)
		return svc_fini(SGFAIL, MREINVAL, svcinfo);

	int version = data->version;
	string key(data->key, data->key + len);

	boost::unordered_map<string, string>::const_iterator iter = MRP->_MRP_key_map.find(key);
	if (iter == MRP->_MRP_key_map.end())
		return svc_fini(SGFAIL, MRENOENT, svcinfo);

	string filename = iter->second;
	if (version >= 0) {
		filename += ".";
		filename += boost::lexical_cast<string>(version);
	}

	int fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0)
		return svc_fini(SGFAIL, MRENOFILE, svcinfo);

	BOOST_SCOPE_EXIT((&fd)) {
		close(fd);
	} BOOST_SCOPE_EXIT_END

	struct stat buf;
	if (fstat(fd, &buf) == -1)
		return svc_fini(SGFAIL, MREOS, svcinfo);

	if (buf.st_size == 0) {
		svcinfo->set_length(0);
	} else {
		svcinfo->set_length(buf.st_size);
		if (read(fd, svcinfo->data(), buf.st_size) != buf.st_size)
			return svc_fini(SGFAIL, MREOS, svcinfo);
	}

	return svc_fini(SGSUCCESS, 0, svcinfo);
}

}
}

