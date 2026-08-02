
#ifndef __INDEXEXCEPTION_HPP__
#define __INDEXEXCEPTION_HPP__

#include "zexception.hpp"

namespace Zigurat 
{

  class IndexException : public ZiguratException
  {
  public:
    IndexException(std::string msg) : ZiguratException(1063, msg) { }
  };

}

#endif // __INDEXEXCEPTION_HPP__
