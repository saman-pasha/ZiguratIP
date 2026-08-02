
#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include "tcpserver.h"
#include "binarystream.h"

namespace Zigurat
{

  class HTTPRequest;
  class HTTPResponse;

  class HTTPServer
  {
  protected:
    TCPServer _server;

  public:
    typedef std::function<void (binarystream*, HTTPRequest*, HTTPResponse*)> client_handler_t;

    HTTPServer();
    static void handle_client(binarystream&, client_handler_t, bool = true, int = 0, 
			      bool = true, size_t = 8000, size_t = 16000, size_t = 2000000000);
    virtual void run(TCPServer::Version, std::string, int, size_t, client_handler_t, bool = true, int = 0, 
		     bool = true, size_t = 8000, size_t = 16000, size_t = 2000000000);
    virtual void shutdown(bool);
    virtual ~HTTPServer();
  };
	
}

#endif // __HTTPSERVER_H__
