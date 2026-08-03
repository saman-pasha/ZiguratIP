#include "ipcbuf.hpp"
#include <cstring>


namespace Zigurat
{

  const int ipcbuf::BUFFER_SIZE = 2048;

  ipcbuf::ipcbuf()
    : socketbuf()
  {
    
  }

  ipcbuf::ipcbuf(ipcbuf&& other)
    : socketbuf(std::forward<socketbuf&&>(other))
  {

  }

  ipcbuf& ipcbuf::operator=(ipcbuf&& other)
  {
    socketbuf::operator=(std::forward<socketbuf&&>(other));
    return *this;
  }

  ipcbuf* ipcbuf::open(Socket::handle_t handle, bool blocking_mode, int timeout)
  {
    if (this->is_open()) this->close();

    this->_handle = handle;

    Socket::suppress_sigpipe(this->_handle);
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

  ipcbuf* ipcbuf::open(std::string server_path, std::string client_path, bool blocking_mode, int timeout)
  {
#if defined(_WIN32) || defined(_WIN64)
#else 
   if (this->is_open()) this->close();

    Socket::ipc_address_t server_address, client_address;

    std::memset(&server_address, 0x00, sizeof(Socket::ipc_address_t));
    std::memset(&client_address, 0x00, sizeof(Socket::ipc_address_t));

    this->_handle = Socket::open(AF_UNIX, SOCK_STREAM, 0);
    if (this->_handle == Socket::INVALID_SOCKET) throw SocketIOException("ipc socket open failed");
      
    client_address.sun_family = AF_UNIX;
    std::strcpy(client_address.sun_path, client_path.c_str());

    Socket::unlink(client_path.c_str());

    int error_code = Socket::bind(this->_handle, (Socket::address_t*)&client_address, sizeof(Socket::ipc_address_t));
    if (error_code == Socket::SOCKET_ERROR) throw SocketIOException("ipc bind failed");    

    server_address.sun_family = AF_UNIX;
    std::strcpy(server_address.sun_path, server_path.c_str());

    error_code = Socket::connect(this->_handle, (Socket::address_t*)&server_address, sizeof(Socket::ipc_address_t));
    if (error_code == Socket::SOCKET_ERROR) throw SocketIOException("ipc connect failed");    

    Socket::suppress_sigpipe(this->_handle);
    Socket::set_timeout(this->_handle, timeout);
    Socket::set_blocking_mode(this->_handle, blocking_mode);

    // Reapplying an existing buffer resets both areas, so a reopened
    // socket never sees bytes left over from the previous peer.
    if (this->_buffer == nullptr)
      this->setbuf(new char[BUFFER_SIZE], BUFFER_SIZE);
    else
      this->setbuf(this->_buffer, this->_length);
#endif
    return this;
  }

  bool ipcbuf::is_open() const
  {
    return Socket::is_open(this->_handle);
  }

  ipcbuf* ipcbuf::close()
  {
    Socket::close(this->_handle);
    return this;
  }

  ipcbuf::~ipcbuf()
  {
    if (this->is_open()) this->close();
  }

}
