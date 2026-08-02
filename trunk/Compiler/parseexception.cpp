#include "parseexception.h"
#include <sstream>

namespace Zigurat
{

  ParseException::ParseException(std::string message)
    : ZiguratException(2200, message)
  {
    
  }
  
  ParseException::ParseException(std::string message, const Token& token)
    : ZiguratException(2200, message)
  {
    std::stringstream ss;
    ss << " at line " << token.line_no;
    ss << " column " << token.column_no;
    ss << " near '" << token.value << "'";
    this->_message += ss.str();
  }

}
