
#ifndef __PARSIEXCEPTION_HPP__
#define __PARSIEXCEPTION_HPP__

#include "zexception.hpp"

namespace Zigurat 
{

  class ParsiException : public ZiguratException
  {
  public:
    ParsiException() = delete;
    ParsiException(std::string msg) : ZiguratException(1100, msg) { }
  };

}

#endif // __PARSIEXCEPTION_HPP__

