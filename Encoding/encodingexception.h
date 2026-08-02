
#ifndef __ENCODINGEXCEPTION_H__
#define __ENCODINGEXCEPTION_H__

#include "zexception.h"
#include <sstream>

namespace Zigurat 
{

  class EncodingException : public ZiguratException
  {
  public:
    EncodingException() = delete;
    EncodingException(std::string msg) : ZiguratException(3019, msg) { }
  };

}

#endif // __ENCODINGEXCEPTION_H__

