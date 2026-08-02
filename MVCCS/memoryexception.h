
#ifndef __MEMORYEXCEPTION_H__
#define __MEMORYEXCEPTION_H__

#include "zexception.h"

namespace Zigurat 
{

  class MemoryException : public ZiguratException
  {
  public:
    MemoryException(std::string msg) : ZiguratException(9390, msg) { }
  };

}

#endif // __MEMORYEXCEPTION_H__
