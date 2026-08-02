#include "socketbuf.h"
#include <cstring>
#include <cerrno>


namespace Zigurat
{

  // The buffer handed to setbuf() is split in two: the first half receives, the
  // second half sends. A socket is full duplex, so the two areas cannot share
  // storage the way a file buffer's can.

  socketbuf::socketbuf()
    : _handle(Socket::INVALID_SOCKET), _buffer(nullptr), _length(0),
      _gbuffer(nullptr), _gsize(0), _pbuffer(nullptr), _psize(0)
  {
    this->setg(nullptr, nullptr, nullptr);
    this->setp(nullptr, nullptr);
  }

  socketbuf::socketbuf(socketbuf&& other)
    : _handle(other._handle), _buffer(other._buffer), _length(other._length),
      _gbuffer(other._gbuffer), _gsize(other._gsize),
      _pbuffer(other._pbuffer), _psize(other._psize)
  {
    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pbase(), other.epptr());
    this->pbump((int)(other.pptr() - other.pbase()));

    other._handle = Socket::INVALID_SOCKET;
    other._buffer = nullptr;
    other._length = 0;
    other._gbuffer = nullptr;
    other._gsize = 0;
    other._pbuffer = nullptr;
    other._psize = 0;
    other.setg(nullptr, nullptr, nullptr);
    other.setp(nullptr, nullptr);
  }

  socketbuf& socketbuf::operator=(socketbuf&& other)
  {
    if (this == &other) return *this;

    if (this->_buffer != nullptr) delete[] this->_buffer;

    this->_handle = other._handle;
    this->_buffer = other._buffer;
    this->_length = other._length;
    this->_gbuffer = other._gbuffer;
    this->_gsize = other._gsize;
    this->_pbuffer = other._pbuffer;
    this->_psize = other._psize;

    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pbase(), other.epptr());
    this->pbump((int)(other.pptr() - other.pbase()));

    other._handle = Socket::INVALID_SOCKET;
    other._buffer = nullptr;
    other._length = 0;
    other._gbuffer = nullptr;
    other._gsize = 0;
    other._pbuffer = nullptr;
    other._psize = 0;
    other.setg(nullptr, nullptr, nullptr);
    other.setp(nullptr, nullptr);

    return *this;
  }

  socketbuf* socketbuf::setbuf(char_type* s, std::streamsize n)
  {
    if (this->_buffer != nullptr && this->_buffer != s) delete[] this->_buffer;

    if (s == nullptr || n < 2) {
      this->_buffer = nullptr;
      this->_length = 0;
      this->_gbuffer = nullptr;
      this->_gsize = 0;
      this->_pbuffer = nullptr;
      this->_psize = 0;
      this->setg(nullptr, nullptr, nullptr);
      this->setp(nullptr, nullptr);
      return this;
    }

    this->_buffer = s;
    this->_length = n;

    this->_gbuffer = this->_buffer;
    this->_gsize   = n / 2;
    this->_pbuffer = this->_buffer + this->_gsize;
    this->_psize   = n - this->_gsize;

    // Empty get area, so the first read triggers underflow().
    this->setg(this->_gbuffer, this->_gbuffer, this->_gbuffer);
    this->setp(this->_pbuffer, this->_pbuffer + this->_psize);

    return this;
  }

  socketbuf::pos_type socketbuf::seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode)
  {
    return pos_type(off_type(-1));
  }

  socketbuf::pos_type socketbuf::seekpos(pos_type, std::ios_base::openmode)
  {
    return pos_type(off_type(-1));
  }

  bool socketbuf::flush_put_area()
  {
    if (this->pptr() == nullptr) return true;

    char_type* cursor = this->pbase();
    std::streamsize pending = this->pptr() - this->pbase();

    while (pending > 0) {
      int count = (int)Socket::send(this->_handle, cursor, (size_t)pending, 0);

      if (count == Socket::SOCKET_ERROR) {
	if (Socket::error_code() == EINTR) continue;

	if (Socket::error_code() == Socket::Error::WOULDBLOCK) {
	  // A non-blocking socket that is momentarily full: keep what is left
	  // at the front of the put area so a later flush can finish the job.
	  if (cursor != this->pbase())
	    std::memmove(this->_pbuffer, cursor, (size_t)pending);
	  this->setp(this->_pbuffer, this->_pbuffer + this->_psize);
	  this->pbump((int)pending);
	  return false;
	}

	this->setp(nullptr, nullptr);
	return false;
      }

      if (count == 0) {  // peer is gone
	this->setp(nullptr, nullptr);
	return false;
      }

      cursor += count;
      pending -= count;
    }

    this->setp(this->_pbuffer, this->_pbuffer + this->_psize);
    return true;
  }

  int socketbuf::sync()
  {
    // Only the put area is synchronised; pulling on the socket here would block
    // a writer waiting for a reply it has not sent the request for yet.
    return (this->flush_put_area()) ? 0 : -1;
  }

  std::streamsize socketbuf::showmanyc()
  {
    if (this->gptr() != nullptr && this->gptr() < this->egptr())
      return this->egptr() - this->gptr();

    return 0;  // unknown, but not necessarily end of stream
  }

  socketbuf::int_type socketbuf::underflow()
  {
    if (this->gptr() != nullptr && this->gptr() < this->egptr())
      return traits_type::to_int_type(*this->gptr());

    if (this->_gbuffer == nullptr) return traits_type::eof();

    // About to block waiting for the peer, so anything still queued for it has
    // to go out first. Without this a request/response protocol deadlocks with
    // the request sitting unsent in the put area.
    if (this->pptr() != nullptr && this->pptr() > this->pbase())
      if (!this->flush_put_area()) return traits_type::eof();

    int count = 0;
    do {
      count = (int)Socket::receive(this->_handle, this->_gbuffer, (size_t)this->_gsize, 0);
    } while (count == Socket::SOCKET_ERROR && Socket::error_code() == EINTR);

    if (count == Socket::SOCKET_ERROR) {
      if (Socket::error_code() != Socket::Error::WOULDBLOCK)
	this->setg(nullptr, nullptr, nullptr);
      return traits_type::eof();
    }

    if (count == 0) {  // orderly shutdown by the peer
      this->setg(this->_gbuffer, this->_gbuffer, this->_gbuffer);
      return traits_type::eof();
    }

    this->setg(this->_gbuffer, this->_gbuffer, this->_gbuffer + count);
    return traits_type::to_int_type(*this->gptr());
  }

  socketbuf::int_type socketbuf::overflow(int_type ch)
  {
    if (!this->flush_put_area()) return traits_type::eof();

    if (traits_type::eq_int_type(ch, traits_type::eof()))
      return traits_type::not_eof(ch);

    if (this->pptr() == nullptr || this->pptr() >= this->epptr())
      return traits_type::eof();

    *this->pptr() = traits_type::to_char_type(ch);
    this->pbump(1);

    return ch;
  }

  socketbuf::int_type socketbuf::pbackfail(int_type ch)
  {
    if (this->gptr() == nullptr || this->gptr() <= this->eback())
      return traits_type::eof();

    this->gbump(-1);

    if (!traits_type::eq_int_type(ch, traits_type::eof()))
      *this->gptr() = traits_type::to_char_type(ch);

    return traits_type::to_int_type(*this->gptr());
  }

  socketbuf::~socketbuf()
  {
    if (this->_buffer != nullptr) delete[] this->_buffer;
  }

}
