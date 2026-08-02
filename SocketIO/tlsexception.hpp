#include "zexception.hpp"


#ifndef __TLSEXCEPTION_HPP__
#define __TLSEXCEPTION_HPP__

namespace Zigurat
{

  class TLSException : public ZiguratException
  {
  public:
    TLSException() = delete;
    TLSException(std::string msg) : ZiguratException(5215, msg) { }
  };

}

#endif // __TLSEXCEPTION_HPP__
