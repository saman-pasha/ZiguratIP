
#ifndef __TOKENIZEEXCEPTION_HPP__
#define __TOKENIZEEXCEPTION_HPP__

#include "zexception.hpp"

namespace Zigurat 
{

  class TokenizeException : public ZiguratException
  {
  public:
    TokenizeException() = delete;
    TokenizeException(std::string, int, int, char);
  };

}

#endif // __TOKENIZEEXCEPTION_HPP__
