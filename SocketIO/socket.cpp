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

  void Socket::suppress_sigpipe(Socket::handle_t handle)
  {
#if defined(SO_NOSIGPIPE)
    int on = 1;
    Socket::set_option(handle, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
    (void)handle;   // Linux carries MSG_NOSIGNAL on the send instead
#endif
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

  // The value has to be an int. It was a bool, so the call went in with an
  // option length of one, and setsockopt rejects that for SO_REUSEADDR with
  // EINVAL -- the option was never actually set, on any socket, ever. Nobody
  // looked at the return value, so it failed in silence.
  //
  // What that cost: restarting the server while any connection to it was still
  // in TIME_WAIT failed with "Address already in use". Moving this call to
  // before bind, which it needed, was necessary and not sufficient.
  int Socket::set_reusable(Socket::handle_t handle, bool reusable)
  {
    const int value = (reusable) ? 1 : 0;
    return Socket::set_option(handle, SOL_SOCKET, SO_REUSEADDR, (const char*)&value, sizeof(value));
  }

  // NAGLE OFF, and it is a correctness-of-latency fix rather than a tuning
  // knob. Both protocols this server speaks are conversations of small
  // messages -- read a request, write a reply, wait for the next -- which is
  // precisely the pattern Nagle's algorithm coalesces and the peer's delayed
  // ACK then stalls. Neither side is at fault alone: Nagle holds a small write
  // until the previous one is acknowledged, delayed ACK holds the
  // acknowledgement for up to 40ms hoping to piggyback it on a reply, and the
  // reply is the write being held. Nothing is wrong, nothing times out, and
  // every exchange costs 40ms.
  //
  // What that cost here: one turn of a cocolog worker is a few dozen
  // exchanges, so twelve of them took a minute to do a second's work, and the
  // test that runs twelve at once failed on a timeout with no error anywhere
  // to say why. See doc/concurrency.md.
  int Socket::set_nodelay(Socket::handle_t handle, bool nodelay)
  {
    const int value = (nodelay) ? 1 : 0;
    return Socket::set_option(handle, IPPROTO_TCP, TCP_NODELAY, (const char*)&value, sizeof(value));
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
