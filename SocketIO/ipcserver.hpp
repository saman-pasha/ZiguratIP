
#ifndef __IPCSERVER_HPP__
#define __IPCSERVER_HPP__

#include "socket.hpp"
#include <functional>
#include "threadpool.hpp"

namespace Zigurat
{

  class IPCServer
  {
  protected:
    Socket::handle_t _handle = Socket::INVALID_SOCKET;
    ThreadPool _pool;
    bool _shutdown;

  public:
    typedef std::function<void (Socket::handle_t)> client_handler_t;

    IPCServer();
    virtual void run(std::string, int, size_t, client_handler_t);
    virtual void shutdown(bool);
    virtual ~IPCServer();
  };

}

#endif // __IPCSERVER_HPP__
