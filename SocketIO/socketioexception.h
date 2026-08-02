#include <string>
#include "zexception.h"


#ifndef __SOCKETIOEXCEPTION_H__
#define __SOCKETIOEXCEPTION_H__

namespace Zigurat
{

  class SocketIOException : public ZiguratException
  {
  public:
    SocketIOException() = delete;
    SocketIOException(std::string msg) : ZiguratException(5200, msg) { }
  };

}

#endif // __SOCKETIOEXCEPTION_H__
