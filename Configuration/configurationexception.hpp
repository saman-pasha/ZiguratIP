
#ifndef __CONFIGURATIONEXCEPTION_HPP__
#define __CONFIGURATIONEXCEPTION_HPP__

#include "zexception.hpp"
#include <sstream>

namespace Zigurat 
{

  class ConfigurationException : public ZiguratException
  {
  public:
    ConfigurationException() = delete;
    ConfigurationException(std::string);
    ConfigurationException(std::string, int, std::string);
  };

}

#endif // __CONFIGURATIONEXCEPTION_HPP__

