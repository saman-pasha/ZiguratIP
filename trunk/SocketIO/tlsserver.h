
#ifndef __TLSSERVER_H__
#define __TLSSERVER_H__

#include "tcpserver.h"

namespace Zigurat
{

  class TLSServer : public TCPServer
  {
  public:
    using TCPServer::TCPServer;

  };

}

#endif // __TLSSERVER_H__
