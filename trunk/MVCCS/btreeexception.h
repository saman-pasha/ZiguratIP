
#ifndef __BTREEEXCEPTION_H__
#define __BTREEEXCEPTION_H__

#include "zexception.h"
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

#endif // __BTREEEXCEPTION_H__

