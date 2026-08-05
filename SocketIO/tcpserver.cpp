#include "tcpserver.hpp"
#include <cstring>


namespace Zigurat
{

  TCPServer::TCPServer()
    : _shutdown(true)
  {

  }

  void TCPServer::run(Version version, std::string service, int backlog, size_t pool_size, client_handler_t handler)
  {
    if (!this->_shutdown) throw SocketIOException("tcp server is running");
    this->_shutdown = false;
    this->_pool.charge(pool_size);

    // A worker is held for a whole connection, so a queued job is a client
    // waiting for somebody else to hang up. Allowing a backlog of four times
    // the workers keeps a burst from being refused while it is still likely to
    // be served; past that, refusing is faster and kinder than accepting.
    this->_pool.limit(pool_size * 4);

    Socket::tcp_address_info_t hints, *res;

    std::memset(&hints, 0x00, sizeof(hints));
    hints.ai_family   = (version == Version::IPV4) ? AF_INET : AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags    = AI_PASSIVE;

    int error_code = Socket::get_address_info(NULL, service.c_str(), &hints, &res);
    if (error_code == Socket::SOCKET_ERROR || !res) throw SocketIOException("tcp getting address information failed");

    while (res) {
      this->_handle = Socket::open(res->ai_family, res->ai_socktype, res->ai_protocol);
      if (this->_handle == Socket::INVALID_SOCKET) {
	if (res->ai_next == NULL) 
	  throw SocketIOException("tcp socket open failed");
	res = res->ai_next;
      } else {
	break;
      }
    }

    // Before bind, not after: SO_REUSEADDR only has any effect on the bind that
    // follows it. Setting it afterwards meant a restart inside the TIME_WAIT
    // window failed with "tcp bind failed".
    if (Socket::set_reusable(this->_handle, true) == Socket::SOCKET_ERROR)
      throw SocketIOException(std::string("tcp cannot make the socket reusable: ")
			      + std::strerror(Socket::error_code()));

    // Which port, and why. "tcp bind failed" on its own tells an operator
    // nothing about whether the address is taken, privileged, or unavailable.
    error_code = Socket::bind(this->_handle, res->ai_addr, res->ai_addrlen);
    if (error_code == Socket::SOCKET_ERROR)
      throw SocketIOException("tcp bind failed on service '" + service + "': "
			      + std::strerror(Socket::error_code()));

    error_code = Socket::listen(this->_handle, backlog);
    if (error_code == Socket::SOCKET_ERROR) throw SocketIOException("tcp listen failed");

    Socket::free_address_info(res);

    Socket::set_blocking_mode(this->_handle, true);

    Socket::poll_info_t info[1];
    std::memset(&info, 0x00, sizeof(info));
    info[0].fd = this->_handle;
    info[0].events = Socket::PollEvent::IN;

    int count = 0;
    while (!this->_shutdown) {
      info[0].revents = 0;
      count = Socket::poll(info, 1, 10000);

      if (count == Socket::SOCKET_ERROR)
	throw SocketIOException("tcp poll failed");
      else if (count == 0)
	continue;
      else {
	if (info[0].revents & Socket::PollEvent::IN) {
	  Socket::handle_t client_handle = Socket::accept(this->_handle, NULL, NULL);
	  try {
	    this->_pool.execute([handler, client_handle] () { handler(client_handle); });
	  } catch (const std::exception&) {
	    // Every worker is busy and the queue is at its limit. Close this one
	    // now: accepting a connection and then holding it in a queue that is
	    // already too long serves nobody, and the descriptor would be lost
	    // here rather than closed by the handler that never ran.
	    Socket::close(client_handle);
	  }
	} else {
	  throw SocketIOException("tcp poll failed");
	}
      }
    }
  }

  void TCPServer::shutdown(bool force)
  {
    this->_shutdown = true;
    Socket::shutdown(this->_handle, Socket::Shutdown::RECEIVE);
    this->_pool.discharge(force);
    Socket::shutdown(this->_handle, Socket::Shutdown::SEND);
  }

  TCPServer::~TCPServer()
  {
    if (!this->_shutdown) this->shutdown(true);
    Socket::close(this->_handle);
  }
	
}

