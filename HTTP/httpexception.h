
#ifndef __HTTPEXCEPTION_H__
#define __HTTPEXCEPTION_H__

#include <string>
#include "zexception.h"

namespace Zigurat
{

  class HTTPException : public ZiguratException
  {
  public:
    HTTPException() = delete;
    HTTPException(std::string);
  };

}

#endif // __HTTPEXCEPTION_H__
