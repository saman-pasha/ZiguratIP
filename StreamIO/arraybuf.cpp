#include "arraybuf.h"
#include <cstring>
#include <limits>


namespace Zigurat
{

  // arraybuf is a fixed size view over an array the caller owns. The get and
  // the put area deliberately share the same memory with independent positions,
  // so a record can be written, rewound and read back without a copy. Nothing
  // is ever allocated or released here: the array outlives the buffer.

  arraybuf::arraybuf()
    : _buffer(nullptr), _length(0)
  {
    this->setg(nullptr, nullptr, nullptr);
    this->setp(nullptr, nullptr);
  }

  arraybuf::arraybuf(arraybuf&& other)
    : _buffer(other._buffer), _length(other._length)
  {
    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pbase(), other.epptr());
    this->pbump_off(other.pptr() - other.pbase());

    other._buffer = nullptr;
    other._length = 0;
    other.setg(nullptr, nullptr, nullptr);
    other.setp(nullptr, nullptr);
  }

  arraybuf& arraybuf::operator=(arraybuf&& other)
  {
    if (this == &other) return *this;

    this->_buffer = other._buffer;
    this->_length = other._length;

    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pbase(), other.epptr());
    this->pbump_off(other.pptr() - other.pbase());

    other._buffer = nullptr;
    other._length = 0;
    other.setg(nullptr, nullptr, nullptr);
    other.setp(nullptr, nullptr);

    return *this;
  }

  // pbump() only takes an int, which is not wide enough for every streamsize.
  void arraybuf::pbump_off(off_type off)
  {
    const off_type step_max = std::numeric_limits<int>::max();
    while (off > 0) {
      int step = (off > step_max) ? std::numeric_limits<int>::max() : (int)off;
      this->pbump(step);
      off -= step;
    }
  }

  arraybuf* arraybuf::setbuf(char_type* s, std::streamsize n)
  {
    if (s == nullptr || n < 0) {
      this->_buffer = nullptr;
      this->_length = 0;
      this->setg(nullptr, nullptr, nullptr);
      this->setp(nullptr, nullptr);
      return this;
    }

    this->_buffer = s;
    this->_length = n;

    this->setg(this->_buffer, this->_buffer, this->_buffer + this->_length);
    this->setp(this->_buffer, this->_buffer + this->_length);

    return this;
  }

  arraybuf::pos_type arraybuf::seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which)
  {
    const bool in  = (which & std::ios_base::in)  == std::ios_base::in;
    const bool out = (which & std::ios_base::out) == std::ios_base::out;

    if (this->_buffer == nullptr || (!in && !out)) return pos_type(off_type(-1));

    // With both areas selected the two positions must end up in step, which is
    // only well defined for an absolute seek.
    if (in && out && dir == std::ios_base::cur) return pos_type(off_type(-1));

    off_type base = 0;
    if (dir == std::ios_base::beg)
      base = 0;
    else if (dir == std::ios_base::end)
      base = this->_length;
    else if (dir == std::ios_base::cur)
      base = (in) ? (this->gptr() - this->eback()) : (this->pptr() - this->pbase());
    else
      return pos_type(off_type(-1));

    return this->seekpos(pos_type(base + off), which);
  }

  arraybuf::pos_type arraybuf::seekpos(pos_type pos, std::ios_base::openmode which)
  {
    const bool in  = (which & std::ios_base::in)  == std::ios_base::in;
    const bool out = (which & std::ios_base::out) == std::ios_base::out;

    const off_type off = off_type(pos);

    if (this->_buffer == nullptr || (!in && !out)) return pos_type(off_type(-1));
    if (off < 0 || off > this->_length) return pos_type(off_type(-1));

    if (in)
      this->setg(this->_buffer, this->_buffer + off, this->_buffer + this->_length);

    if (out) {
      this->setp(this->_buffer, this->_buffer + this->_length);
      this->pbump_off(off);
    }

    return pos;
  }

  int arraybuf::sync()
  {
    return 0;
  }

  std::streamsize arraybuf::showmanyc()
  {
    // Only reached with the get area drained, and a fixed array never refills.
    return -1;
  }

  arraybuf::int_type arraybuf::underflow()
  {
    if (this->gptr() != nullptr && this->gptr() < this->egptr())
      return traits_type::to_int_type(*this->gptr());

    return traits_type::eof();
  }

  arraybuf::int_type arraybuf::overflow(int_type ch)
  {
    // The array cannot grow, so a full put area is the end of the road.
    if (traits_type::eq_int_type(ch, traits_type::eof()))
      return traits_type::not_eof(ch);

    if (this->pptr() != nullptr && this->pptr() < this->epptr()) {
      *this->pptr() = traits_type::to_char_type(ch);
      this->pbump(1);
      return ch;
    }

    return traits_type::eof();
  }

  arraybuf::int_type arraybuf::pbackfail(int_type ch)
  {
    if (this->gptr() == nullptr || this->gptr() <= this->eback())
      return traits_type::eof();

    this->gbump(-1);

    if (!traits_type::eq_int_type(ch, traits_type::eof()))
      *this->gptr() = traits_type::to_char_type(ch);

    return traits_type::to_int_type(*this->gptr());
  }

  bool arraybuf::is_open() const
  {
    return this->_buffer != nullptr;
  }

  arraybuf* arraybuf::close()
  {
    // The array belongs to the caller; only the view is dropped.
    this->_buffer = nullptr;
    this->_length = 0;
    this->setg(nullptr, nullptr, nullptr);
    this->setp(nullptr, nullptr);
    return this;
  }

  arraybuf::~arraybuf()
  {

  }

}
