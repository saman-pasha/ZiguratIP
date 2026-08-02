
#ifndef __ZIGURATEXCEPTION_H__
#define __ZIGURATEXCEPTION_H__

#include <string>
#include <exception>

namespace Zigurat 
{

  class ZiguratException : public std::exception
  {
  protected:
    int _code;
    std::string _message;

  public:
    ZiguratException() = delete;
    ZiguratException(int, std::string);
    virtual const char* what() const noexcept;
    virtual int code() const noexcept;
    virtual const std::string message() const noexcept;
  };

}

#endif // __ZIGURATEXCEPTION_H__
