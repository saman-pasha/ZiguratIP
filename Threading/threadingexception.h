#include <string>
#include "zexception.h"


#ifndef __THREADINGEXCEPTION_H__
#define __THREADINGEXCEPTION_H__

namespace Zigurat
{

  class ThreadingException : public ZiguratException
  {
  public:
    ThreadingException() = delete;
    ThreadingException(std::string msg) : ZiguratException(7219, msg) { }
  };

}

#endif // __THREADINGEXCEPTION_H__
