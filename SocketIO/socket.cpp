#include "socket.hpp"
#include <cstring>


namespace Zigurat
{

  const Socket::SocketStatic Socket::__static__;
    
  Socket::SocketStatic::SocketStatic()
  {
#if defined(_WIN32) || defined(_WIN64)	
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
      throw SocketIOException("winsock startup failed");
#endif
  }

  Socket::SocketStatic::~SocketStatic() 
  {
#if defined(_WIN32) || defined(_WIN64)	
    WSACleanup();
#endif
  }

  int Socket::error_code()
  {
    return errno;
  }

  bool Socket::is_open(Socket::handle_t handle)
  {
    int error = 0;
    socklen_t length = sizeof(error);
    int check = Socket::get_option(handle, SOL_SOCKET, SO_ERROR, &error, &length);
    return (check == 0 && error == 0) ? true : false;
  }

  bool Socket::input_available(Socket::handle_t handle)
  {
    Socket::poll_info_t info[1];
    std::memset(&info, 0x00, sizeof(Socket::poll_info_t));
    info[0].fd = handle;
    info[0].events = Socket::PollEvent::IN;
    int count = Socket::poll(info, 1, 0);
    if (count == Socket::SOCKET_ERROR)
      throw SocketIOException("socket poll failed");
    else if (count == 0)
      return false;
    else if (info[0].revents & Socket::PollEvent::IN)
      return true;
    else
      return false;
  }

  int Socket::set_reusable(Socket::handle_t handle, bool reusable)
  {
    return Socket::set_option(handle, SOL_SOCKET,  SO_REUSEADDR, (char *)&reusable, sizeof(reusable));
  }

  // The timeout is in seconds, matching /SERVER/TIMEOUT and /HTTP/TIMEOUT in
  // the configuration file. Zero leaves the socket waiting indefinitely.
  int Socket::set_timeout(Socket::handle_t handle, int timeout)
  {
#if defined(_WIN32) || defined(_WIN64)
    DWORD tv = timeout * 1000;  // Winsock counts milliseconds
    int result = Socket::set_option(handle, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    if (result == Socket::SOCKET_ERROR) return result;
    return Socket::set_option(handle, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    int result = Socket::set_option(handle, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));
    if (result == Socket::SOCKET_ERROR) return result;
    return Socket::set_option(handle, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(struct timeval));
#endif
  }

  int Socket::set_blocking_mode(Socket::handle_t handle, bool blocking_mode)
  {
#if defined(_WIN32) || defined(_WIN64)
    int mode = (blocking_mode) ? 0 : 1;
    return ioctlsocket(handle, FIONBIO, &mode);
#else
    // Clearing the flag, not toggling it: XOR turned an already blocking socket
    // into a non-blocking one.
#if defined(sun) || defined(__sun)
    const int NONBLOCK_FLAG = FNDELAY;
#else
    const int NONBLOCK_FLAG = O_NONBLOCK;
#endif
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags == -1) return Socket::SOCKET_ERROR;
    return fcntl(handle, F_SETFL, (blocking_mode) ? (flags & ~NONBLOCK_FLAG) : (flags | NONBLOCK_FLAG));
#endif
  }

}
