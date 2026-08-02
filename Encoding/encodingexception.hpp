
#ifndef __ENCODINGEXCEPTION_HPP__
#define __ENCODINGEXCEPTION_HPP__

#include "zexception.hpp"
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

#endif // __ENCODINGEXCEPTION_HPP__

