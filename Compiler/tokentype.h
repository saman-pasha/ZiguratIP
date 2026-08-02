
#ifndef __TOKENTYPE_H__
#define __TOKENTYPE_H__

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

#endif // __TOKENTYPE_H__
