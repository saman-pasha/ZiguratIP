#include <string>
#include "zexception.hpp"


#ifndef __THREADINGEXCEPTION_HPP__
#define __THREADINGEXCEPTION_HPP__

namespace Zigurat
{

  class ThreadingException : public ZiguratException
  {
  public:
    ThreadingException() = delete;
    ThreadingException(std::string msg) : ZiguratException(7219, msg) { }
  };

}

#endif // __THREADINGEXCEPTION_HPP__
