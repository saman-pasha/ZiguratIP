
#ifndef __ARGUMENTEXCEPTION_HPP__
#define __ARGUMENTEXCEPTION_HPP__

#include "zexception.hpp"
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

#endif // __ARGUMENTEXCEPTION_HPP__

