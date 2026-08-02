#include <string>
#include "zexception.h"


#ifndef __ARITHMETICEXCEPTION_H__
#define __ARITHMETICEXCEPTION_H__

namespace Zigurat 
{

  class ArithmeticException : public ZiguratException
  {
  public:
    ArithmeticException() = delete;
    ArithmeticException(std::string msg) : ZiguratException(5211, msg) { }
  };

}

#endif // __ARITHMETICEXCEPTION_H__
