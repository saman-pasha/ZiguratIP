
#ifndef __TLSSERVER_HPP__
#define __TLSSERVER_HPP__

#include "tcpserver.hpp"

namespace Zigurat
{

  class TLSServer : public TCPServer
  {
  public:
    using TCPServer::TCPServer;

  };

}

#endif // __TLSSERVER_HPP__
