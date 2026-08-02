
#ifndef __TOKENIZEEXCEPTION_H__
#define __TOKENIZEEXCEPTION_H__

#include "zexception.h"

namespace Zigurat 
{

  class TokenizeException : public ZiguratException
  {
  public:
    TokenizeException() = delete;
    TokenizeException(std::string, int, int, char);
  };

}

#endif // __TOKENIZEEXCEPTION_H__
