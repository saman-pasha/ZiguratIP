
#ifndef __CRYPTOGRAPHYEXCEPTION_HPP__
#define __CRYPTOGRAPHYEXCEPTION_HPP__

#include "zexception.hpp"

namespace Zigurat 
{

  class CryptographyException : public ZiguratException
  {
  public:
    CryptographyException() = delete;
    CryptographyException(std::string msg) : ZiguratException(6579, msg) { }
  };

}

#endif // __CRYPTOGRAPHYEXCEPTION_HPP__

