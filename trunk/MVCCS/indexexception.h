
#ifndef __INDEXEXCEPTION_H__
#define __INDEXEXCEPTION_H__

#include "zexception.h"

namespace Zigurat 
{

  class IndexException : public ZiguratException
  {
  public:
    IndexException(std::string msg) : ZiguratException(1063, msg) { }
  };

}

#endif // __INDEXEXCEPTION_H__
