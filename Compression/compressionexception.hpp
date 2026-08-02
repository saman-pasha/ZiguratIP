
#ifndef __COMPRESSIONEXCEPTION_HPP__
#define __COMPRESSIONEXCEPTION_HPP__

#include "zexception.hpp"

namespace Zigurat 
{

  class CompressionException : public ZiguratException
  {
  public:
    CompressionException() = delete;
    CompressionException(std::string msg) : ZiguratException(2035, msg) { }
  };

}

#endif // __CERTIFICATIONEXCEPTION_HPP__

