
#ifndef __HTTPSERVER_HPP__
#define __HTTPSERVER_HPP__

#include "tlsserver.hpp"
#include "binarystream.hpp"

namespace Zigurat
{

  class HTTPRequest;
  class HTTPResponse;

  class HTTPServer
  {
  protected:
    // A TLSServer is a TCPServer that also knows how to hand over a connection
    // whose peer has authenticated. Holding the derived one means the plain and
    // the secure listener are the same object, reached by different run calls.
    TLSServer _server;

  public:
    typedef std::function<void (binarystream*, HTTPRequest*, HTTPResponse*)> client_handler_t;

    HTTPServer();
    static void handle_client(binarystream&, client_handler_t, bool = true, int = 0, 
			      bool = true, size_t = 8000, size_t = 16000, size_t = 2000000000);
    virtual void run(TCPServer::Version, std::string, int, size_t, client_handler_t, bool = true, int = 0, 
		     bool = true, size_t = 8000, size_t = 16000, size_t = 2000000000);

    // The same server over authenticated, encrypted connections. Every request
    // is then made by a peer holding a certificate the configured authority
    // issued, and one that cannot show it never reaches the handler.
    virtual void run(const TLS::HandshakeParameters&, TCPServer::Version, std::string, int, size_t,
		     client_handler_t, bool = true, int = 0,
		     bool = true, size_t = 8000, size_t = 16000, size_t = 2000000000);
    virtual void shutdown(bool);
    virtual ~HTTPServer();
  };
	
}

#endif // __HTTPSERVER_HPP__
