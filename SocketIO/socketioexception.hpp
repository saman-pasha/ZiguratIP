#include <string>
#include "zexception.hpp"


#ifndef __SOCKETIOEXCEPTION_HPP__
#define __SOCKETIOEXCEPTION_HPP__

namespace Zigurat
{

  class SocketIOException : public ZiguratException
  {
  public:
    SocketIOException() = delete;
    SocketIOException(std::string msg) : ZiguratException(5200, msg) { }
  };

}

#endif // __SOCKETIOEXCEPTION_HPP__
