
#ifndef __PARSEEXCEPTION_H__
#define __PARSEEXCEPTION_H__

#include "zexception.h"
#include "token.h"
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

#endif // __PARSEEXCEPTION_H__

