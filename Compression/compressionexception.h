
#ifndef __COMPRESSIONEXCEPTION_H__
#define __COMPRESSIONEXCEPTION_H__

#include "zexception.h"

namespace Zigurat 
{

  class CompressionException : public ZiguratException
  {
  public:
    CompressionException() = delete;
    CompressionException(std::string msg) : ZiguratException(2035, msg) { }
  };

}

#endif // __CERTIFICATIONEXCEPTION_H__

