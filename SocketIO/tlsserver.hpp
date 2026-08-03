#ifndef __TLSSERVER_HPP__
#define __TLSSERVER_HPP__

#include "tcpserver.hpp"
#include "tlsstream.hpp"
#include "tls.hpp"

namespace Zigurat
{

  // The accepting half of a secure connection. There is no matching TLSClient,
  // for the same reason there is no TCPClient: tlsstream is the client, opened
  // against a host and a service, exactly as tcpstream is for a plain one.
  //
  // The listening socket is a TCPServer's -- accepting is accepting. What
  // differs is what happens to a handle once it arrives: it is wrapped in a
  // tlsstream and made to complete a handshake before the handler ever sees it,
  // so a handler is only ever given a connection whose peer has already proved
  // who it is.
  class TLSServer : public TCPServer
  {
  protected:
    TLS::HandshakeParameters _handshake_params;

  public:
    typedef std::function<void (tlsstream&)> secure_handler_t;

    TLSServer() = default;
    explicit TLSServer(const TLS::HandshakeParameters&);

    void credentials(const TLS::HandshakeParameters&);

    // A peer that cannot complete the handshake -- no certificate, one this
    // authority did not sign, a signature that does not check out -- is dropped,
    // and the server carries on serving everybody else.
    virtual void run(Version, std::string, int, size_t, secure_handler_t);

    // The inherited plaintext form, for anything that deliberately wants it.
    using TCPServer::run;

    virtual ~TLSServer() = default;
  };

}

#endif // __TLSSERVER_HPP__
