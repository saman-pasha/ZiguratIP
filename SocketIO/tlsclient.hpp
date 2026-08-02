
#ifndef __TLSCLIENT_HPP__
#define __TLSCLIENT_HPP__

#include "tcpclient.h"

namespace Zigurat
{

  class TLSClient : public TCPClient
  {
  public:
    using TCPClient::TCPClient;

  };

}

#endif // __TLSCLIENT_HPP__
