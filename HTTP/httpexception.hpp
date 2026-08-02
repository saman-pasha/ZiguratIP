
#ifndef __HTTPEXCEPTION_HPP__
#define __HTTPEXCEPTION_HPP__

#include <string>
#include "zexception.hpp"

namespace Zigurat
{

  class HTTPException : public ZiguratException
  {
  public:
    HTTPException() = delete;
    HTTPException(std::string);
  };

}

#endif // __HTTPEXCEPTION_HPP__
