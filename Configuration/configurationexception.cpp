#include "configurationexception.h"
#include <tuple>


namespace Zigurat
{

  ConfigurationException::ConfigurationException(std::string message)
    : ZiguratException(9200, message)
  {

  }

  ConfigurationException::ConfigurationException(std::string message, int line_no, std::string line)
    : ZiguratException(9200, message)
  {
    this->_message = this->_message + " at line " + std::to_string(line_no) + ", '" + line + "'";
  }

}
