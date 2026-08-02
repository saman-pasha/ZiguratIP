
#ifndef __PARSEEXCEPTION_HPP__
#define __PARSEEXCEPTION_HPP__

#include "zexception.hpp"
#include "token.hpp"
#include <string>

namespace Zigurat 
{

  class ParseException : public ZiguratException
  {
  public:
    ParseException() = delete;
    ParseException(std::string);
    ParseException(std::string, const Token&);
  };

}

#endif // __PARSEEXCEPTION_HPP__

