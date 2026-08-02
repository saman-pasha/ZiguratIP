#include <string>
#include "zexception.h"


#ifndef __STREAMIOEXCEPTION_H__
#define __STREAMIOEXCEPTION_H__

namespace Zigurat 
{

  class StreamIOException : public ZiguratException
  {
  public:
    StreamIOException() = delete;
    StreamIOException(std::string msg) : ZiguratException(5100, msg) { }
  };

}

#endif // __STREAMIOEXCEPTION_H__
