#include "zexception.hpp"
#include <sstream>


namespace Zigurat
{

  ZiguratException::ZiguratException(int code, std::string message)
  {
    this->_code = code;
    this->_message = message;
  }

  const char* ZiguratException::what() const noexcept
  {
    return this->_message.c_str();
  }

  int ZiguratException::code() const noexcept
  {
    return this->_code;
  }

  const std::string ZiguratException::message() const noexcept
  {
    return this->_message;
  }

}
