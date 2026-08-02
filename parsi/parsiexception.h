
#ifndef __PARSIEXCEPTION_H__
#define __PARSIEXCEPTION_H__

#include "zexception.h"

namespace Zigurat 
{

  class ParsiException : public ZiguratException
  {
  public:
    ParsiException() = delete;
    ParsiException(std::string msg) : ZiguratException(1100, msg) { }
  };

}

#endif // __PARSIEXCEPTION_H__

