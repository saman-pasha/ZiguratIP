
#ifndef __TOKENTYPE_HPP__
#define __TOKENTYPE_HPP__

namespace Zigurat
{

  enum class TokenType
  {
    BOOL,
    INT,
    FLOAT,
    STR,
    NAME,
    OP,
    LPAR,
    RPAR,
    EOF_
  };

}

#endif // __TOKENTYPE_HPP__
