#include <string>
#include "zexception.hpp"


#ifndef __ARITHMETICEXCEPTION_HPP__
#define __ARITHMETICEXCEPTION_HPP__

namespace Zigurat 
{

  class ArithmeticException : public ZiguratException
  {
  public:
    ArithmeticException() = delete;
    ArithmeticException(std::string msg) : ZiguratException(5211, msg) { }
  };

}

#endif // __ARITHMETICEXCEPTION_HPP__
