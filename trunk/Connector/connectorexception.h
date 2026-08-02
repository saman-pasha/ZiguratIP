#include "zexception.h"
#include <sstream>


#ifndef __CONNECTOREXCEPTION_H__
#define __CONNECTOREXCEPTION_H__

namespace Zigurat 
{

  class ConnectorException : public ZiguratException
  {
  public:
    ConnectorException() = delete;
    ConnectorException(std::string msg) : ZiguratException(2077, msg) { }
  };

}

#endif // __CONNECTOREXCEPTION_H__
