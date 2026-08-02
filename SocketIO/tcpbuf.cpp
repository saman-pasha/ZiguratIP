#include "tcpbuf.h"
#include <cstring>


namespace Zigurat
{

  const int tcpbuf::BUFFER_SIZE = 2048;

  tcpbuf::tcpbuf()
    : socketbuf()
  {
    
  }

  tcpbuf::tcpbuf(tcpbuf&& other)
    : socketbuf(std::forward<socketbuf&&>(other))
  {

  }

  tcpbuf& tcpbuf::operator=(tcpbuf&& other)
  {
    socketbuf::operator=(std::forward<socketbuf&&>(other));
    return *this;
  }

  tcpbuf* tcpbuf::open(Socket::handle_t handle, bool blocking_mode, int timeout)
  {
    if (this->is_open()) this->close();

    this->_handle = handle;

    Socket::set_timeout(this->_handle, timeout);
    Socket::set_reusable(this->_handle, true);
    Socket::set_blocking_mode(this->_handle, blocking_mode);

    // Reapplying an existing buffer resets both areas, so a reopened
    // socket never sees bytes left over from the previous peer.
    if (this->_buffer == nullptr)
      this->setbuf(new char[BUFFER_SIZE], BUFFER_SIZE);
    else
      this->setbuf(this->_buffer, this->_length);

    return this;
  }

  tcpbuf* tcpbuf::open(std::string node, std::string service, bool blocking_mode, int timeout)
  {
    if (this->is_open()) this->close();

    Socket::tcp_address_info_t hints, *res = nullptr;

    std::memset(&hints, 0x00, sizeof(Socket::tcp_address_info_t));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_CANONNAME;

    // getaddrinfo reports EAI_* codes, not the -1 the socket calls use.
    int error_code = Socket::get_address_info(node.c_str(), service.c_str(), &hints, &res);
    if (error_code != 0 || res == nullptr) throw SocketIOException("tcp getting address information failed");

    // Try each candidate address in turn and keep the first that connects,
    // closing the ones that do not rather than leaking them.
    bool connected = false;
    for (Socket::tcp_address_info_t* cursor = res; cursor != nullptr; cursor = cursor->ai_next) {
      this->_handle = Socket::open(cursor->ai_family, cursor->ai_socktype, cursor->ai_protocol);
      if (this->_handle == Socket::INVALID_SOCKET) continue;

      if (Socket::connect(this->_handle, cursor->ai_addr, cursor->ai_addrlen) != Socket::SOCKET_ERROR) {
	connected = true;
	break;
      }

      Socket::close(this->_handle);
      this->_handle = Socket::INVALID_SOCKET;
    }

    Socket::free_address_info(res);

    if (!connected) throw SocketIOException("tcp connect failed");

    Socket::set_timeout(this->_handle, timeout);
    Socket::set_reusable(this->_handle, true);
    Socket::set_blocking_mode(this->_handle, blocking_mode);

    // Reapplying an existing buffer resets both areas, so a reopened
    // socket never sees bytes left over from the previous peer.
    if (this->_buffer == nullptr)
      this->setbuf(new char[BUFFER_SIZE], BUFFER_SIZE);
    else
      this->setbuf(this->_buffer, this->_length);

    return this;
  }

  bool tcpbuf::is_open() const
  {
    return Socket::is_open(this->_handle);
  }

  tcpbuf* tcpbuf::close()
  {
    Socket::close(this->_handle);
    return this;
  }

  tcpbuf::~tcpbuf()
  {
    if (this->is_open()) this->close();
  }

}
