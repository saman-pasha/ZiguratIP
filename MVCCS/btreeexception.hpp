
#ifndef __BTREEEXCEPTION_HPP__
#define __BTREEEXCEPTION_HPP__

#include "zexception.hpp"
#include <sstream>

namespace Zigurat
{

  class BTreeException : public ZiguratException
  {
  public:
    BTreeException() = delete;
    BTreeException(std::string msg) : ZiguratException(8710, msg) { }
  };

}

#endif // __BTREEEXCEPTION_HPP__

