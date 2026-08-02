#include "tokenizeexception.h"
#include <sstream>


namespace Zigurat
{

  TokenizeException::TokenizeException(std::string message, int lineno, int colno, char character)
    : ZiguratException(2100, message)
  {
    std::stringstream ss;
    ss << " at line " << lineno;
    ss << " column " << colno;
    ss << " near '" << character << "'";
    this->_message += ss.str();
  }

}
