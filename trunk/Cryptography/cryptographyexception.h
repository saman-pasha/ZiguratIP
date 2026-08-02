
#ifndef __CRYPTOGRAPHYEXCEPTION_H__
#define __CRYPTOGRAPHYEXCEPTION_H__

#include "zexception.h"

namespace Zigurat 
{

  class CryptographyException : public ZiguratException
  {
  public:
    CryptographyException() = delete;
    CryptographyException(std::string msg) : ZiguratException(6579, msg) { }
  };

}

#endif // __CRYPTOGRAPHYEXCEPTION_H__

