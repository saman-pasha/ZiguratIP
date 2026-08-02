
#ifndef __CONFIGURATIONEXCEPTION_H__
#define __CONFIGURATIONEXCEPTION_H__

#include "zexception.h"
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

#endif // __CONFIGURATIONEXCEPTION_H__

