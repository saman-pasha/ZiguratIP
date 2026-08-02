
#ifndef __TOKEN_H__
#define __TOKEN_H__

#include "tokentype.h"
#include <string>

namespace Zigurat
{

  class Token
  {
  public:
    TokenType type;
    std::string value;
    int line_no;
    int column_no;
    inline Token(TokenType type, std::string value, int line_no, int column_no)
      : type(type), value(value), line_no(line_no), column_no(column_no) { }
  };

}

#endif // __TOKEN_H__
