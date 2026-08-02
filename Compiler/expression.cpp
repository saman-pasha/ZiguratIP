#include "expression.h"


namespace Zigurat
{

  Expression::Expression(const Token& token)
    : token(token)
  {
    
  }

  Expression::Expression(Token&& token)
    : token(std::forward<Token&&>(token))
  {
    
  }

  Expression::~Expression()
  {
  
  }
	
}
