/*
 * svc_test_advertise.h
 *
 *  Created on: 2012-3-26
 *      Author: renyz
 */

#ifndef svc_test_advertise_H_
#define svc_test_advertise_H_
#include "sg_public.h"
namespace ai
{
namespace sg
{
class svc_test_advertise:public sg_svc{
public:
	typedef  boost::tokenizer<boost::char_separator<char> > tokenizer;
	svc_test_advertise();
	virtual ~svc_test_advertise();
	svc_fini_t svc(message_pointer& svcinfo);
};

}
}
#endif /* svc_test_advertise_H_ */
