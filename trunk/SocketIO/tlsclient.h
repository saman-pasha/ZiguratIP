
#ifndef __TLSCLIENT_H__
#define __TLSCLIENT_H__

#include "tcpclient.h"

namespace Zigurat
{

  class TLSClient : public TCPClient
  {
  public:
    using TCPClient::TCPClient;

  };

}

#endif // __TLSCLIENT_H__
