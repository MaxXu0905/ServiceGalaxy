/**
 * @file tcpsocket.h
 * @brief The definition of the tcpsocket class.
 * 
 * @author Dejin Bao
 * @author Former author list: Genquan Xu
 * @version 3.5.0
 * @date 2005-12-07
 */

#ifndef __TCPSOCKET_H__
#define __TCPSOCKET_H__

#include <string>
#include "dstream.h"
#include "ipsocket.h"

namespace ai
{
namespace sg
{

using std::string;

/**
 * @class tcpsocket
 * @brief The tcpsocket class.
 */
class tcpsocket : public ipsocket
{
public:
    tcpsocket();
    ~tcpsocket();
    int create(const char* ipaddr, int port);
    int create(int port);
    void receive(string& packet);
    void send(const string& packet);
    
private:
    sockaddr_in inet_address;    ///<The socket address.
    int _runmode;    ///<Run mode: 0=client, 1=server.
};

}
}

#endif

