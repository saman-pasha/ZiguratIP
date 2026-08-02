#include "zexception.hpp"
#include <sstream>


#ifndef __CONNECTOREXCEPTION_HPP__
#define __CONNECTOREXCEPTION_HPP__

namespace Zigurat 
{

  class ConnectorException : public ZiguratException
  {
  public:
    ConnectorException() = delete;
    ConnectorException(std::string msg) : ZiguratException(2077, msg) { }
  };

}

#endif // __CONNECTOREXCEPTION_HPP__
