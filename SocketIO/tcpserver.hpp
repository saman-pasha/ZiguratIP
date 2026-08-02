
#ifndef __TCPSERVER_HPP__
#define __TCPSERVER_HPP__

#include "socket.hpp"
#include <functional>
#include "threadpool.hpp"

namespace Zigurat
{

  class TCPServer
  {
  protected:
    Socket::handle_t _handle = Socket::INVALID_SOCKET;
    ThreadPool _pool;
    bool _shutdown;

  public:
    typedef std::function<void (Socket::handle_t)> client_handler_t;

    enum Version {
      IPV4,
      IPV6
    };
      
    TCPServer();
    virtual void run(Version, std::string, int, size_t, client_handler_t);
    virtual void shutdown(bool);
    virtual ~TCPServer();
  };

}

#endif // __TCPSERVER_HPP__
