
#ifndef __EXPRESSION_H__
#define __EXPRESSION_H__

#include "token.h"
#include <string>
#include <vector>

namespace Zigurat
{

  class Expression
  {
  public:
    Token token;
    std::vector<Expression> args;
    Expression(const Token&);
    Expression(Token&&);
    virtual ~Expression();
  };

}

#endif // __EXPRESSION_H__
