#include "ipcserver.hpp"
#include <cstring>


namespace Zigurat
{

  IPCServer::IPCServer()
    : _shutdown(false)
  {

  }

  void IPCServer::run(std::string server_path, int backlog, size_t pool_size, client_handler_t handler)
  {
    if (!this->_shutdown) throw SocketIOException("ipc server is running");
    this->_shutdown = false;
    this->_pool.charge(pool_size);
    this->_pool.limit(pool_size * 4);   // see the note in tcpserver.cpp

    Socket::ipc_address_t server_address;

    std::memset(&server_address, 0x00, sizeof(Socket::ipc_address_t));

    this->_handle = Socket::open(AF_UNIX, SOCK_STREAM, 0);
    if (this->_handle == Socket::INVALID_SOCKET) throw SocketIOException("ipc socket open failed");
      
    server_address.sun_family = AF_UNIX;
    std::strcpy(server_address.sun_path, server_path.c_str());

    Socket::unlink(server_path.c_str());

    int error_code = Socket::bind(this->_handle, (Socket::address_t*)&server_address, sizeof(Socket::ipc_address_t));
    if (error_code == Socket::SOCKET_ERROR) throw SocketIOException("ipc bind failed");    

    error_code = Socket::listen(this->_handle, backlog);
    if (error_code == Socket::SOCKET_ERROR) throw SocketIOException("ipc listen failed");    
    
    Socket::set_reusable(this->_handle, true);
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
	throw SocketIOException("ipc poll failed");
      else if (count == 0)
	continue;
      else {
	if (info[0].revents & Socket::PollEvent::IN) {
	  Socket::handle_t client_handle = Socket::accept(this->_handle, NULL, NULL);
	  try {
	    this->_pool.execute([handler, client_handle] () { handler(client_handle); });
	  } catch (const std::exception&) {
	    Socket::close(client_handle);   // see the note in tcpserver.cpp
	  }
	} else {
	  throw SocketIOException("ipc poll failed");
	}
      }
    }
  }

  void IPCServer::shutdown(bool force)
  {
    this->_shutdown = true;
    Socket::shutdown(this->_handle, Socket::Shutdown::RECEIVE);
    this->_pool.discharge(force);
    Socket::shutdown(this->_handle, Socket::Shutdown::SEND);
  }

  IPCServer::~IPCServer()
  {
    if (!this->_shutdown) this->shutdown(true);
    Socket::close(this->_handle);
  }
	
}

