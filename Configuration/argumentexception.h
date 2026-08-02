
#ifndef __ARGUMENTEXCEPTION_H__
#define __ARGUMENTEXCEPTION_H__

#include "zexception.h"
#include <sstream>

namespace Zigurat 
{

  class ArgumentException : public ZiguratException
  {
  public:
    ArgumentException() = delete;
    ArgumentException(std::string msg) : ZiguratException(9100, msg) { }
  };

}

#endif // __ARGUMENTEXCEPTION_H__

