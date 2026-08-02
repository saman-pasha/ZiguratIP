
#ifndef __ZIGURATIPEXCEPTION_H__
#define __ZIGURATIPEXCEPTION_H__

#include "zexception.h"

namespace Zigurat 
{

  class ZiguratIPException : public ZiguratException
  {
  public:
    ZiguratIPException() = delete;
    ZiguratIPException(std::string msg) : ZiguratException(1200, msg) { }
  };

}

#endif // __ZIGURATIPEXCEPTION_H__

