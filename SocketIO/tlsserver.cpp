#include "tlsserver.hpp"
#include "tlsexception.hpp"


namespace Zigurat
{

  TLSServer::TLSServer(const TLS::HandshakeParameters& params)
    : _handshake_params(params)
  {

  }

  void TLSServer::credentials(const TLS::HandshakeParameters& params)
  {
    this->_handshake_params = params;
  }

  void TLSServer::run(Version version, std::string service, int backlog, size_t pool_size,
		      secure_handler_t handler)
  {
    TLS::HandshakeParameters params = this->_handshake_params;

    TCPServer::run(version, service, backlog, pool_size,
		   [params, handler] (Socket::handle_t handle) -> void {

		     tlsstream stream;

		     try {
		       stream.open(TLS::ConnectionEnd::SERVER, params, handle, true, 0);
		     } catch (...) {
		       // The peer did not get in. Whatever the reason -- and the
		       // handshake has already told it which -- this connection is
		       // over, and it is not the server's problem beyond closing.
		       stream.close();
		       return;
		     }

		     handler(stream);

		     stream.close();
		   });
  }

}
