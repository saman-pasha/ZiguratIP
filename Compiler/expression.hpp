
#ifndef __EXPRESSION_HPP__
#define __EXPRESSION_HPP__

#include "token.hpp"
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

#endif // __EXPRESSION_HPP__
