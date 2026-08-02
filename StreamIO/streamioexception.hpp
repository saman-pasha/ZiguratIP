#include <string>
#include "zexception.hpp"


#ifndef __STREAMIOEXCEPTION_HPP__
#define __STREAMIOEXCEPTION_HPP__

namespace Zigurat 
{

  class StreamIOException : public ZiguratException
  {
  public:
    StreamIOException() = delete;
    StreamIOException(std::string msg) : ZiguratException(5100, msg) { }
  };

}

#endif // __STREAMIOEXCEPTION_HPP__
