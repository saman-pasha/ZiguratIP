
#ifndef __ZIGURATIPEXCEPTION_HPP__
#define __ZIGURATIPEXCEPTION_HPP__

#include "zexception.hpp"

namespace Zigurat 
{

  class ZiguratIPException : public ZiguratException
  {
  public:
    ZiguratIPException() = delete;
    ZiguratIPException(std::string msg) : ZiguratException(1200, msg) { }
  };

}

#endif // __ZIGURATIPEXCEPTION_HPP__

