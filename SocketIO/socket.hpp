
#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

#include <cstdlib>
#include <cstdio>
#if defined(_WIN32) || defined(_WIN64)
#undef UNICODE
#include <winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <ioctl.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <errno.h>
#endif
#include "socketioexception.hpp"

namespace Zigurat
{
  
  class Socket
  {
  public:
#if defined(_WIN32) || defined(_WIN64)
    typedef SOCKET      handle_t;
#else
    typedef int         handle_t;
    typedef sockaddr_un ipc_address_t;
#endif
    typedef sockaddr    address_t;
    typedef sockaddr_in tcp_address_t;
    typedef addrinfo    tcp_address_info_t;
    typedef pollfd      poll_info_t;

#if defined(_WIN32) || defined(_WIN64)
    static const handle_t INVALID_SOCKET = ::INVALID_SOCKET;
    static const int      SOCKET_ERROR   = ::SOCKET_ERROR;
#else
    static const handle_t INVALID_SOCKET = -1;
    static const int      SOCKET_ERROR   = -1;
#endif

    static constexpr auto get_address_info  = &::getaddrinfo;
    static constexpr auto free_address_info = &::freeaddrinfo;

    static constexpr auto open     = &::socket;
    static constexpr auto connect  = &::connect;
    static constexpr auto receive  = &::recv;
    static constexpr auto send     = &::send;
    static constexpr auto bind     = &::bind;
    static constexpr auto listen   = &::listen;
    static constexpr auto accept   = &::accept;
    static constexpr auto select   = &::select;
    static constexpr auto shutdown = &::shutdown;
#if defined(_WIN32) || defined(_WIN64)
    static constexpr auto poll       = &::WSAPoll;
    static constexpr auto close      = &::closesocket;
    static constexpr auto io_control = &::ioctlsocket;
#else
    static constexpr auto poll       = &::poll;
    static constexpr auto close      = &::close;
    static constexpr auto io_control = &::ioctl;
    static constexpr auto unlink     = &::unlink;
#endif

    static constexpr auto get_option = &::getsockopt;
    static constexpr auto set_option = &::setsockopt;

    // Writing to a socket whose peer has hung up raises SIGPIPE, and the default
    // disposition kills the process. main_ziguratip ignores the signal, so the
    // server survives; nothing else did, so the connector, the test binary and
    // anybody's own client died the moment the far end went away. Suppressing it
    // is the socket layer's job, not every caller's.
    //
    // Two mechanisms, because no single one is portable: BSD and Darwin take
    // SO_NOSIGPIPE on the socket, Linux takes MSG_NOSIGNAL on the send.
#if defined(MSG_NOSIGNAL)
    static const int SEND_FLAGS = MSG_NOSIGNAL;
#else
    static const int SEND_FLAGS = 0;
#endif

    static void suppress_sigpipe(handle_t);

    static int  error_code();
    static bool is_open(handle_t);
    static bool input_available(handle_t);
    static int  set_timeout(handle_t, int);
    static int  set_reusable(handle_t, bool);
    static int  set_blocking_mode(handle_t, bool);

    enum Error {
      ACCES       = EACCES, 
      ADDRINUSE   = EADDRINUSE,
      WOULDBLOCK  = EWOULDBLOCK,
      BADF        = EBADF,
      CONNREFUSED = ECONNREFUSED,
      FAULT       = EFAULT,
      INTR        = EINTR,
      INVAL       = EINVAL,
      NOTCONN     = ENOTCONN,
      NOTSOCK     = ENOTSOCK 
    };

    enum PollEvent {
      IN   = POLLIN,
      OUT  = POLLOUT,
      ERR  = POLLERR,
      HUP  = POLLHUP,
      NVAL = POLLNVAL
    };

    enum Shutdown {
      RECEIVE,
      SEND,
      BOTH
    };

  private:
    class SocketStatic
    {
    public:
      SocketStatic();
      virtual ~SocketStatic();
    };
    static const SocketStatic __static__;
  };

}

#endif // __SOCKET_HPP__
