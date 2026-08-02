
#ifndef __HTTP_H__
#define __HTTP_H__

#include "basepage.h"
#include "httpserver.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "httpexception.h"

namespace Zigurat
{
  typedef BasePage BasePage;
  typedef HTTPServer HTTPServer;
  typedef HTTPRequest HTTPRequest;
  typedef HTTPResponse HTTPResponse;
  typedef HTTPException HTTPException;
}

#endif // __HTTP_H__
