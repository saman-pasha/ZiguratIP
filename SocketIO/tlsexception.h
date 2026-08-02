#include "zexception.h"


#ifndef __TLSEXCEPTION_H__
#define __TLSEXCEPTION_H__

namespace Zigurat
{

  class TLSException : public ZiguratException
  {
  public:
    TLSException() = delete;
    TLSException(std::string msg) : ZiguratException(5215, msg) { }
  };

}

#endif // __TLSEXCEPTION_H__
