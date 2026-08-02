
#ifndef __HTTP_HPP__
#define __HTTP_HPP__

#include "basepage.hpp"
#include "httpserver.hpp"
#include "httprequest.hpp"
#include "httpresponse.hpp"
#include "httpexception.hpp"

namespace Zigurat
{
  typedef BasePage BasePage;
  typedef HTTPServer HTTPServer;
  typedef HTTPRequest HTTPRequest;
  typedef HTTPResponse HTTPResponse;
  typedef HTTPException HTTPException;
}

#endif // __HTTP_HPP__
