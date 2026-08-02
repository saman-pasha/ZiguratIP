#include "compileexception.h"


namespace Zigurat
{

  CompileException::CompileException(std::string message, const Expression& expr)
    : ZiguratException(2300, message)
  {
    std::stringstream ss;
    ss << " at line " << expr.token.line_no;
    ss << " column " << expr.token.column_no;
    if (expr.token.type == TokenType::STR)
      ss << " near '" << expr.token.value.substr(11, expr.token.value.size() - 21) << "'";      
    else
      ss << " near '" << expr.token.value << "'";
    this->_message += ss.str();
  }

}
