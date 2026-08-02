
#ifndef __MEMORYEXCEPTION_HPP__
#define __MEMORYEXCEPTION_HPP__

#include "zexception.hpp"

namespace Zigurat 
{

  class MemoryException : public ZiguratException
  {
  public:
    MemoryException(std::string msg) : ZiguratException(9390, msg) { }
  };

}

#endif // __MEMORYEXCEPTION_HPP__
