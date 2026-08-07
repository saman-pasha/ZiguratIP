
#ifndef __SOCKETBUF_HPP__
#define __SOCKETBUF_HPP__

#include <streambuf>
#include "socket.hpp"

namespace Zigurat
{

  class socketbuf : public std::basic_streambuf<char>
  {
  protected:
    Socket::handle_t _handle;
    char_type       *_buffer;   // owned: released by setbuf()/the destructor
    std::streamsize  _length;
    char_type       *_gbuffer;  // receive half of _buffer
    std::streamsize  _gsize;
    char_type       *_pbuffer;  // send half of _buffer
    std::streamsize  _psize;
    socketbuf();

    // Drains the put area to the socket. Returns false on a socket error.
    bool flush_put_area();

  public:
    // The descriptor itself. OpenSSL is given the socket directly through a
    // BIO, so the TLS buffer needs to know which one it is; nothing else here
    // hands it out, and nothing else should.
    Socket::handle_t handle() const;

  public:    
    socketbuf(const socketbuf&) = delete;
    socketbuf(socketbuf&&);

    socketbuf& operator=(const socketbuf&) = delete;
    socketbuf& operator=(socketbuf&&);

    virtual socketbuf* setbuf(char_type*, std::streamsize) override;
    virtual pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;
    virtual pos_type seekpos(pos_type, std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;
    virtual int sync() override;
    virtual std::streamsize showmanyc() override;
    virtual int_type underflow() override;
    virtual int_type overflow(int_type = traits_type::eof()) override;
    virtual int_type pbackfail(int_type = traits_type::eof()) override;

    virtual ~socketbuf();
  };

}

#endif // __SOCKETBUF_HPP__
